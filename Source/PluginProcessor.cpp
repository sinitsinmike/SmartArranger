#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

SmartArrangerAudioProcessor::SmartArrangerAudioProcessor()
    : juce::AudioProcessor (juce::AudioProcessor::BusesProperties()),
      parameters (*this, nullptr, "SMART_ARRANGER_STATE", createParameterLayout())
{
    accompanimentEnabledParameter = parameters.getRawParameterValue ("accompanimentEnabled");
}

juce::AudioProcessorValueTreeState::ParameterLayout
SmartArrangerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "accompanimentEnabled", 1 }, "Accompaniment", false));
    return layout;
}

void SmartArrangerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    playbackSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    heldNotes.fill (0);
    activeGeneratedNotes.fill (255);
    lastGeneratedStep = -1;
    detectedChordRoot.store (-1, std::memory_order_relaxed);
    detectedChordQuality.store (noChord, std::memory_order_relaxed);
    detectedBassNote.store (-1, std::memory_order_relaxed);
    activeStyleNotes.fill (0);
    activeStyleOutputNotes.fill (255);
    activeStyleOutputChannels.fill (255);
    styleHarmonyBar = -1;
    styleHarmonyRoot = 0;
    styleHarmonyQuality = major;
    styleClockPpq = 0.0;
    fallbackClockPpq = 0.0;
    styleSectionStartPpq = 0.0;
    lastStyleBlockEndPpq = 0.0;
    styleWasPlaying = false;
    styleBufferInUse = -1;
    activeStyleSection = -1;
    lastRequestedStyleSection = -1;
    queuedStyleSection = -1;
    queuedStyleSwitchPpq = -1.0;
    mainStyleSection = -1;
    oneShotReturnSection = -1;
    stopAfterOneShot = false;
    stylePlaybackFinished.store (false, std::memory_order_relaxed);
}

void SmartArrangerAudioProcessor::releaseResources()
{
    heldNotes.fill (0);
    activeGeneratedNotes.fill (255);
    lastGeneratedStep = -1;
    detectedChordRoot.store (-1, std::memory_order_relaxed);
    detectedChordQuality.store (noChord, std::memory_order_relaxed);
    detectedBassNote.store (-1, std::memory_order_relaxed);
    activeStyleNotes.fill (0);
    activeStyleOutputNotes.fill (255);
    activeStyleOutputChannels.fill (255);
    styleHarmonyBar = -1;
    styleHarmonyRoot = 0;
    styleHarmonyQuality = major;
    styleClockPpq = 0.0;
    fallbackClockPpq = 0.0;
    styleSectionStartPpq = 0.0;
    lastStyleBlockEndPpq = 0.0;
    styleWasPlaying = false;
    styleBufferInUse = -1;
    activeStyleSection = -1;
    lastRequestedStyleSection = -1;
    queuedStyleSection = -1;
    queuedStyleSwitchPpq = -1.0;
    mainStyleSection = -1;
    oneShotReturnSection = -1;
    stopAfterOneShot = false;
    stylePlaybackFinished.store (false, std::memory_order_relaxed);
}

namespace
{
enum StyleSectionRole
{
    otherSection = 0,
    introSection,
    mainSection,
    fillSection,
    breakSection,
    endingSection
};

int getStyleSectionRole (const juce::String& name)
{
    const auto folded = name.toLowerCase();
    if (folded.contains ("intro") || folded.startsWith ("sint")) return introSection;
    if (folded.contains ("main") || folded.startsWith ("sma")) return mainSection;
    if (folded.contains ("fill") || folded.startsWith ("sfi")) return fillSection;
    if (folded.contains ("break") || folded.startsWith ("sbr")) return breakSection;
    if (folded.contains ("ending") || folded.contains ("outro") || folded.startsWith ("send"))
        return endingSection;
    return otherSection;
}
}

bool SmartArrangerAudioProcessor::setLoadedStyle (const SmartArrangerStyleInfo& style,
                                                   int sectionIndex,
                                                   juce::String& error,
                                                   const juce::File& sourceFile)
{
    error.clear();
    if (style.ticksPerQuarter <= 0 || sectionIndex < 0
        || sectionIndex >= static_cast<int> (style.sections.size()))
    {
        error = "У выбранной секции нет корректной временной сетки.";
        return false;
    }

    StylePlaybackData next;
    next.ticksPerQuarter = style.ticksPerQuarter;
    next.tempo = juce::jlimit (20, 400, style.tempo);
    next.numerator = juce::jlimit (1, 32, style.numerator);
    next.denominator = style.denominator > 0 ? style.denominator : 4;
    next.casmRecords = style.casmRecords;
    next.casmSegments = style.casmSegments;
    next.sections.resize (style.sections.size());
    for (size_t sectionIndexToBuild = 0; sectionIndexToBuild < style.sections.size(); ++sectionIndexToBuild)
    {
        const auto& sourceSection = style.sections[sectionIndexToBuild];
        auto& destinationSection = next.sections[sectionIndexToBuild];
        destinationSection.lengthTicks = sourceSection.lengthTicks;
        if (destinationSection.lengthTicks == 0 && style.totalTicks > sourceSection.startTick)
            destinationSection.lengthTicks = style.totalTicks - sourceSection.startTick;
        destinationSection.role = getStyleSectionRole (sourceSection.name);
        if (destinationSection.lengthTicks == 0)
            continue;

        // Yamaha styles commonly put channel setup (Bank Select and Program
        // Change) in SInt, rather than before the first section marker. A user
        // may start any section directly, so carry the latest setup state from
        // all earlier style events into this section. Events at the section's
        // own start remain in its event list below. Keep Bank Select before
        // Program Change, as required by MIDI.
        struct ChannelSetup
        {
            SmartArrangerStyleEvent bankMsb, bankLsb, program;
            bool hasBankMsb = false, hasBankLsb = false, hasProgram = false;
        };
        std::array<ChannelSetup, 16> channelSetup {};
        for (const auto& event : style.events)
        {
            if (event.tick >= sourceSection.startTick || event.status >= 0xf0u)
                continue;

            const int sourceChannel = event.status & 0x0fu;
            const auto command = event.status & 0xf0u;
            auto& setup = channelSetup[(size_t) sourceChannel];
            if (command == 0xc0u)
            {
                setup.program = event;
                setup.hasProgram = true;
            }
            else if (command == 0xb0u && event.data1 == 0)
            {
                setup.bankMsb = event;
                setup.hasBankMsb = true;
            }
            else if (command == 0xb0u && event.data1 == 32)
            {
                setup.bankLsb = event;
                setup.hasBankLsb = true;
            }
        }
        for (size_t sourceChannel = 0; sourceChannel < channelSetup.size(); ++sourceChannel)
        {
            const auto appendSetup = [&] (SmartArrangerStyleEvent event)
            {
                event.tick = 0;
                event.sectionIndex = static_cast<int> (sectionIndexToBuild);
                destinationSection.events.push_back (event);
            };
            const auto& setup = channelSetup[sourceChannel];
            if (setup.hasBankMsb) appendSetup (setup.bankMsb);
            if (setup.hasBankLsb) appendSetup (setup.bankLsb);
            if (setup.hasProgram) appendSetup (setup.program);
        }

        const auto sectionEnd = sourceSection.startTick + destinationSection.lengthTicks;
        for (const auto& event : style.events)
        {
            const bool fallbackTrack = sourceSection.name == "Track" && event.sectionIndex < 0;
            if (! fallbackTrack && event.sectionIndex != static_cast<int> (sectionIndexToBuild))
                continue;
            if (event.tick < sourceSection.startTick || event.tick >= sectionEnd)
                continue;
            auto normalized = event;
            normalized.tick -= sourceSection.startTick;
            destinationSection.events.push_back (normalized);
        }

        std::stable_sort (destinationSection.events.begin(), destinationSection.events.end(),
            [] (const SmartArrangerStyleEvent& a, const SmartArrangerStyleEvent& b)
            {
                return a.tick < b.tick;
            });

        // Build a lossless per-section view of source channels and their
        // logical Yamaha destinations. A shared CSEG may declare rules for
        // channels that have no notes in this particular section, so retain
        // those rules too and mark actual MIDI activity separately.
        const auto findPart = [&] (uint8_t sourceChannel, uint8_t destinationChannel)
            -> StylePlaybackData::SourcePart*
        {
            const auto found = std::find_if (destinationSection.sourceParts.begin(),
                                             destinationSection.sourceParts.end(),
                [sourceChannel, destinationChannel] (const auto& part)
                {
                    return part.sourceChannel == sourceChannel
                        && part.destinationChannel == destinationChannel;
                });
            return found == destinationSection.sourceParts.end() ? nullptr : &*found;
        };

        std::array<bool, 16> sourceChannelsWithEvents {};
        for (const auto& event : destinationSection.events)
        {
            if ((event.status & 0xf0u) >= 0xf0u)
                continue;

            const auto sourceChannel = static_cast<uint8_t> (event.status & 0x0fu);
            sourceChannelsWithEvents[sourceChannel] = true;
        }

        const auto foldedSectionName = sourceSection.name.trim().toLowerCase();
        for (const auto& segment : style.casmSegments)
        {
            const bool appliesToSection = std::any_of (
                segment.sectionNames.begin(), segment.sectionNames.end(),
                [&foldedSectionName] (const juce::String& declaredName)
                {
                    return declaredName.trim().toLowerCase() == foldedSectionName;
                });
            if (! appliesToSection)
                continue;

            for (const auto& record : segment.records)
            {
                if (! record.hasCommonChannelFields)
                    continue;

                // CASM CTAB/CTB2 source channels use the same zero-based
                // 0..15 numbering as the MIDI status nibble and SourcePart.
                const auto sourceChannel = record.sourceChannel;
                auto* part = findPart (sourceChannel, record.destinationChannel);
                if (part == nullptr)
                {
                    destinationSection.sourceParts.push_back ({ sourceChannel,
                        record.destinationChannel, record.partLabel,
                        sourceChannel < sourceChannelsWithEvents.size()
                            && sourceChannelsWithEvents[sourceChannel], { record } });
                    continue;
                }

                part->destinationChannel = record.destinationChannel;
                if (part->partLabel.isEmpty())
                    part->partLabel = record.partLabel;
                part->rules.push_back (record);
            }
        }

        for (size_t sourceChannel = 0; sourceChannel < sourceChannelsWithEvents.size(); ++sourceChannel)
        {
            if (! sourceChannelsWithEvents[sourceChannel])
                continue;

            const bool hasCasmMapping = std::any_of (
                destinationSection.sourceParts.begin(), destinationSection.sourceParts.end(),
                [sourceChannel] (const auto& part)
                {
                    return part.sourceChannel == sourceChannel && part.destinationChannel != 0xff;
                });
            if (! hasCasmMapping)
                destinationSection.sourceParts.push_back ({ static_cast<uint8_t> (sourceChannel),
                    0xff, {}, true, {} });
        }
    }

    const auto& selectedSection = next.sections[static_cast<size_t> (sectionIndex)];
    if (selectedSection.lengthTicks == 0 || selectedSection.events.empty())
    {
        error = "В выбранной секции не найдено MIDI-событий.";
        return false;
    }

    const int current = activeStyleBuffer.load (std::memory_order_acquire);
    const int target = current == 0 ? 1 : 0;
    while (styleReaders[static_cast<size_t> (target)].load (std::memory_order_acquire) != 0)
        juce::Thread::sleep (1);

    stylePlaybackBuffers[static_cast<size_t> (target)] = std::move (next);
    activeStyleBuffer.store (target, std::memory_order_release);
    requestedStyleSection.store (sectionIndex, std::memory_order_release);
    stylePlaybackFinished.store (false, std::memory_order_release);
    parameters.state.setProperty ("stylePath", sourceFile.getFullPathName(), nullptr);
    parameters.state.setProperty ("styleSection", sectionIndex, nullptr);
    return true;
}

void SmartArrangerAudioProcessor::requestStyleSection (int sectionIndex)
{
    if (sectionIndex < 0) return;

    requestedStyleSection.store (sectionIndex, std::memory_order_release);
    parameters.state.setProperty ("styleSection", sectionIndex, nullptr);
}

juce::String SmartArrangerAudioProcessor::getStoredStylePath() const
{
    return parameters.state.getProperty ("stylePath").toString();
}

int SmartArrangerAudioProcessor::getStoredStyleSection() const noexcept
{
    return static_cast<int> (parameters.state.getProperty ("styleSection", 0));
}

int SmartArrangerAudioProcessor::getRequestedStyleSection() const noexcept
{
    return requestedStyleSection.load (std::memory_order_acquire);
}

void SmartArrangerAudioProcessor::clearLoadedStyle (bool clearSavedSelection) noexcept
{
    activeStyleBuffer.store (-1, std::memory_order_release);
    requestedStyleSection.store (0, std::memory_order_release);
    stylePlaybackFinished.store (false, std::memory_order_release);
    if (clearSavedSelection)
    {
        parameters.state.removeProperty ("stylePath", nullptr);
        parameters.state.removeProperty ("styleSection", nullptr);
    }
}

bool SmartArrangerAudioProcessor::hasStylePlaybackFinished() const noexcept
{
    return stylePlaybackFinished.load (std::memory_order_acquire);
}

uint64_t SmartArrangerAudioProcessor::getStyleNoteOnCount() const noexcept
{
    return styleNoteOnCount.load (std::memory_order_relaxed);
}

uint64_t SmartArrangerAudioProcessor::getStyleMaskRejectedCount() const noexcept
{
    return styleMaskRejectedCount.load (std::memory_order_relaxed);
}

uint64_t SmartArrangerAudioProcessor::getStyleScannedEventCount() const noexcept
{
    return styleScannedEventCount.load (std::memory_order_relaxed);
}

uint64_t SmartArrangerAudioProcessor::getStyleRendererCallCount() const noexcept
{
    return styleRendererCallCount.load (std::memory_order_relaxed);
}

void SmartArrangerAudioProcessor::processBlock (juce::AudioBuffer<float>& audio,
                                                 juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    audio.clear();

    const int splitPoint = keyboardSplitPoint.load (std::memory_order_relaxed);

    // Observe chord-zone notes for harmony recognition. In the standalone,
    // upper-zone notes are melody and must not alter the current chord.
    for (const auto metadata : midi)
    {
        const auto& message = metadata.getMessage();
        if (! message.isNoteOn() && ! message.isNoteOff())
            continue;

        const int note = message.getNoteNumber();
        const int channel = message.getChannel() - 1;
        if (note < 0 || note >= 128 || channel < 0 || channel >= 16)
            continue;
        if (splitPoint >= 0 && note > splitPoint)
            continue;

        heldNotes[static_cast<size_t> (channel * 128 + note)] = message.isNoteOn() ? 1 : 0;
        updateDetectedChord();
    }

    if (splitPoint >= 0)
    {
        // Low-zone notes are chord controls only. Upper-zone note events
        // always pass to the selected MIDI output/SF2 on their original
        // channels; MIDI Thru gates the remaining raw controller messages.
        juce::MidiBuffer filtered;
        const bool passOtherMessages = midiThruEnabled.load (std::memory_order_relaxed);
        for (const auto metadata : midi)
        {
            const auto& message = metadata.getMessage();
            if (message.isNoteOnOrOff())
            {
                if (message.getNoteNumber() > splitPoint)
                    filtered.addEvent (message, metadata.samplePosition);
            }
            else if (passOtherMessages)
                filtered.addEvent (message, metadata.samplePosition);
        }
        midi.swapWith (filtered);
    }
    // The AU keeps its existing MIDI-thru behavior when split mode is off.
    else if (! midiThruEnabled.load (std::memory_order_relaxed))
        midi.clear();

    renderAccompaniment (midi, audio.getNumSamples());
}

namespace
{
struct ChordPattern
{
    SmartArrangerAudioProcessor::ChordQuality quality;
    uint16_t intervals;
};

constexpr ChordPattern chordPatterns[] =
{
    { SmartArrangerAudioProcessor::major,                 (1u << 0) | (1u << 4) | (1u << 7) },
    { SmartArrangerAudioProcessor::minor,                 (1u << 0) | (1u << 3) | (1u << 7) },
    { SmartArrangerAudioProcessor::diminished,            (1u << 0) | (1u << 3) | (1u << 6) },
    { SmartArrangerAudioProcessor::augmented,             (1u << 0) | (1u << 4) | (1u << 8) },
    { SmartArrangerAudioProcessor::suspendedSecond,       (1u << 0) | (1u << 2) | (1u << 7) },
    { SmartArrangerAudioProcessor::suspendedFourth,       (1u << 0) | (1u << 5) | (1u << 7) },
    { SmartArrangerAudioProcessor::dominantSeventh,       (1u << 0) | (1u << 4) | (1u << 7) | (1u << 10) },
    { SmartArrangerAudioProcessor::majorSeventh,          (1u << 0) | (1u << 4) | (1u << 7) | (1u << 11) },
    { SmartArrangerAudioProcessor::minorSeventh,          (1u << 0) | (1u << 3) | (1u << 7) | (1u << 10) },
    { SmartArrangerAudioProcessor::halfDiminishedSeventh, (1u << 0) | (1u << 3) | (1u << 6) | (1u << 10) },
    { SmartArrangerAudioProcessor::diminishedSeventh,     (1u << 0) | (1u << 3) | (1u << 6) | (1u << 9) }
};

int countBits (uint16_t bits) noexcept
{
    int count = 0;
    while (bits != 0)
    {
        count += bits & 1u;
        bits >>= 1;
    }
    return count;
}

int transformStyleNote (int note, int root, int quality) noexcept
{
    // Yamaha style MIDI parts are commonly authored around C major. This
    // lightweight mapping follows the chord root and triad quality; it does
    // not replace the per-part rules stored in a Yamaha CASM block.
    int third = 4;
    int fifth = 7;
    switch (quality)
    {
        case SmartArrangerAudioProcessor::minor:
        case SmartArrangerAudioProcessor::minorSeventh:
        case SmartArrangerAudioProcessor::halfDiminishedSeventh:
        case SmartArrangerAudioProcessor::diminishedSeventh:
        case SmartArrangerAudioProcessor::diminished:
            third = 3;
            break;
        case SmartArrangerAudioProcessor::suspendedSecond: third = 2; break;
        case SmartArrangerAudioProcessor::suspendedFourth: third = 5; break;
        default: break;
    }
    if (quality == SmartArrangerAudioProcessor::diminished
        || quality == SmartArrangerAudioProcessor::halfDiminishedSeventh
        || quality == SmartArrangerAudioProcessor::diminishedSeventh)
        fifth = 6;
    else if (quality == SmartArrangerAudioProcessor::augmented)
        fifth = 8;

    const int sourcePitchClass = note % 12;
    const int rootShift = root > 6 ? root - 12 : root;
    int result = note + rootShift;
    if (sourcePitchClass == 4)
        result += third - 4;
    else if (sourcePitchClass == 7)
        result += fifth - 7;
    return juce::jlimit (0, 127, result);
}

uint16_t sourceChordIntervals (int sourceChord) noexcept
{
    // Source chord values follow the Yamaha CASM enum documented in the
    // reverse-engineered Style Files v2.1 reference. Keep this table limited
    // to chord types whose core notes we can identify without guessing.
    switch (sourceChord)
    {
        case 0x00: return (1u << 0) | (1u << 4) | (1u << 7);             // Maj
        case 0x01: return (1u << 0) | (1u << 4) | (1u << 7) | (1u << 9); // Maj6
        case 0x02: return (1u << 0) | (1u << 4) | (1u << 7) | (1u << 11);// Maj7
        case 0x07: return (1u << 0) | (1u << 4) | (1u << 8);             // aug
        case 0x08: return (1u << 0) | (1u << 3) | (1u << 7);             // min
        case 0x09: return (1u << 0) | (1u << 3) | (1u << 7) | (1u << 9); // min6
        case 0x0a: return (1u << 0) | (1u << 3) | (1u << 7) | (1u << 10);// min7
        case 0x0b: return (1u << 0) | (1u << 3) | (1u << 6) | (1u << 10);// min7b5
        case 0x11: return (1u << 0) | (1u << 3) | (1u << 6);             // dim
        case 0x12: return (1u << 0) | (1u << 3) | (1u << 6) | (1u << 9); // dim7
        case 0x13: return (1u << 0) | (1u << 4) | (1u << 7) | (1u << 10);// 7th
        case 0x14: return (1u << 0) | (1u << 5) | (1u << 7);             // 7sus4
        case 0x15: return (1u << 0) | (1u << 4) | (1u << 6) | (1u << 10);// 7b5
        case 0x20: return (1u << 0) | (1u << 5) | (1u << 7);             // sus4
        case 0x21: return (1u << 0) | (1u << 2) | (1u << 7);             // 1+2+5
        default: return 0;
    }
}

uint16_t detectedChordIntervals (int quality) noexcept
{
    switch (quality)
    {
        case SmartArrangerAudioProcessor::major:
            return (1u << 0) | (1u << 4) | (1u << 7);
        case SmartArrangerAudioProcessor::minor:
            return (1u << 0) | (1u << 3) | (1u << 7);
        case SmartArrangerAudioProcessor::diminished:
            return (1u << 0) | (1u << 3) | (1u << 6);
        case SmartArrangerAudioProcessor::augmented:
            return (1u << 0) | (1u << 4) | (1u << 8);
        case SmartArrangerAudioProcessor::suspendedSecond:
            return (1u << 0) | (1u << 2) | (1u << 7);
        case SmartArrangerAudioProcessor::suspendedFourth:
            return (1u << 0) | (1u << 5) | (1u << 7);
        case SmartArrangerAudioProcessor::dominantSeventh:
            return (1u << 0) | (1u << 4) | (1u << 7) | (1u << 10);
        case SmartArrangerAudioProcessor::majorSeventh:
            return (1u << 0) | (1u << 4) | (1u << 7) | (1u << 11);
        case SmartArrangerAudioProcessor::minorSeventh:
            return (1u << 0) | (1u << 3) | (1u << 7) | (1u << 10);
        case SmartArrangerAudioProcessor::halfDiminishedSeventh:
            return (1u << 0) | (1u << 3) | (1u << 6) | (1u << 10);
        case SmartArrangerAudioProcessor::diminishedSeventh:
            return (1u << 0) | (1u << 3) | (1u << 6) | (1u << 9);
        default: return 0;
    }
}

int transformRootTransHarmonicMinor5th (int note, int sourceRoot, int sourceChord,
                                        int targetRoot, int targetQuality) noexcept
{
    if (note < 0 || note > 127 || sourceRoot < 0 || sourceRoot > 11
        || targetRoot < 0 || targetRoot > 11)
        return note;

    const int sourceInterval = (note - sourceRoot + 12) % 12;
    const bool sourceHasMajorThird = (sourceChordIntervals (sourceChord) & (1u << 4)) != 0;
    const bool sourceHasMinorThird = (sourceChordIntervals (sourceChord) & (1u << 3)) != 0;
    const bool targetIsMinor = targetQuality == SmartArrangerAudioProcessor::minor
        || targetQuality == SmartArrangerAudioProcessor::minorSeventh
        || targetQuality == SmartArrangerAudioProcessor::halfDiminishedSeventh
        || targetQuality == SmartArrangerAudioProcessor::diminished
        || targetQuality == SmartArrangerAudioProcessor::diminishedSeventh;
    const bool targetIsMajor = targetQuality == SmartArrangerAudioProcessor::major
        || targetQuality == SmartArrangerAudioProcessor::majorSeventh
        || targetQuality == SmartArrangerAudioProcessor::dominantSeventh;

    int intervalAdjustment = 0;
    if (sourceHasMajorThird && targetIsMinor && sourceInterval == 4)
        intervalAdjustment = -1;
    else if (sourceHasMinorThird && targetIsMajor && sourceInterval == 3)
        intervalAdjustment = 1;
    else if (sourceHasMajorThird && targetIsMinor && sourceInterval == 9)
        intervalAdjustment = -1;
    else if (sourceHasMinorThird && targetIsMajor && sourceInterval == 8)
        intervalAdjustment = 1;

    // The "5th" variant extends the harmonic-minor substitution to the fifth
    // for augmented/diminished target chords. Preserve other authored/passing
    // tones; only alter a source perfect fifth when the detected chord calls
    // for a changed fifth.
    if (sourceInterval == 7)
    {
        if (targetQuality == SmartArrangerAudioProcessor::diminished
            || targetQuality == SmartArrangerAudioProcessor::halfDiminishedSeventh
            || targetQuality == SmartArrangerAudioProcessor::diminishedSeventh)
            intervalAdjustment = -1;
        else if (targetQuality == SmartArrangerAudioProcessor::augmented)
            intervalAdjustment = 1;
    }

    int rootShift = targetRoot - sourceRoot;
    if (rootShift > 6)
        rootShift -= 12;
    else if (rootShift < -6)
        rootShift += 12;
    return juce::jlimit (0, 127, note + rootShift + intervalAdjustment);
}

int transformRootFixedChordNote (int note, int sourceRoot, int sourceChord,
                                 int targetRoot, int targetQuality,
                                 int lowLimit, int highLimit) noexcept
{
    if (note < 0 || note > 127 || sourceRoot < 0 || sourceRoot > 11
        || targetRoot < 0 || targetRoot > 11)
        return note;

    const auto sourceIntervals = sourceChordIntervals (sourceChord);
    const auto targetIntervals = detectedChordIntervals (targetQuality);
    if (sourceIntervals == 0 || targetIntervals == 0)
        return note;

    const int sourceInterval = (note - sourceRoot + 12) % 12;
    if ((sourceIntervals & (1u << sourceInterval)) == 0)
        return note; // retain non-chord/passing notes until their NTT is modeled

    lowLimit = juce::jlimit (0, 127, lowLimit);
    highLimit = juce::jlimit (lowLimit, 127, highLimit);
    int bestNote = note;
    int bestDistance = 128;
    for (int candidate = lowLimit; candidate <= highLimit; ++candidate)
    {
        const int interval = (candidate - targetRoot + 12) % 12;
        if ((targetIntervals & (1u << interval)) == 0)
            continue;

        const int distance = std::abs (candidate - note);
        // On an equal-distance choice, prefer the higher pitch, matching the
        // documented C3-E3-G3 -> C3-F3-A3 Root Fixed example.
        if (distance < bestDistance || (distance == bestDistance && candidate > bestNote))
        {
            bestDistance = distance;
            bestNote = candidate;
        }
    }
    return bestNote;
}

template <typename Section>
int getCasmOutputChannel (const Section& section, int sourceChannel) noexcept
{
    int mappedDestination = -1;
    bool sourceHasMapping = false;
    for (const auto& part : section.sourceParts)
    {
        if (part.sourceChannel != sourceChannel || part.destinationChannel == 0xff)
            continue;

        sourceHasMapping = true;
        if (part.destinationChannel > 15)
            return sourceChannel;
        if (mappedDestination >= 0 && mappedDestination != part.destinationChannel)
            return sourceChannel; // ambiguous source routing: retain original MIDI channel
        mappedDestination = part.destinationChannel;
    }

    return sourceHasMapping && mappedDestination >= 0 ? mappedDestination : sourceChannel;
}

template <typename Section>
bool isCasmSourceEligibleForChord (int quality, const Section& section,
                                   int sourceChannel) noexcept
{
    // The five chord-mask bytes follow the Wierzba/Bedesem bit table. Only
    // chord classes recognized by this prototype are mapped here. Unknown or
    // conflicting data is permissive so a partial decoder cannot mute a part.
    int maskByte = -1;
    int maskBit = -1;
    switch (quality)
    {
        case SmartArrangerAudioProcessor::suspendedFourth:       maskByte = 0; maskBit = 0; break;
        case SmartArrangerAudioProcessor::suspendedSecond:       maskByte = 0; maskBit = 1; break;
        case SmartArrangerAudioProcessor::dominantSeventh:       maskByte = 2; maskBit = 3; break;
        case SmartArrangerAudioProcessor::majorSeventh:          maskByte = 4; maskBit = 2; break;
        case SmartArrangerAudioProcessor::minorSeventh:          maskByte = 3; maskBit = 2; break;
        case SmartArrangerAudioProcessor::halfDiminishedSeventh: maskByte = 2; maskBit = 5; break;
        case SmartArrangerAudioProcessor::diminishedSeventh:     maskByte = 2; maskBit = 2; break;
        case SmartArrangerAudioProcessor::minor:                 maskByte = 3; maskBit = 0; break;
        case SmartArrangerAudioProcessor::diminished:            maskByte = 2; maskBit = 1; break;
        case SmartArrangerAudioProcessor::augmented:             maskByte = 4; maskBit = 7; break;
        case SmartArrangerAudioProcessor::major:                 maskByte = 4; maskBit = 0; break;
        default: return true;
    }

    int matchedPartIndex = -1;
    bool sourceIsMapped = false;
    for (size_t index = 0; index < section.sourceParts.size(); ++index)
    {
        const auto& part = section.sourceParts[index];
        if (part.sourceChannel != sourceChannel || part.destinationChannel == 0xff)
            continue;
        sourceIsMapped = true;
        if (matchedPartIndex >= 0)
            return true;
        matchedPartIndex = static_cast<int> (index);
    }

    if (! sourceIsMapped || matchedPartIndex < 0)
        return true;

    const auto& matchedPart = section.sourceParts[static_cast<size_t> (matchedPartIndex)];
    if (matchedPart.rules.empty())
        return true;

    const auto& expectedMask = matchedPart.rules.front().chordMask;
    for (const auto& rule : matchedPart.rules)
        if (rule.chordMask != expectedMask)
            return true;

    return (expectedMask[static_cast<size_t> (maskByte)] & (1u << maskBit)) != 0;
}

template <typename Section>
int transformStyleNoteForCasm (int note, int root, int quality,
                               const Section& section,
                               int sourceChannel) noexcept
{
    // Apply bounded CASM cases: SFF1/SFF2 Bypass with Root Trans/Fixed, plus
    // the supported SFF2 Root Fixed + Chord note mapping. Other combinations
    // remain unchanged rather than being interpreted speculatively.
    // Derive the nested private type from the template argument instead of
    // naming it outside the processor class.
    decltype (section.sourceParts.data()) matchedPart = nullptr;
    bool sourceIsMapped = false;
    for (const auto& part : section.sourceParts)
    {
        if (part.sourceChannel != sourceChannel || part.destinationChannel == 0xff)
            continue;

        sourceIsMapped = true;
        if (matchedPart != nullptr)
            return note; // one MIDI source mapped to multiple destinations: ambiguous for now
        matchedPart = &part;
    }

    if (matchedPart == nullptr)
        return sourceIsMapped ? note : transformStyleNote (note, root, quality);

    const SmartArrangerCasmRecord* sff2Rule = nullptr;
    for (const auto& rule : matchedPart->rules)
    {
        if (! rule.hasSff2Zones)
            continue;

        if (sff2Rule != nullptr && sff2Rule->payload != rule.payload)
            return note; // conflicting SFF2 records are not resolved by guessing
        sff2Rule = &rule;
    }

    if (sff2Rule != nullptr)
    {
        // Select the authored Low/Main/High zone from the source MIDI note.
        // The middle bounds are inclusive; 0/127 disable the outer zones.
        size_t zoneIndex = 1;
        if (sff2Rule->ctb2MiddleLowNote > 0
            && note < sff2Rule->ctb2MiddleLowNote)
            zoneIndex = 0;
        else if (sff2Rule->ctb2MiddleHighNote < 127
                 && note > sff2Rule->ctb2MiddleHighNote)
            zoneIndex = 2;

        const auto& zone = sff2Rule->zones[zoneIndex];
        const int ntt = zone.nttAndBassOn & 0x7f;
        const bool bassOn = (zone.nttAndBassOn & 0x80) != 0;
        if (root < 0 || root >= 12 || sff2Rule->sourceRoot >= 12)
            return note;

        if (zone.ntr == 1 && ntt == 0) // Root Fixed + Bypass keeps authored pitch
            return note;
        if (zone.ntr == 1 && ntt == 2)
            return transformRootFixedChordNote (note, sff2Rule->sourceRoot,
                sff2Rule->sourceChord, root, quality, zone.noteLowLimit,
                zone.noteHighLimit);
        // Root Trans + Melody NTT must follow a changed chord root whether
        // Bass-On is enabled or not. Bridge Over Troubled Water / Main D uses
        // this exact combination on its PhrsMain part (MIDI output 15), with
        // Bass-On off. Requiring Bass-On here left that phrase locked to its
        // authored root. Preserve the source interval and shift the pattern;
        // chord-type-specific Melody substitutions remain a separate NTT.
        const bool rootTransposingMelody = zone.ntr == 0 && ntt == 1;
        if (zone.ntr == 0 && ntt == 7 && ! bassOn)
            return transformRootTransHarmonicMinor5th (note, sff2Rule->sourceRoot,
                sff2Rule->sourceChord, root, quality);
        if (ntt != 0 && ! rootTransposingMelody)
            return note; // Other Melody, Guitar, and non-implemented NTTs stay untouched
        if (zone.ntr != 0) // Root Fixed/Guitar and unknown NTR values remain untouched
            return note;

        const int semitoneShift = (root - static_cast<int> (sff2Rule->sourceRoot) + 12) % 12;
        return juce::jlimit (0, 127, note + semitoneShift);
    }

    const SmartArrangerCasmRecord* supportedRule = nullptr;
    for (const auto& rule : matchedPart->rules)
    {
        if (! rule.hasSff1TranspositionFields)
            continue;

        if (supportedRule != nullptr
            && (supportedRule->sff1Ntr != rule.sff1Ntr
                || supportedRule->sff1Ntt != rule.sff1Ntt
                || supportedRule->sourceRoot != rule.sourceRoot))
            return note; // conflicting rules must not be resolved by guesswork
        supportedRule = &rule;
    }

    if (supportedRule == nullptr || root < 0 || root >= 12
        || supportedRule->sourceRoot >= 12 || supportedRule->sff1Ntt != 0)
        return note;

    if (supportedRule->sff1Ntr == 1) // Root Fixed + NTT Bypass: retain source pitches
        return note;

    if (supportedRule->sff1Ntr != 0) // Guitar and unknown NTR values are not implemented
        return note;

    const int semitoneShift = (root - static_cast<int> (supportedRule->sourceRoot) + 12) % 12;
    return juce::jlimit (0, 127, note + semitoneShift);
}
}

void SmartArrangerAudioProcessor::updateDetectedChord() noexcept
{
    uint16_t pitchClasses = 0;
    int lowestNote = 128;
    for (int channel = 0; channel < 16; ++channel)
    {
        for (int note = 0; note < 128; ++note)
        {
            if (heldNotes[static_cast<size_t> (channel * 128 + note)] == 0)
                continue;

            pitchClasses |= static_cast<uint16_t> (1u << (note % 12));
            lowestNote = juce::jmin (lowestNote, note);
        }
    }

    // Yamaha exposes chord detection and bass-note detection as separate
    // inputs. We do not yet offer channel masks, but preserving this value
    // separately prevents an inversion from being mistaken for a new root.
    detectedBassNote.store (lowestNote < 128 ? lowestNote : -1, std::memory_order_relaxed);

    if (countBits (pitchClasses) < 3)
    {
        // Hold the latest recognized chord after its input notes are released.
        // This is how a style arranger sustains the harmony between changes.
        return;
    }

    int bestScore = 1000;
    int bestTieBreak = 1000;
    int bestRoot = -1;
    ChordQuality bestQuality = noChord;
    const int previousRoot = detectedChordRoot.load (std::memory_order_relaxed);
    const int previousQuality = detectedChordQuality.load (std::memory_order_relaxed);

    for (int root = 0; root < 12; ++root)
    {
        for (const auto& pattern : chordPatterns)
        {
            uint16_t transposed = 0;
            for (int interval = 0; interval < 12; ++interval)
                if ((pattern.intervals & (1u << interval)) != 0)
                    transposed |= static_cast<uint16_t> (1u << ((root + interval) % 12));

            const int missing = countBits (static_cast<uint16_t> (transposed & ~pitchClasses));
            const int extra = countBits (static_cast<uint16_t> (pitchClasses & ~transposed));
            const int matched = countBits (static_cast<uint16_t> (transposed & pitchClasses));
            if (matched < 3)
                continue;

            const int score = missing * 4 + extra * 2;
            // When multiple incomplete voicings fit equally well, keep the
            // current harmony to avoid root flapping while notes are changing.
            const int tieBreak = root == previousRoot && pattern.quality == previousQuality ? 0 : 1;
            if (score < bestScore || (score == bestScore && tieBreak < bestTieBreak))
            {
                bestScore = score;
                bestTieBreak = tieBreak;
                bestRoot = root;
                bestQuality = pattern.quality;
            }
        }
    }

    if (bestScore >= 1000)
    {
        detectedChordRoot.store (-1, std::memory_order_relaxed);
        detectedChordQuality.store (noChord, std::memory_order_relaxed);
        return;
    }

    detectedChordRoot.store (bestRoot, std::memory_order_relaxed);
    detectedChordQuality.store (bestQuality, std::memory_order_relaxed);
}

void SmartArrangerAudioProcessor::renderAccompaniment (juce::MidiBuffer& midi,
                                                        int blockSamples) noexcept
{
    const auto sendNoteOffs = [&] (int samplePosition)
    {
        for (auto& note : activeGeneratedNotes)
        {
            if (note != 255)
            {
                midi.addEvent (juce::MidiMessage::noteOff (1, note), samplePosition);
                note = 255;
            }
        }
    };

    if (activeStyleBuffer.load (std::memory_order_acquire) < 0 && styleWasPlaying)
    {
        for (int channel = 0; channel < 16; ++channel)
            for (int note = 0; note < 128; ++note)
            {
                const auto index = static_cast<size_t> (channel * 128 + note);
                if (activeStyleNotes[index] != 0)
                {
                    const int outputNote = activeStyleOutputNotes[index] == 255
                                         ? note : activeStyleOutputNotes[index];
                    const int outputChannel = activeStyleOutputChannels[index] == 255
                                            ? channel + 1 : activeStyleOutputChannels[index];
                    midi.addEvent (juce::MidiMessage::noteOff (outputChannel, outputNote), 0);
                    activeStyleNotes[index] = 0;
                    activeStyleOutputNotes[index] = 255;
                    activeStyleOutputChannels[index] = 255;
                }
            }
        styleWasPlaying = false;
        styleBufferInUse = -1;
        styleHarmonyBar = -1;
    }

    if (renderLoadedStyle (midi, blockSamples))
    {
        sendNoteOffs (0);
        lastGeneratedStep = -1;
        fallbackClockPpq = 0.0;
        return;
    }

    if (! isAccompanimentEnabled() || blockSamples <= 0)
    {
        sendNoteOffs (0);
        lastGeneratedStep = -1;
        fallbackClockPpq = 0.0;
        return;
    }

    if (playbackSampleRate <= 0.0)
    {
        sendNoteOffs (0);
        lastGeneratedStep = -1;
        fallbackClockPpq = 0.0;
        return;
    }

    double tempo = 120.0;
    int numerator = 4;
    int denominator = 4;
    if (auto* playHead = getPlayHead())
    {
        const auto position = playHead->getPosition();
        if (position.hasValue())
        {
            if (const auto hostTempo = position->getBpm(); hostTempo.hasValue() && *hostTempo > 0.0)
                tempo = *hostTempo;
            if (const auto signature = position->getTimeSignature(); signature.hasValue())
            {
                const auto validDenominator = signature->denominator == 1 || signature->denominator == 2
                                           || signature->denominator == 4 || signature->denominator == 8
                                           || signature->denominator == 16 || signature->denominator == 32;
                if (signature->numerator > 0 && signature->numerator <= 32 && validDenominator)
                {
                    numerator = signature->numerator;
                    denominator = signature->denominator;
                }
            }
        }
    }

    const double ppqStart = fallbackClockPpq;
    const double ppqPerBlock = static_cast<double> (blockSamples) * tempo
                             / (60.0 * playbackSampleRate);
    const double ppqEnd = ppqStart + ppqPerBlock;
    fallbackClockPpq = ppqEnd;
    const int stepsPerBar = numerator * 2;
    const double stepPpq = 2.0 / denominator;
    const auto firstStep = static_cast<int64_t> (std::ceil (ppqStart / stepPpq - 1.0e-9));
    const auto lastStep = static_cast<int64_t> (std::floor (ppqEnd / stepPpq - 1.0e-9));

    if (firstStep < lastGeneratedStep)
    {
        sendNoteOffs (0);
        lastGeneratedStep = firstStep - 1;
    }

    const int root = detectedChordRoot.load (std::memory_order_relaxed);
    const int quality = detectedChordQuality.load (std::memory_order_relaxed);
    if (root < 0 || root > 11)
    {
        sendNoteOffs (0);
        lastGeneratedStep = lastStep;
        return;
    }

    int third = 4;
    int fifth = 7;
    switch (quality)
    {
        case minor: case minorSeventh: case halfDiminishedSeventh: case diminishedSeventh:
            third = 3; break;
        case augmented: fifth = 8; break;
        case suspendedSecond: third = 2; break;
        case suspendedFourth: third = 5; break;
        default: break;
    }
    if (quality == diminished || quality == halfDiminishedSeventh || quality == diminishedSeventh)
        fifth = 6;

    int seventh = -1;
    if (quality == dominantSeventh || quality == minorSeventh || quality == halfDiminishedSeventh)
        seventh = 10;
    else if (quality == majorSeventh)
        seventh = 11;
    else if (quality == diminishedSeventh)
        seventh = 9;

    const int bassHalfBarStep = stepsPerBar / 2;
    const int firstChordStep = juce::jlimit (1, stepsPerBar - 1,
        static_cast<int> (std::llround (stepsPerBar * 0.25)));
    const int secondChordStep = juce::jlimit (1, stepsPerBar - 1,
        static_cast<int> (std::llround (stepsPerBar * 0.75)));
    for (auto step = juce::jmax (firstStep, lastGeneratedStep + 1);
         step <= lastStep; ++step)
    {
        const double eventPpq = static_cast<double> (step) * stepPpq;
        const int samplePosition = juce::jlimit (0, blockSamples - 1,
            static_cast<int> (std::llround ((eventPpq - ppqStart) * 60.0 * playbackSampleRate / tempo)));
        sendNoteOffs (samplePosition);

        int stepInBar = static_cast<int> (step % stepsPerBar);
        if (stepInBar < 0)
            stepInBar += stepsPerBar;
        const auto addGenerated = [&] (int note, int velocity)
        {
            note = juce::jlimit (0, 127, note);
            midi.addEvent (juce::MidiMessage::noteOn (1, note, static_cast<juce::uint8> (velocity)),
                           samplePosition);
            for (auto& active : activeGeneratedNotes)
                if (active == 255)
                {
                    active = static_cast<uint8_t> (note);
                    break;
                }
        };

        // The demonstration pattern follows the host's time-signature grid.
        // Bass marks the bar start and midpoint; chord stabs fall at quarter-bar points.
        if (stepInBar == 0 || stepInBar == bassHalfBarStep)
            addGenerated (36 + root, 92);

        if (stepInBar == firstChordStep || stepInBar == secondChordStep)
        {
            addGenerated (60 + root, 78);
            addGenerated (60 + ((root + third) % 12), 74);
            addGenerated (60 + ((root + fifth) % 12), 74);
            if (seventh >= 0)
                addGenerated (60 + ((root + seventh) % 12), 68);
        }

        lastGeneratedStep = step;
    }
}

bool SmartArrangerAudioProcessor::renderLoadedStyle (juce::MidiBuffer& midi,
                                                       int blockSamples) noexcept
{
    styleRendererCallCount.fetch_add (1, std::memory_order_relaxed);
    int slot = -1;
    const StylePlaybackData* style = nullptr;
    for (;;)
    {
        slot = activeStyleBuffer.load (std::memory_order_acquire);
        if (slot < 0 || slot > 1)
            return false;
        styleReaders[static_cast<size_t> (slot)].fetch_add (1, std::memory_order_acq_rel);
        if (activeStyleBuffer.load (std::memory_order_acquire) == slot)
        {
            style = &stylePlaybackBuffers[static_cast<size_t> (slot)];
            break;
        }
        styleReaders[static_cast<size_t> (slot)].fetch_sub (1, std::memory_order_release);
    }

    const auto releaseReader = [this, slot]
    {
        styleReaders[static_cast<size_t> (slot)].fetch_sub (1, std::memory_order_release);
    };
    const auto sendStyleNoteOffs = [this, &midi] (int samplePosition)
    {
        for (int channel = 0; channel < 16; ++channel)
            for (int note = 0; note < 128; ++note)
            {
                const auto index = static_cast<size_t> (channel * 128 + note);
                if (activeStyleNotes[index] != 0)
                {
                    const int outputNote = activeStyleOutputNotes[index] == 255
                                         ? note : activeStyleOutputNotes[index];
                    const int outputChannel = activeStyleOutputChannels[index] == 255
                                            ? channel + 1 : activeStyleOutputChannels[index];
                    midi.addEvent (juce::MidiMessage::noteOff (outputChannel, outputNote), samplePosition);
                    activeStyleNotes[index] = 0;
                    activeStyleOutputNotes[index] = 255;
                    activeStyleOutputChannels[index] = 255;
                }
            }
    };

    if (! isAccompanimentEnabled() || blockSamples <= 0 || playbackSampleRate <= 0.0)
    {
        sendStyleNoteOffs (0);
        styleWasPlaying = false;
        styleBufferInUse = -1;
        activeStyleSection = -1;
        queuedStyleSection = -1;
        queuedStyleSwitchPpq = -1.0;
        oneShotReturnSection = -1;
        stopAfterOneShot = false;
        stylePlaybackFinished.store (false, std::memory_order_release);
        styleHarmonyBar = -1;
        lastStyleBlockEndPpq = 0.0;
        styleClockPpq = 0.0;
        styleSectionStartPpq = 0.0;
        releaseReader();
        return true;
    }

    if (stylePlaybackFinished.load (std::memory_order_acquire))
    {
        sendStyleNoteOffs (0);
        releaseReader();
        return true;
    }

    double tempo = static_cast<double> (style->tempo);
    double barLengthPpq = static_cast<double> (style->numerator) * 4.0
                        / juce::jmax (1, style->denominator);
    if (auto* playHead = getPlayHead())
    {
        const auto position = playHead->getPosition();
        if (position.hasValue())
        {
            if (const auto hostTempo = position->getBpm(); hostTempo.hasValue() && *hostTempo > 0.0)
                tempo = *hostTempo;
            if (const auto signature = position->getTimeSignature();
                signature.hasValue() && signature->numerator > 0 && signature->denominator > 0)
                barLengthPpq = static_cast<double> (signature->numerator) * 4.0
                             / signature->denominator;
        }
    }

    if (style->ticksPerQuarter <= 0 || style->sections.empty())
    {
        sendStyleNoteOffs (0);
        styleWasPlaying = false;
        styleBufferInUse = -1;
        styleHarmonyBar = -1;
        lastStyleBlockEndPpq = 0.0;
        styleClockPpq = 0.0;
        releaseReader();
        return true;
    }

    if (styleWasPlaying && styleBufferInUse != slot)
    {
        sendStyleNoteOffs (0);
        styleHarmonyBar = -1;
        styleClockPpq = 0.0;
        styleSectionStartPpq = 0.0;
        styleWasPlaying = false;
        activeStyleSection = -1;
        queuedStyleSection = -1;
        queuedStyleSwitchPpq = -1.0;
        mainStyleSection = -1;
    }

    const double ppqStart = styleClockPpq;
    const double ppqPerBlock = static_cast<double> (blockSamples) * tempo
                             / (60.0 * playbackSampleRate);
    const double ppqEnd = ppqStart + ppqPerBlock;
    const auto firstMainSection = [&]()
    {
        for (size_t i = 0; i < style->sections.size(); ++i)
            if (style->sections[i].role == mainSection && style->sections[i].lengthTicks > 0
                && ! style->sections[i].events.empty())
                return static_cast<int> (i);
        return -1;
    };
    const auto isPlayableSection = [&] (int index)
    {
        return index >= 0 && index < static_cast<int> (style->sections.size())
            && style->sections[static_cast<size_t> (index)].lengthTicks > 0
            && ! style->sections[static_cast<size_t> (index)].events.empty();
    };
    if (mainStyleSection < 0 || mainStyleSection >= static_cast<int> (style->sections.size()))
        mainStyleSection = firstMainSection();

    const int requested = requestedStyleSection.load (std::memory_order_acquire);
    if (! styleWasPlaying)
    {
        activeStyleSection = isPlayableSection (requested) ? requested : firstMainSection();
        if (! isPlayableSection (activeStyleSection))
            for (size_t i = 0; i < style->sections.size(); ++i)
                if (isPlayableSection (static_cast<int> (i)))
                {
                    activeStyleSection = static_cast<int> (i);
                    break;
                }
        if (! isPlayableSection (activeStyleSection))
        {
            styleWasPlaying = false;
            releaseReader();
            return true;
        }
        styleSectionStartPpq = ppqStart;
        styleWasPlaying = true;
        styleBufferInUse = slot;
        lastRequestedStyleSection = activeStyleSection;
        const auto role = style->sections[static_cast<size_t> (activeStyleSection)].role;
        if (role == mainSection)
            mainStyleSection = activeStyleSection;
        oneShotReturnSection = role == introSection ? mainStyleSection : -1;
        stopAfterOneShot = role == endingSection;
    }
    else if (requested != lastRequestedStyleSection)
    {
        if (isPlayableSection (requested))
        {
            queuedStyleSection = requested;
            const auto& active = style->sections[static_cast<size_t> (activeStyleSection)];
            const bool activeIsOneShot = active.role == introSection || active.role == fillSection
                                      || active.role == breakSection || active.role == endingSection;
            if (activeIsOneShot)
                queuedStyleSwitchPpq = styleSectionStartPpq
                                     + static_cast<double> (active.lengthTicks) / style->ticksPerQuarter;
            else
                queuedStyleSwitchPpq = std::ceil (ppqStart / barLengthPpq - 1.0e-9) * barLengthPpq;
        }
        lastRequestedStyleSection = requested;
    }

    const auto sampleAtPpq = [&] (double ppq)
    {
        return juce::jlimit (0, blockSamples - 1,
            static_cast<int> (std::llround ((ppq - ppqStart) * 60.0 * playbackSampleRate / tempo)));
    };

    const auto armSection = [&] (int sectionIndex, double startPpq)
    {
        if (! isPlayableSection (sectionIndex))
            return;
        activeStyleSection = sectionIndex;
        requestedStyleSection.store (sectionIndex, std::memory_order_release);
        lastRequestedStyleSection = sectionIndex;
        styleSectionStartPpq = startPpq;
        styleHarmonyBar = -1;
        const int role = style->sections[static_cast<size_t> (sectionIndex)].role;
        if (role == mainSection)
            mainStyleSection = sectionIndex;
        oneShotReturnSection = role == introSection || role == fillSection || role == breakSection
                             ? mainStyleSection : -1;
        stopAfterOneShot = role == endingSection;
    };

    const auto scheduleSectionRange = [&] (double rangeStart, double rangeEnd)
    {
        if (activeStyleSection < 0 || activeStyleSection >= static_cast<int> (style->sections.size()))
            return;
        const auto& section = style->sections[static_cast<size_t> (activeStyleSection)];
        if (section.lengthTicks == 0 || rangeEnd <= rangeStart)
            return;

        const double sectionLengthPpq = static_cast<double> (section.lengthTicks) / style->ticksPerQuarter;
        const double localStart = juce::jmax (0.0, rangeStart - styleSectionStartPpq);
        const double localEnd = juce::jmax (localStart, rangeEnd - styleSectionStartPpq);
        const auto firstCycle = static_cast<int64_t> (std::floor (localStart / sectionLengthPpq));
        const auto lastCycle = static_cast<int64_t> (std::floor ((localEnd - 1.0e-12) / sectionLengthPpq));
        for (int64_t cycle = firstCycle; cycle <= lastCycle; ++cycle)
        {
            const double cycleStart = styleSectionStartPpq + static_cast<double> (cycle) * sectionLengthPpq;
            const double segmentStart = juce::jmax (rangeStart, cycleStart);
            const double segmentEnd = juce::jmin (rangeEnd, cycleStart + sectionLengthPpq);
            if (segmentEnd <= segmentStart)
                continue;
            if (segmentStart <= cycleStart + 1.0e-12 && cycle > 0)
                sendStyleNoteOffs (sampleAtPpq (cycleStart));

            const auto firstTick = static_cast<uint64_t> (juce::jmax (0.0,
                std::ceil ((segmentStart - cycleStart) * style->ticksPerQuarter - 1.0e-9)));
            const auto endTick = segmentEnd >= cycleStart + sectionLengthPpq - 1.0e-12
                ? section.lengthTicks
                : static_cast<uint64_t> (juce::jmax (0.0,
                    std::ceil ((segmentEnd - cycleStart) * style->ticksPerQuarter - 1.0e-9)));
            auto event = std::lower_bound (section.events.begin(), section.events.end(), firstTick,
                [] (const SmartArrangerStyleEvent& candidate, uint64_t tick)
                {
                    return candidate.tick < tick;
                });
            for (; event != section.events.end() && event->tick < endTick; ++event)
            {
                styleScannedEventCount.fetch_add (1, std::memory_order_relaxed);
                const double eventPpq = cycleStart + static_cast<double> (event->tick) / style->ticksPerQuarter;
                const int samplePosition = sampleAtPpq (eventPpq);
                const auto command = static_cast<uint8_t> (event->status & 0xf0u);
                const int channel = static_cast<int> (event->status & 0x0fu);
                const bool isChannelVoiceMessage = event->status < 0xf0u;
                const int outputChannel = isChannelVoiceMessage
                    ? getCasmOutputChannel (section, channel) + 1 : channel + 1;
                const auto outputStatus = static_cast<uint8_t> (
                    isChannelVoiceMessage
                        ? ((event->status & 0xf0u) | ((outputChannel - 1) & 0x0fu))
                        : event->status);
                const auto bar = static_cast<int64_t> (std::floor (eventPpq / barLengthPpq));
                if (bar != styleHarmonyBar)
                {
                    const int liveRoot = detectedChordRoot.load (std::memory_order_relaxed);
                    const int liveQuality = detectedChordQuality.load (std::memory_order_relaxed);
                    if (liveRoot >= 0 && liveRoot < 12)
                    {
                        if (liveRoot != styleHarmonyRoot || liveQuality != styleHarmonyQuality)
                            sendStyleNoteOffs (samplePosition);
                        styleHarmonyRoot = liveRoot;
                        styleHarmonyQuality = liveQuality;
                        styleHarmonyHasRecognizedChord = true;
                    }
                    styleHarmonyBar = bar;
                }

                const auto noteIndex = static_cast<size_t> (channel * 128 + event->data1);
                if (event->dataBytes == 1)
                    midi.addEvent (juce::MidiMessage (outputStatus, event->data1), samplePosition);
                else if (command == 0x90u && event->data2 != 0)
                {
                    const int maskChordQuality = styleHarmonyHasRecognizedChord
                        ? styleHarmonyQuality : noChord;
                    if (isCasmSourceEligibleForChord (maskChordQuality, section, channel))
                    {
                        const int outputNote = transformStyleNoteForCasm (event->data1,
                            styleHarmonyRoot, styleHarmonyQuality, section, channel);
                        midi.addEvent (juce::MidiMessage::noteOn (outputChannel, outputNote,
                            static_cast<juce::uint8> (event->data2)), samplePosition);
                        styleNoteOnCount.fetch_add (1, std::memory_order_relaxed);
                        activeStyleNotes[noteIndex] = 1;
                        activeStyleOutputNotes[noteIndex] = static_cast<uint8_t> (outputNote);
                        activeStyleOutputChannels[noteIndex] = static_cast<uint8_t> (outputChannel);
                    }
                    else
                        styleMaskRejectedCount.fetch_add (1, std::memory_order_relaxed);
                }
                else if (command == 0x80u || (command == 0x90u && event->data2 == 0))
                {
                    if (activeStyleNotes[noteIndex] != 0)
                    {
                        const int outputNote = activeStyleOutputNotes[noteIndex] == 255
                            ? transformStyleNoteForCasm (event->data1, styleHarmonyRoot,
                                styleHarmonyQuality, section, channel)
                            : activeStyleOutputNotes[noteIndex];
                        const int noteOffChannel = activeStyleOutputChannels[noteIndex] == 255
                            ? outputChannel : activeStyleOutputChannels[noteIndex];
                        midi.addEvent (juce::MidiMessage::noteOff (noteOffChannel, outputNote,
                            static_cast<juce::uint8> (event->data2)), samplePosition);
                    }
                    activeStyleNotes[noteIndex] = 0;
                    activeStyleOutputNotes[noteIndex] = 255;
                    activeStyleOutputChannels[noteIndex] = 255;
                }
                else
                    midi.addEvent (juce::MidiMessage (outputStatus, event->data1, event->data2),
                                   samplePosition);
            }
        }
    };

    double cursor = ppqStart;
    while (cursor < ppqEnd - 1.0e-12)
    {
        if (! isPlayableSection (activeStyleSection))
        {
            sendStyleNoteOffs (sampleAtPpq (cursor));
            styleWasPlaying = false;
            activeStyleSection = -1;
            break;
        }
        const auto& active = style->sections[static_cast<size_t> (activeStyleSection)];
        const double activeLengthPpq = static_cast<double> (active.lengthTicks) / style->ticksPerQuarter;
        double transitionPpq = ppqEnd;
        bool queuedTransition = false;
        if (queuedStyleSection >= 0 && queuedStyleSwitchPpq >= cursor - 1.0e-12
            && queuedStyleSwitchPpq <= transitionPpq)
        {
            transitionPpq = juce::jmax (cursor, queuedStyleSwitchPpq);
            queuedTransition = true;
        }
        const bool oneShot = active.role == introSection || active.role == fillSection
                          || active.role == breakSection || active.role == endingSection;
        const double oneShotEnd = styleSectionStartPpq + activeLengthPpq;
        if (oneShot && oneShotEnd >= cursor - 1.0e-12 && oneShotEnd < transitionPpq)
        {
            transitionPpq = oneShotEnd;
            queuedTransition = false;
        }

        const double segmentEnd = juce::jmin (ppqEnd, transitionPpq);
        scheduleSectionRange (cursor, segmentEnd);
        cursor = segmentEnd;
        if (cursor >= ppqEnd - 1.0e-12)
            break;

        const int switchSample = sampleAtPpq (cursor);
        sendStyleNoteOffs (switchSample);
        if (queuedTransition && queuedStyleSection >= 0)
        {
            const int nextSection = queuedStyleSection;
            queuedStyleSection = -1;
            queuedStyleSwitchPpq = -1.0;
            armSection (nextSection, cursor);
        }
        else if (oneShot)
        {
            if (stopAfterOneShot)
            {
                stylePlaybackFinished.store (true, std::memory_order_release);
                styleWasPlaying = false;
                activeStyleSection = -1;
                break;
            }
            const int returnSection = oneShotReturnSection >= 0 ? oneShotReturnSection : mainStyleSection;
            if (returnSection >= 0 && returnSection < static_cast<int> (style->sections.size()))
                armSection (returnSection, cursor);
            else if (activeStyleSection >= 0)
                armSection (activeStyleSection, cursor);
        }
        else
            break;
    }

    lastStyleBlockEndPpq = ppqEnd;
    styleClockPpq = ppqEnd;
    releaseReader();
    return true;
}

int SmartArrangerAudioProcessor::getDetectedChordRoot() const noexcept
{
    return detectedChordRoot.load (std::memory_order_relaxed);
}

int SmartArrangerAudioProcessor::getDetectedChordQuality() const noexcept
{
    return detectedChordQuality.load (std::memory_order_relaxed);
}

int SmartArrangerAudioProcessor::getDetectedBassNote() const noexcept
{
    return detectedBassNote.load (std::memory_order_relaxed);
}

void SmartArrangerAudioProcessor::setAccompanimentEnabled (bool shouldEnable) noexcept
{
    if (auto* parameter = parameters.getParameter ("accompanimentEnabled"))
        parameter->setValueNotifyingHost (shouldEnable ? 1.0f : 0.0f);
}

bool SmartArrangerAudioProcessor::isAccompanimentEnabled() const noexcept
{
    return accompanimentEnabledParameter != nullptr
        && accompanimentEnabledParameter->load (std::memory_order_relaxed) >= 0.5f;
}

void SmartArrangerAudioProcessor::setMidiThruEnabled (bool shouldEnable) noexcept
{
    midiThruEnabled.store (shouldEnable, std::memory_order_relaxed);
}

void SmartArrangerAudioProcessor::setKeyboardSplitPoint (int midiNote) noexcept
{
    keyboardSplitPoint.store (juce::jlimit (-1, 127, midiNote), std::memory_order_relaxed);
}

juce::AudioProcessorValueTreeState& SmartArrangerAudioProcessor::getParameterState() noexcept
{
    return parameters;
}

juce::AudioProcessorEditor* SmartArrangerAudioProcessor::createEditor()
{
    return new SmartArrangerAudioProcessorEditor (*this);
}

bool SmartArrangerAudioProcessor::hasEditor() const { return true; }
const juce::String SmartArrangerAudioProcessor::getName() const { return "Smart Arranger"; }
bool SmartArrangerAudioProcessor::acceptsMidi() const { return true; }
bool SmartArrangerAudioProcessor::producesMidi() const { return true; }
bool SmartArrangerAudioProcessor::isMidiEffect() const { return true; }
double SmartArrangerAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int SmartArrangerAudioProcessor::getNumPrograms() { return 1; }
int SmartArrangerAudioProcessor::getCurrentProgram() { return 0; }
void SmartArrangerAudioProcessor::setCurrentProgram (int) {}
const juce::String SmartArrangerAudioProcessor::getProgramName (int) { return {}; }
void SmartArrangerAudioProcessor::changeProgramName (int, const juce::String&) {}

void SmartArrangerAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destination);
}

void SmartArrangerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType().toString()))
        {
            parameters.replaceState (juce::ValueTree::fromXml (*xml));

            const juce::File styleFile (getStoredStylePath());
            if (styleFile.existsAsFile())
            {
                SmartArrangerStyleInfo style;
                juce::String error;
                const int savedSection = getStoredStyleSection();
                if (SmartArrangerStyleFile::inspect (styleFile, style, error)
                    && savedSection >= 0
                    && savedSection < static_cast<int> (style.sections.size()))
                    setLoadedStyle (style, savedSection, error, styleFile);
            }
            else
            {
                clearLoadedStyle (false);
            }
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SmartArrangerAudioProcessor();
}
