#include "MainComponent.h"
#include "../PluginEditor.h"

#define TSF_IMPLEMENTATION
#include "../../ThirdParty/TinySoundFont/tsf.h"

#include <algorithm>
#include <cstring>
#include <map>

namespace
{
const juce::Colour gold (0xffffc45a);
const juce::Colour muted (0xffc2c9d1);
const juce::Colour darkPanel (0xff0d141c);

juce::String midiNoteLabel (int note)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[note % 12]) + juce::String (note / 12 - 1);
}

class SoundFontSettingsPanel final : public juce::Component
{
public:
    SoundFontSettingsPanel (bool enabled, bool shouldRememberStyle, int splitPointMidiNote, juce::String path,
                            std::function<void(bool)> onEnabledChanged,
                            std::function<void(bool)> onRememberStyleChanged,
                            std::function<void(int)> onSplitPointChanged,
                            std::function<void()> onChoose)
        : enabledChanged (std::move (onEnabledChanged)),
          rememberStyleChanged (std::move (onRememberStyleChanged)),
          splitPointChanged (std::move (onSplitPointChanged)),
          choose (std::move (onChoose))
    {
        splitPointNote = splitPointMidiNote;
        heading.setText ("Playback and Startup", juce::dontSendNotification);
        heading.setFont (juce::Font (17.0f, juce::Font::bold));
        addAndMakeVisible (heading);
        enable.setButtonText ("Enable internal SF2 playback");
        enable.setToggleState (enabled, juce::dontSendNotification);
        enable.onClick = [this] { enabledChanged (enable.getToggleState()); };
        addAndMakeVisible (enable);
        rememberStyle.setButtonText ("Remember last style and load it at startup");
        rememberStyle.setToggleState (shouldRememberStyle, juce::dontSendNotification);
        rememberStyle.onClick = [this]
        {
            rememberStyleChanged (rememberStyle.getToggleState());
        };
        addAndMakeVisible (rememberStyle);
        splitLabel.setText ("Keyboard Split Point", juce::dontSendNotification);
        splitLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc2c9d1));
        addAndMakeVisible (splitLabel);
        for (int note = 24; note <= 84; ++note)
            splitPointSelector.addItem (midiNoteLabel (note), note + 1);
        splitPointSelector.setSelectedId (juce::jlimit (24, 84, splitPointNote) + 1, juce::dontSendNotification);
        splitPointSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff202830));
        splitPointSelector.setColour (juce::ComboBox::textColourId, juce::Colour (0xfff3f6f9));
        splitPointSelector.setTooltip ("Notes at or below this key control accompaniment and are not played. Higher notes play as melody.");
        splitPointSelector.onChange = [this]
        {
            const int note = splitPointSelector.getSelectedId() - 1;
            splitHelp.setText ("At or below " + midiNoteLabel (note)
                                   + ": chord control only. Above " + midiNoteLabel (note)
                                   + ": melody notes pass on their original MIDI channels.",
                               juce::dontSendNotification);
            splitPointChanged (note);
        };
        addAndMakeVisible (splitPointSelector);
        splitHelp.setText ("At or below " + midiNoteLabel (splitPointNote)
                               + ": chord control only. Above " + midiNoteLabel (splitPointNote)
                               + ": melody notes pass on their original MIDI channels.",
                           juce::dontSendNotification);
        splitHelp.setColour (juce::Label::textColourId, juce::Colour (0xff909ba6));
        splitHelp.setFont (juce::Font (12.0f));
        addAndMakeVisible (splitHelp);
        soundFont.setText (path.isNotEmpty() ? path : "No SoundFont selected", juce::dontSendNotification);
        soundFont.setColour (juce::Label::textColourId, juce::Colour (0xffc2c9d1));
        soundFont.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (soundFont);
        chooseButton.setButtonText ("Choose GM/XG SoundFont (.sf2)");
        chooseButton.onClick = [this] { choose(); };
        addAndMakeVisible (chooseButton);
        help.setText ("Choose a GM or XG SF2 bank. Preset names and bank numbers are read from the file; the app does not bundle SoundFonts.",
                      juce::dontSendNotification);
        help.setColour (juce::Label::textColourId, juce::Colour (0xff909ba6));
        help.setFont (juce::Font (12.0f));
        help.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (help);
        setSize (500, 300);
    }

    void setSoundFontPath (const juce::String& path)
    {
        soundFont.setText (path.isNotEmpty() ? path : "No SoundFont selected", juce::dontSendNotification);
    }

    void setStatus (const juce::String& text)
    {
        help.setText (text, juce::dontSendNotification);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (18);
        heading.setBounds (b.removeFromTop (26));
        b.removeFromTop (8);
        enable.setBounds (b.removeFromTop (28));
        b.removeFromTop (8);
        rememberStyle.setBounds (b.removeFromTop (28));
        b.removeFromTop (8);
        auto splitRow = b.removeFromTop (28);
        splitLabel.setBounds (splitRow.removeFromLeft (190));
        splitPointSelector.setBounds (splitRow);
        b.removeFromTop (3);
        splitHelp.setBounds (b.removeFromTop (24));
        b.removeFromTop (5);
        chooseButton.setBounds (b.removeFromTop (34));
        b.removeFromTop (5);
        soundFont.setBounds (b.removeFromTop (25));
        b.removeFromTop (6);
        help.setBounds (b);
    }

private:
    juce::Label heading, splitLabel, splitHelp, soundFont, help;
    juce::ToggleButton enable, rememberStyle;
    juce::ComboBox splitPointSelector;
    int splitPointNote = 36;
    juce::TextButton chooseButton;
    std::function<void(bool)> enabledChanged;
    std::function<void(bool)> rememberStyleChanged;
    std::function<void(int)> splitPointChanged;
    std::function<void()> choose;
};

juce::String gmProgramName (int program)
{
    static const char* names[] = {
        "Piano", "Bright Piano", "Electric Grand", "Honky-tonk Piano", "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavinet",
        "Celesta", "Glockenspiel", "Music Box", "Vibraphone", "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
        "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ", "Reed Organ", "Accordion", "Harmonica", "Tango Accordion",
        "Nylon Guitar", "Steel Guitar", "Jazz Guitar", "Clean Guitar", "Muted Guitar", "Overdrive Guitar", "Distortion Guitar", "Guitar Harmonics",
        "Acoustic Bass", "Finger Bass", "Pick Bass", "Fretless Bass", "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
        "Violin", "Viola", "Cello", "Contrabass", "Tremolo Strings", "Pizzicato Strings", "Harp", "Timpani",
        "Strings", "Slow Strings", "Synth Strings 1", "Synth Strings 2", "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit",
        "Trumpet", "Trombone", "Tuba", "Muted Trumpet", "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
        "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax", "Oboe", "English Horn", "Bassoon", "Clarinet",
        "Piccolo", "Flute", "Recorder", "Pan Flute", "Bottle", "Shakuhachi", "Whistle", "Ocarina",
        "Square Lead", "Saw Lead", "Calliope Lead", "Chiff Lead", "Charang Lead", "Voice Lead", "Fifths Lead", "Bass + Lead",
        "New Age Pad", "Warm Pad", "Polysynth Pad", "Choir Pad", "Bowed Pad", "Metallic Pad", "Halo Pad", "Sweep Pad",
        "Rain FX", "Soundtrack FX", "Crystal FX", "Atmosphere FX", "Brightness FX", "Goblins FX", "Echoes FX", "Sci-fi FX",
        "Sitar", "Banjo", "Shamisen", "Koto", "Kalimba", "Bagpipe", "Fiddle", "Shanai",
        "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock", "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
        "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird", "Telephone", "Helicopter", "Applause", "Gunshot"
    };
    return program >= 0 && program < 128 ? juce::String (names[program]) : juce::String();
}

std::vector<SmartArrangerSoundFontPreset> readSoundFontPresets (const juce::File& file)
{
    juce::MemoryBlock bytes;
    if (! file.loadFileAsData (bytes) || bytes.getSize() < 12)
        return {};
    const auto* data = static_cast<const uint8_t*> (bytes.getData());
    const size_t size = bytes.getSize();
    auto read32 = [data, size] (size_t at) -> uint32_t
    {
        return at + 4 <= size ? juce::ByteOrder::littleEndianInt (data + at) : 0;
    };
    auto isId = [data, size] (size_t at, const char* id)
    {
        return at + 4 <= size && std::memcmp (data + at, id, 4) == 0;
    };
    if (! isId (0, "RIFF") || ! isId (8, "sfbk"))
        return {};

    size_t phdr = size;
    for (size_t pos = 12; pos + 8 <= size;)
    {
        const uint32_t chunkSize = read32 (pos + 4);
        const size_t body = pos + 8;
        if (chunkSize > size - body)
            break;
        if (isId (pos, "LIST") && chunkSize >= 4 && isId (body, "pdta"))
        {
            for (size_t child = body + 4; child + 8 <= body + chunkSize;)
            {
                const uint32_t childSize = read32 (child + 4);
                if (childSize > body + chunkSize - (child + 8))
                    break;
                if (isId (child, "phdr"))
                    phdr = child + 8;
                child += 8 + childSize + (childSize & 1u);
            }
        }
        pos = body + chunkSize + (chunkSize & 1u);
    }
    if (phdr == size)
        return {};

    const size_t phdrSize = read32 (phdr - 4);
    constexpr size_t recordSize = 38;
    const size_t recordCount = phdrSize / recordSize;
    std::vector<SmartArrangerSoundFontPreset> presets;
    presets.reserve (recordCount > 0 ? recordCount - 1 : 0);
    for (size_t i = 0; i + 1 < recordCount; ++i) // final record is the SF2 EOP sentinel
    {
        const size_t record = phdr + i * recordSize;
        char name[21] {};
        std::memcpy (name, data + record, 20);
        const int program = juce::ByteOrder::littleEndianInt (data + record + 20) & 0xffff;
        const int bank = juce::ByteOrder::littleEndianInt (data + record + 22) & 0xffff;
        presets.push_back ({ bank, program, -1, juce::String::fromUTF8 (name).trim() });
    }
    return presets;
}

juce::String nicePartName (juce::String label)
{
    label = label.trim();
    if (label.equalsIgnoreCase ("Bs")) return "Bass";
    if (label.equalsIgnoreCase ("Gt")) return "Guitar";
    if (label.equalsIgnoreCase ("Pf")) return "Piano";
    if (label.equalsIgnoreCase ("Vox")) return "Voice";
    if (label.equalsIgnoreCase ("Drs")) return "Drums";
    if (label.equalsIgnoreCase ("NGt") || label.equalsIgnoreCase ("NIGt")) return "Nylon Guitar";
    return label;
}

void styleTextButton (juce::TextButton& button)
{
    button.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff262b30));
    button.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffdf9b));
    button.setColour (juce::ComboBox::outlineColourId, gold.withAlpha (0.65f));
}
}

SmartArrangerStandaloneComponent::SmartArrangerStandaloneComponent()
{
    for (auto& program : activeSoundFontPrograms)
        program.store (-1, std::memory_order_relaxed);

    juce::PropertiesFile::Options settingsOptions;
    settingsOptions.applicationName = "Smart Arranger Standalone";
    settingsOptions.filenameSuffix = "settings";
    settingsOptions.folderName = "SmartArranger";
    settingsOptions.osxLibrarySubFolder = "Application Support";
    settingsFile = std::make_unique<juce::PropertiesFile> (settingsOptions);
    internalSoundsEnabled.store (settingsFile->getBoolValue ("internalSoundsEnabled", false));
    keyboardSplitPoint.store (juce::jlimit (24, 84, settingsFile->getIntValue ("keyboardSplitPoint", 48)),
                              std::memory_order_relaxed);
    viewMode = settingsFile->getIntValue ("viewMode", 100);
    if (viewMode != 0 && viewMode != 100 && viewMode != 101 && viewMode != 102)
        viewMode = 100;
    showMidiControls = settingsFile->getBoolValue ("viewShowMidiControls", true);
    showStyleParts = settingsFile->getBoolValue ("viewShowStyleParts", true);
    showMidiActivity = settingsFile->getBoolValue ("viewShowMidiActivity", true);
    showArranger = settingsFile->getBoolValue ("viewShowArranger", true);

    setAudioChannels (0, 2);
    if (auto* deviceType = deviceManager.getCurrentDeviceTypeObject())
    {
        const auto names = deviceType->getDeviceNames (false);
        const int defaultIndex = deviceType->getDefaultDeviceIndex (false);
        if (juce::isPositiveAndBelow (defaultIndex, names.size()))
            lastSystemDefaultAudioOutput = names[defaultIndex];
    }
    processor.setMidiThruEnabled (false);
    processor.setKeyboardSplitPoint (keyboardSplitPoint.load (std::memory_order_relaxed));
    processor.setAccompanimentEnabled (false);
    editor.reset (processor.createEditor());
    arrangerViewport.setViewedComponent (editor.get(), false);
    arrangerViewport.setScrollBarsShown (true, false);
    arrangerViewport.setScrollBarThickness (12);
    arrangerViewport.setColour (juce::ScrollBar::thumbColourId, gold.withAlpha (0.75f));
    editor->setVisible (true);
    addAndMakeVisible (arrangerViewport);

    inputLabel.setText ("MIDI IN", juce::dontSendNotification);
    inputLabel.setColour (juce::Label::textColourId, muted);
    inputLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    addAndMakeVisible (inputLabel);
    inputSelector.setTextWhenNothingSelected ("Select MIDI keyboard (optional)");
    inputSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff202830));
    inputSelector.setColour (juce::ComboBox::textColourId, juce::Colour (0xfff3f6f9));
    inputSelector.onChange = [this] { selectInput (inputSelector.getSelectedId()); };
    addAndMakeVisible (inputSelector);
    outputLabel.setText ("MIDI OUT", juce::dontSendNotification);
    outputLabel.setColour (juce::Label::textColourId, muted);
    outputLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    addAndMakeVisible (outputLabel);
    outputSelector.setTextWhenNothingSelected ("Select MIDI output");
    outputSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff202830));
    outputSelector.setColour (juce::ComboBox::textColourId, juce::Colour (0xfff3f6f9));
    outputSelector.onChange = [this] { selectOutput (outputSelector.getSelectedId()); };
    addAndMakeVisible (outputSelector);
    addAndMakeVisible (midiInLamp);
    addAndMakeVisible (midiThruLamp);
    addAndMakeVisible (midiOutLamp);

    midiInActivity.setText ("IN notes: 0", juce::dontSendNotification);
    midiInActivity.setColour (juce::Label::textColourId, muted);
    midiInActivity.setFont (juce::Font (11.0f, juce::Font::bold));
    addAndMakeVisible (midiInActivity);

    midiThruButton.setClickingTogglesState (true);
    midiThruButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff262b30));
    midiThruButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff49634a));
    midiThruButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffdf9b));
    midiThruButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffefffed));
    midiThruButton.onClick = [this]
    {
        const bool enabled = midiThruButton.getToggleState();
        midiThruEnabled.store (enabled, std::memory_order_relaxed);
        processor.setMidiThruEnabled (enabled);
        midiThruButton.setButtonText (enabled ? "MIDI THRU ON" : "MIDI THRU OFF");
    };
    midiThruButton.setTooltip ("Upper-zone melody notes always play. This switch also passes incoming MIDI controllers and other messages.");
    addAndMakeVisible (midiThruButton);

    midiOutActivity.setText ("OUT: 0", juce::dontSendNotification);
    midiOutActivity.setColour (juce::Label::textColourId, muted);
    midiOutActivity.setFont (juce::Font (11.0f, juce::Font::bold));
    addAndMakeVisible (midiOutActivity);

    outputStatus.setText ("Creating CoreMIDI output...", juce::dontSendNotification);
    outputStatus.setColour (juce::Label::textColourId, muted);
    outputStatus.setFont (juce::Font (13.0f, juce::Font::bold));
    addAndMakeVisible (outputStatus);
    styleTextButton (panicButton);
    panicButton.onClick = [this] { sendAllNotesOff(); };
    addAndMakeVisible (panicButton);

    partsHeader.setText ("STYLE PARTS - MIDI CHANNEL AND INSTRUMENT", juce::dontSendNotification);
    partsHeader.setColour (juce::Label::textColourId, gold);
    partsHeader.setFont (juce::Font (14.0f, juce::Font::bold));
    addAndMakeVisible (partsHeader);
    partsList.setMultiLine (true);
    partsList.setReadOnly (true);
    partsList.setScrollbarsShown (true);
    partsList.setCaretVisible (false);
    partsList.setColour (juce::TextEditor::backgroundColourId, darkPanel);
    partsList.setColour (juce::TextEditor::textColourId, muted);
    partsList.setColour (juce::TextEditor::outlineColourId, gold.withAlpha (0.45f));
    partsList.setFont (juce::Font (juce::FontOptions().withName (juce::Font::getDefaultMonospacedFontName())
                                                        .withHeight (14.0f)));
    partsList.setText ("Load a Yamaha style in the arranger window to see its MIDI parts here.");
    addAndMakeVisible (partsList);

    activityHeader.setText ("MIDI OUT ACTIVITY - click a channel to mute or unmute", juce::dontSendNotification);
    activityHeader.setColour (juce::Label::textColourId, gold);
    activityHeader.setFont (juce::Font (12.0f, juce::Font::bold));
    addAndMakeVisible (activityHeader);
    for (int channel = 0; channel < 16; ++channel)
    {
        auto& button = channelButtons[(size_t) channel];
        button.setButtonText (juce::String::formatted ("%02d", channel + 1));
        button.setTooltip ("Click to mute MIDI channel " + juce::String (channel + 1));
        button.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff252a30));
        button.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff923e37));
        button.setColour (juce::TextButton::textColourOffId, muted);
        button.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffffe2a0));
        button.onClick = [this, channel]
        {
            setChannelMuted (channel, ! channelMuted[(size_t) channel].load (std::memory_order_relaxed));
        };
        button.onRightClick = [this, channel] { showChannelInstrumentMenu (channel); };
        addAndMakeVisible (button);
        channelCounters[(size_t) channel].setText ("0", juce::dontSendNotification);
        channelCounters[(size_t) channel].setJustificationType (juce::Justification::centred);
        channelCounters[(size_t) channel].setColour (juce::Label::textColourId, muted);
        channelCounters[(size_t) channel].setFont (juce::Font (10.0f));
        addAndMakeVisible (channelCounters[(size_t) channel]);
    }

    addKeyListener (this);
    setWantsKeyboardFocus (true);
    refreshInputs();
    midiOutput = juce::MidiOutput::createNewDevice ("Smart Arranger MIDI Out");
    if (midiOutput != nullptr)
    {
        midiOutput->startBackgroundThread();
        outputStatus.setText ("MIDI OUT ready", juce::dontSendNotification);
        outputStatus.setColour (juce::Label::textColourId, juce::Colour (0xff8fdb99));
    }
    else
    {
        outputStatus.setText ("MIDI OUT unavailable", juce::dontSendNotification);
        outputStatus.setColour (juce::Label::textColourId, juce::Colour (0xffff8c83));
    }
    refreshOutputs();
    const auto savedMidiOutput = settingsFile->getValue ("midiOutputIdentifier", "SMART_ARRANGER_VIRTUAL");
    if (savedMidiOutput.isNotEmpty() && savedMidiOutput != "SMART_ARRANGER_VIRTUAL")
    {
        for (int i = 0; i < midiOutputDevices.size(); ++i)
            if (midiOutputDevices.getReference (i).identifier == savedMidiOutput)
            {
                outputSelector.setSelectedId (i + 2, juce::sendNotification);
                break;
            }
    }
    if (settingsFile->getBoolValue ("rememberLastStyle", true))
    {
        const juce::File lastStyle (settingsFile->getValue ("lastStylePath"));
        if (lastStyle.existsAsFile())
        {
            if (auto* arranger = dynamic_cast<SmartArrangerAudioProcessorEditor*> (editor.get()))
                arranger->loadStyleFile (lastStyle);
            if (processor.getStoredStylePath().isNotEmpty())
            {
                lastInspectedStylePath = processor.getStoredStylePath();
                updateStyleChannels (juce::File (lastInspectedStylePath));
            }
        }
    }
    const auto savedSoundFont = settingsFile->getValue ("soundFontPath");
    if (savedSoundFont.isNotEmpty() && juce::File (savedSoundFont).existsAsFile())
        loadSoundFont (juce::File (savedSoundFont));
    refreshViewContent();
    startTimerHz (8);
}

SmartArrangerStandaloneComponent::~SmartArrangerStandaloneComponent()
{
    stopTimer();
    if (midiInput != nullptr)
        midiInput->stop();
    midiInput.reset();
    shutdownAudio();
    if (auto* pending = pendingSoundFont.exchange (nullptr))
        tsf_close (pending);
    if (auto* retired = retiredSoundFont.exchange (nullptr))
        tsf_close (retired);
    if (activeSoundFont != nullptr)
        tsf_close (activeSoundFont);
    editor.reset();
    if (midiOutput != nullptr)
        midiOutput->stopBackgroundThread();
    midiOutput.reset();
    if (selectedMidiOutput != nullptr)
        selectedMidiOutput->stopBackgroundThread();
    selectedMidiOutput.reset();
}

void SmartArrangerStandaloneComponent::prepareToPlay (int expectedBlockSize, double sampleRate)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    midiCollector.reset (currentSampleRate);
    midiCollector.ensureStorageAllocated (16384);
    processor.prepareToPlay (currentSampleRate, expectedBlockSize);
    outgoingMidi.ensureSize (16384);
    soundFontScratch.resize ((size_t) juce::jmax (expectedBlockSize, 512) * 2u);
}

void SmartArrangerStandaloneComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    audioCallbackCount.fetch_add (1, std::memory_order_relaxed);
    info.clearActiveBufferRegion();
    if (info.numSamples <= 0)
        return;
    juce::MidiBuffer midi;
    midiCollector.removeNextBlockOfMessages (midi, info.numSamples);
    juce::AudioBuffer<float> silentAudio (info.buffer->getArrayOfWritePointers(),
                                         info.buffer->getNumChannels(), info.startSample,
                                         info.numSamples);
    processor.processBlock (silentAudio, midi);
    outgoingMidi.clear();
    for (int channel = 0; channel < 16; ++channel)
        if (pendingInstrumentChange[(size_t) channel].exchange (false, std::memory_order_relaxed)
            && manualInstrumentEnabled[(size_t) channel].load (std::memory_order_relaxed)
            && ! channelMuted[(size_t) channel].load (std::memory_order_relaxed))
        {
            const int bank = juce::jlimit (0, 16383,
                manualInstrumentBank[(size_t) channel].load (std::memory_order_relaxed));
            outgoingMidi.addEvent (juce::MidiMessage::controllerEvent (channel + 1, 0, bank / 128), 0);
            outgoingMidi.addEvent (juce::MidiMessage::controllerEvent (channel + 1, 32, bank % 128), 0);
            outgoingMidi.addEvent (juce::MidiMessage::programChange (channel + 1,
                manualInstrumentProgram[(size_t) channel].load (std::memory_order_relaxed)), 0);
        }
    for (const auto metadata : midi)
    {
        const auto& message = metadata.getMessage();
        const int channel = message.getChannel() - 1;
        const auto status = message.getRawData()[0];
        if (channel >= 0 && channel < 16
            && manualInstrumentEnabled[(size_t) channel].load (std::memory_order_relaxed)
            && (message.isProgramChange()
                || (message.isController() && (message.getControllerNumber() == 0
                                               || message.getControllerNumber() == 32))))
            continue;
        if (status >= 0x80 && status < 0xf0 && channel >= 0 && channel < 16
            && channelMuted[(size_t) channel].load (std::memory_order_relaxed))
            continue;
        outgoingMidi.addEvent (message, metadata.samplePosition);
    }
    for (int channel = 0; channel < 16; ++channel)
    {
        if (pendingChannelPanic[(size_t) channel].exchange (false, std::memory_order_relaxed))
            outgoingMidi.addEvent (juce::MidiMessage::controllerEvent (channel + 1, 123, 0), 0);
    }
    midi.swapWith (outgoingMidi);
    outgoingMidi.clear();
    renderInternalSound (info, midi);
    if (! midi.isEmpty())
    {
        midiOutEventCount.fetch_add (static_cast<uint64_t> (midi.getNumEvents()), std::memory_order_relaxed);
        midiOutPulse.store (true, std::memory_order_relaxed);
    }

    for (const auto metadata : midi)
    {
        const auto& message = metadata.getMessage();
        if (message.isNoteOn())
        {
            const int channel = message.getChannel() - 1;
            if (channel >= 0 && channel < 16)
            {
                noteOnCounts[(size_t) channel].fetch_add (1, std::memory_order_relaxed);
                channelActivityPulse[(size_t) channel].store (true, std::memory_order_relaxed);
            }
        }
    }
    if (! midi.isEmpty())
    {
        const juce::ScopedLock lock (midiOutputLock);
        auto* activeMidiOutput = selectedMidiOutputId == 1 ? midiOutput.get() : selectedMidiOutput.get();
        if (activeMidiOutput != nullptr)
            activeMidiOutput->sendBlockOfMessages (midi, juce::Time::getMillisecondCounterHiRes() + 3.0,
                                                   currentSampleRate);
    }
}

void SmartArrangerStandaloneComponent::releaseResources()
{
    processor.releaseResources();
}

void SmartArrangerStandaloneComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff070b10));
    auto frame = getLocalBounds().toFloat().reduced (8.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff39414a), 0.0f, 0.0f,
        juce::Colour (0xff111820), (float) getWidth(), (float) getHeight(), false));
    g.fillRoundedRectangle (frame, 18.0f);
    g.setColour (juce::Colour (0xffd5d9dc));
    g.drawRoundedRectangle (frame, 18.0f, 2.0f);
    g.setColour (gold.withAlpha (0.8f));
    g.drawRoundedRectangle (frame.reduced (5.0f), 14.0f, 1.4f);
    g.setColour (juce::Colour (0xff0c131b));
    g.fillRoundedRectangle (getLocalBounds().reduced (18).toFloat(), 13.0f);
    g.setColour (gold.withAlpha (0.55f));
    g.drawRoundedRectangle (getLocalBounds().reduced (18).toFloat(), 13.0f, 1.0f);
    g.setColour (juce::Colour (0xfff3f6f9));
    g.setFont (juce::Font (27.0f, juce::Font::bold));
    g.drawText ("SMART", 38, 22, 130, 42, juce::Justification::centredLeft);
    g.setColour (gold);
    g.drawText ("ARRANGER", 178, 22, 205, 42, juce::Justification::centredLeft);
    g.setColour (muted.withAlpha (0.18f));
    for (int y = 118; y < 138; y += 4)
        g.drawHorizontalLine (y, 32.0f, (float) getWidth() - 32.0f);
}

void SmartArrangerStandaloneComponent::resized()
{
    if (showArranger && editor != nullptr && editor->getHeight() > 0)
        preferredArrangerHeight = editor->getHeight();

    int preferredPartsHeight = 0;
    if (showStyleParts)
    {
        const auto text = partsList.getText();
        int lineCount = 1;
        for (const auto character : text)
            if (character == '\n')
                ++lineCount;
        const int lineHeight = juce::roundToInt (partsList.getFont().getHeight()) + 1;
        // The TextEditor already draws its border. Size it to its actual lines
        // instead of keeping a fixed-height panel with a large blank tail.
        preferredPartsHeight = 26 + lineCount * lineHeight + 8;
    }

    constexpr int sectionGap = 12;
    const int visibleSectionCount = static_cast<int> (showStyleParts)
        + static_cast<int> (showMidiActivity) + static_cast<int> (showArranger);
    const int gapsHeight = juce::jmax (0, visibleSectionCount - 1) * sectionGap;
    const int naturalRequiredHeight = 172 + preferredPartsHeight
        + (showMidiActivity ? 24 + 48 : 0)
        + (showArranger ? preferredArrangerHeight : 0) + gapsHeight;
    // Keep the window on-screen. If all visible sections cannot fit, the
    // arranger viewport below provides scrolling instead of clipping the
    // Style Selection panel at the bottom edge.
    const int maxContentHeight = juce::jmax (600, getParentMonitorArea().getHeight() - 100);
    const int requiredHeight = juce::jmin (naturalRequiredHeight, maxContentHeight);
    if (lastRequestedContentHeight != requiredHeight)
    {
        lastRequestedContentHeight = requiredHeight;
        if (onPreferredHeightChanged != nullptr)
            onPreferredHeightChanged (requiredHeight);
    }

    auto area = getLocalBounds().reduced (30);
    area.removeFromTop (112);
    const int rowY = 74;
    inputLabel.setBounds (30, rowY + 6, 56, 24);
    inputSelector.setBounds (88, rowY, 236, 36);
    midiInLamp.setBounds (324, rowY + 3, 22, 30);
    midiInActivity.setBounds (347, rowY + 5, 84, 26);
    midiThruButton.setBounds (436, rowY, 116, 36);
    midiThruLamp.setBounds (553, rowY + 3, 22, 30);
    outputLabel.setBounds (580, rowY + 5, 58, 26);
    outputSelector.setBounds (638, rowY, 165, 36);
    midiOutLamp.setBounds (805, rowY + 3, 22, 30);
    midiOutActivity.setBounds (827, rowY + 5, 62, 26);
    panicButton.setBounds (getWidth() - 180, rowY, 150, 36);
    outputStatus.setBounds (juce::Rectangle<int> (890, rowY + 4,
        juce::jmax (0, getWidth() - 180 - 890), 28));
    if (showStyleParts)
    {
        auto partsArea = area.removeFromTop (preferredPartsHeight);
        partsHeader.setBounds (partsArea.removeFromTop (26));
        partsList.setBounds (partsArea.reduced (0, 2));
        if (showMidiActivity || showArranger)
            area.removeFromTop (sectionGap);
    }
    else
    {
        partsHeader.setBounds ({ });
        partsList.setBounds ({ });
    }
    if (showMidiActivity)
    {
        activityHeader.setBounds (area.removeFromTop (24));
        auto ledRow = area.removeFromTop (48);
        const int cellWidth = ledRow.getWidth() / 16;
        for (int channel = 0; channel < 16; ++channel)
        {
            auto cell = juce::Rectangle<int> (ledRow.getX() + channel * cellWidth,
                                              ledRow.getY(), cellWidth - 4, 24);
            channelButtons[(size_t) channel].setBounds (cell);
            channelCounters[(size_t) channel].setBounds (cell.withY (cell.getBottom()).withHeight (19));
        }
        if (showArranger)
            area.removeFromTop (sectionGap);
    }
    else
    {
        activityHeader.setBounds ({ });
        for (int channel = 0; channel < 16; ++channel)
        {
            channelButtons[(size_t) channel].setBounds ({ });
            channelCounters[(size_t) channel].setBounds ({ });
        }
    }
    if (showArranger)
    {
        arrangerViewport.setBounds (area);
        editor->setBounds (0, 0, arrangerViewport.getWidth(), preferredArrangerHeight);
    }
    else
        arrangerViewport.setBounds ({ });
}

void SmartArrangerStandaloneComponent::childBoundsChanged (juce::Component* child)
{
    if (showArranger && child == editor.get() && editor != nullptr
        && preferredArrangerHeight != editor->getHeight())
    {
        preferredArrangerHeight = editor->getHeight();
        resized();
    }
}

void SmartArrangerStandaloneComponent::refreshContentHeight()
{
    lastRequestedContentHeight = -1;
    resized();
}

void SmartArrangerStandaloneComponent::timerCallback()
{
    followSystemDefaultAudioOutput();
    if (showArranger && editor != nullptr && editor->getHeight() > 0
        && preferredArrangerHeight != editor->getHeight())
    {
        preferredArrangerHeight = editor->getHeight();
        lastRequestedContentHeight = -1;
        resized();
    }
    const auto path = processor.getStoredStylePath();
    if (path.isNotEmpty() && path != lastInspectedStylePath)
    {
        updateStyleChannels (juce::File (path));
        if (settingsFile->getBoolValue ("rememberLastStyle", true))
            settingsFile->setValue ("lastStylePath", path);
        else
            settingsFile->removeValue ("lastStylePath");
        settingsFile->saveIfNeeded();
        resized();
    }
    updateChannelActivity();
    midiInActivity.setText (juce::String::formatted ("IN %llu",
        static_cast<unsigned long long> (midiInNoteOnCount.load (std::memory_order_relaxed))),
        juce::dontSendNotification);
    midiOutActivity.setText (juce::String::formatted ("OUT: %llu",
        static_cast<unsigned long long> (midiOutEventCount.load (std::memory_order_relaxed))),
        juce::dontSendNotification);
    midiInLamp.setLit (midiInPulse.exchange (false));
    midiOutLamp.setLit (midiOutPulse.exchange (false));
    const bool thruEnabled = midiThruButton.getToggleState();
    midiThruLamp.setLit (thruEnabled && midiThruPulse.exchange (false), juce::Colour (0xffffc45a));
    if (! thruEnabled)
        midiThruPulse.store (false, std::memory_order_relaxed);
    if (midiOutput != nullptr)
        outputStatus.setText (juce::String::formatted (
            "Ready | run %s | ticks %llu | calls %llu | scan %llu | notes %llu | mask %llu | sent %llu",
            processor.isAccompanimentEnabled() ? "ON" : "OFF",
            static_cast<unsigned long long> (audioCallbackCount.load (std::memory_order_relaxed)),
            static_cast<unsigned long long> (processor.getStyleRendererCallCount()),
            static_cast<unsigned long long> (processor.getStyleScannedEventCount()),
            static_cast<unsigned long long> (processor.getStyleNoteOnCount()),
            static_cast<unsigned long long> (processor.getStyleMaskRejectedCount()),
            static_cast<unsigned long long> (midiOutEventCount.load (std::memory_order_relaxed))),
            juce::dontSendNotification);
    if (auto* retired = retiredSoundFont.exchange (nullptr, std::memory_order_acq_rel))
        tsf_close (retired);
}

void SmartArrangerStandaloneComponent::followSystemDefaultAudioOutput()
{
    auto* deviceType = deviceManager.getCurrentDeviceTypeObject();
    if (deviceType == nullptr)
        return;

    const auto names = deviceType->getDeviceNames (false);
    const int defaultIndex = deviceType->getDefaultDeviceIndex (false);
    if (! juce::isPositiveAndBelow (defaultIndex, names.size()))
        return;

    const auto defaultName = names[defaultIndex];
    if (defaultName.isEmpty())
        return;

    auto* currentDevice = deviceManager.getCurrentAudioDevice();
    if (currentDevice != nullptr && currentDevice->getName() == defaultName)
    {
        lastSystemDefaultAudioOutput = defaultName;
        return;
    }

    // CoreAudio may temporarily have no current device during a headphone
    // hot-swap. Retry the default target, with a short delay after failures.
    const auto now = juce::Time::getMillisecondCounter();
    if (defaultName == lastSystemDefaultAudioOutput
        && now - lastAudioOutputSwitchAttemptMs < 1000u)
        return;
    lastAudioOutputSwitchAttemptMs = now;

    auto setup = deviceManager.getAudioDeviceSetup();
    setup.outputDeviceName = defaultName;
    const auto error = deviceManager.setAudioDeviceSetup (setup, false);
    currentDevice = deviceManager.getCurrentAudioDevice();
    if (error.isEmpty() && currentDevice != nullptr && currentDevice->getName() == defaultName)
        lastSystemDefaultAudioOutput = defaultName;
}

void SmartArrangerStandaloneComponent::showSettings()
{
    auto panelSafe = std::make_shared<juce::Component::SafePointer<SoundFontSettingsPanel>>();
    auto* panel = new SoundFontSettingsPanel (
        internalSoundsEnabled.load (std::memory_order_relaxed),
        settingsFile->getBoolValue ("rememberLastStyle", true),
        keyboardSplitPoint.load (std::memory_order_relaxed),
        settingsFile->getValue ("soundFontPath"),
        [this, panelSafe] (bool enabled)
        {
            internalSoundsEnabled.store (enabled, std::memory_order_relaxed);
            settingsFile->setValue ("internalSoundsEnabled", enabled);
            settingsFile->saveIfNeeded();
            if (auto* settings = panelSafe->getComponent())
                settings->setStatus (enabled ? "Internal SF2 playback is on." : "Internal SF2 playback is off.");
        },
        [this] (bool remember)
        {
            settingsFile->setValue ("rememberLastStyle", remember);
            if (! remember)
                settingsFile->removeValue ("lastStylePath");
            settingsFile->saveIfNeeded();
        },
        [this] (int midiNote)
        {
            const int split = juce::jlimit (24, 84, midiNote);
            keyboardSplitPoint.store (split, std::memory_order_relaxed);
            processor.setKeyboardSplitPoint (split);
            settingsFile->setValue ("keyboardSplitPoint", split);
            settingsFile->saveIfNeeded();
        },
        [this, panelSafe]
        {
            soundFontChooser = std::make_unique<juce::FileChooser> (
                "Choose a GM-compatible SoundFont", juce::File(), "*.sf2;*.SF2");
            soundFontChooser->launchAsync (juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles,
                [this, panelSafe] (const juce::FileChooser& chooser)
                {
                    const auto file = chooser.getResult();
                    if (file.existsAsFile() && file.hasFileExtension ("sf2"))
                    {
                        settingsFile->setValue ("soundFontPath", file.getFullPathName());
                        settingsFile->saveIfNeeded();
                        if (auto* settingsPanel = panelSafe->getComponent())
                        {
                            settingsPanel->setSoundFontPath (file.getFullPathName());
                            settingsPanel->setStatus ("Loading SoundFont…");
                        }
                        loadSoundFont (file);
                    }
                });
        });
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Smart Arranger Settings";
    options.dialogBackgroundColour = juce::Colour (0xff171b20);
    options.content.setOwned (panel);
    *panelSafe = panel;
    settingsPanel = panel;
    options.componentToCentreAround = this;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

bool SmartArrangerStandaloneComponent::isViewSectionVisible (int sectionId) const
{
    switch (sectionId)
    {
        case 110: return showMidiControls;
        case 111: return showStyleParts;
        case 112: return showMidiActivity;
        case 113: return showArranger;
        default:  return false;
    }
}

void SmartArrangerStandaloneComponent::handleViewCommand (int commandId)
{
    if (commandId == 100)
    {
        viewMode = 100;
        showMidiControls = showStyleParts = showMidiActivity = showArranger = true;
    }
    else if (commandId == 101 || commandId == 102)
    {
        viewMode = commandId;
        showMidiControls = showStyleParts = true;
        showMidiActivity = showArranger = false;
    }
    else if (commandId >= 110 && commandId <= 113)
    {
        viewMode = 0;
        switch (commandId)
        {
            case 110: showMidiControls = ! showMidiControls; break;
            case 111: showStyleParts = ! showStyleParts; break;
            case 112: showMidiActivity = ! showMidiActivity; break;
            case 113: showArranger = ! showArranger; break;
            default: break;
        }
    }
    else
        return;

    settingsFile->setValue ("viewMode", viewMode);
    settingsFile->setValue ("viewShowMidiControls", showMidiControls);
    settingsFile->setValue ("viewShowStyleParts", showStyleParts);
    settingsFile->setValue ("viewShowMidiActivity", showMidiActivity);
    settingsFile->setValue ("viewShowArranger", showArranger);
    settingsFile->saveIfNeeded();

    refreshViewContent();
    resized();
}

void SmartArrangerStandaloneComponent::refreshViewContent()
{
    inputLabel.setVisible (showMidiControls);
    inputSelector.setVisible (showMidiControls);
    midiInLamp.setVisible (showMidiControls);
    midiInActivity.setVisible (showMidiControls);
    midiThruButton.setVisible (showMidiControls);
    midiThruLamp.setVisible (showMidiControls);
    outputLabel.setVisible (showMidiControls);
    outputSelector.setVisible (showMidiControls);
    midiOutLamp.setVisible (showMidiControls);
    midiOutActivity.setVisible (showMidiControls);
    outputStatus.setVisible (showMidiControls);
    panicButton.setVisible (showMidiControls);
    partsHeader.setVisible (showStyleParts);
    partsList.setVisible (showStyleParts);
    activityHeader.setVisible (showMidiActivity);
    for (int channel = 0; channel < 16; ++channel)
    {
        channelButtons[(size_t) channel].setVisible (showMidiActivity);
        channelCounters[(size_t) channel].setVisible (showMidiActivity);
    }
    editor->setVisible (showArranger);
    arrangerViewport.setVisible (showArranger);

    if (viewMode == 102)
    {
        partsHeader.setText ("INSTRUMENT LIST - LOADED SOUNDFONT", juce::dontSendNotification);
        juce::String text ("BANK   PC   INSTRUMENT\n");
        text << "------------------------------------------------------------\n";
        if (soundFontPresets.empty())
            text << "No SoundFont loaded. Choose an SF2 bank in Settings.\n";
        else
        {
            constexpr int columnWidth = 42;
            const int columns = juce::jlimit (1, 3, partsList.getWidth() / (columnWidth * 8));
            const int rows = (int) ((soundFontPresets.size() + (size_t) columns - 1u)
                                    / (size_t) columns);
            for (int row = 0; row < rows; ++row)
            {
                for (int column = 0; column < columns; ++column)
                {
                    const auto index = (size_t) column * (size_t) rows + (size_t) row;
                    if (index >= soundFontPresets.size())
                        continue;
                    const auto& preset = soundFontPresets[index];
                    auto entry = juce::String (preset.bank).paddedLeft (' ', 4) + "  "
                        + juce::String (preset.program + 1).paddedLeft (' ', 3) + "  "
                        + preset.name;
                    if (entry.length() > columnWidth)
                        entry = entry.substring (0, columnWidth - 1) + "…";
                    text << entry.paddedRight (' ', columnWidth);
                }
                text << "\n";
            }
        }
        partsList.setText (text.trimEnd(), juce::dontSendNotification);
    }
    else
    {
        partsHeader.setText ("STYLE PARTS - MIDI CHANNEL AND INSTRUMENT", juce::dontSendNotification);
        if (lastInspectedStylePath.isNotEmpty())
            updateStyleChannels (juce::File (lastInspectedStylePath));
    }
}

void SmartArrangerStandaloneComponent::loadSoundFont (const juce::File& file)
{
    if (file.hasFileExtension ("sf2") == false || ! file.existsAsFile())
        return;
    auto* candidate = tsf_load_filename (file.getFullPathName().toRawUTF8());
    if (candidate == nullptr)
    {
        if (auto* settings = dynamic_cast<SoundFontSettingsPanel*> (settingsPanel.getComponent()))
            settings->setStatus ("Could not load this SF2 file.");
        outputStatus.setText ("Could not load SF2 file", juce::dontSendNotification);
        outputStatus.setColour (juce::Label::textColourId, juce::Colour (0xffff8c83));
        return;
    }
    tsf_set_output (candidate, TSF_STEREO_INTERLEAVED, (int) currentSampleRate, -6.0f);
    tsf_set_max_voices (candidate, 512);
    auto presets = readSoundFontPresets (file);
    for (auto& preset : presets)
        preset.presetIndex = tsf_get_presetindex (candidate, preset.bank, preset.program);
    soundFontPresets = std::move (presets);
    if (lastInspectedStylePath.isNotEmpty())
        updateStyleChannels (juce::File (lastInspectedStylePath));
    for (int channel = 0; channel < 16; ++channel)
        tsf_channel_set_presetnumber (candidate, channel, 0, channel == 9 ? 1 : 0);
    const int availableGMPrograms = [&]
    {
        int available = 0;
        for (int program = 0; program < 128; ++program)
            available += tsf_get_presetindex (candidate, 0, program) >= 0 ? 1 : 0;
        return available;
    }();
    if (auto* oldPending = pendingSoundFont.exchange (candidate, std::memory_order_acq_rel))
        tsf_close (oldPending);
    if (auto* settings = dynamic_cast<SoundFontSettingsPanel*> (settingsPanel.getComponent()))
        settings->setStatus ("Loaded. GM bank 0: " + juce::String (availableGMPrograms) + "/128 programs.");
}

void SmartArrangerStandaloneComponent::renderInternalSound (const juce::AudioSourceChannelInfo& info,
                                                              const juce::MidiBuffer& midi)
{
    if (retiredSoundFont.load (std::memory_order_acquire) == nullptr)
        if (auto* replacement = pendingSoundFont.exchange (nullptr, std::memory_order_acq_rel))
        {
            auto* old = activeSoundFont;
            activeSoundFont = replacement;
            if (old != nullptr)
                retiredSoundFont.store (old, std::memory_order_release);
        }

    const bool enabled = internalSoundsEnabled.load (std::memory_order_relaxed);
    if (activeSoundFont != nullptr)
        for (int channel = 0; channel < 16; ++channel)
            if (pendingInternalSoundOff[(size_t) channel].exchange (false, std::memory_order_relaxed))
                tsf_channel_sounds_off_all (activeSoundFont, channel);
    if (activeSoundFont == nullptr || ! enabled)
    {
        if (activeSoundFont != nullptr && wasInternalSoundEnabled && ! enabled)
            for (int channel = 0; channel < 16; ++channel)
                tsf_channel_sounds_off_all (activeSoundFont, channel);
        wasInternalSoundEnabled = enabled;
        return;
    }
    wasInternalSoundEnabled = true;

    auto* output = info.buffer;
    if (output == nullptr || soundFontScratch.size() < 2)
        return;
    const int scratchFrames = (int) (soundFontScratch.size() / 2u);
    int renderedUntil = 0;
    auto renderRange = [&] (int begin, int length)
    {
        int offset = begin;
        int remaining = length;
        while (remaining > 0)
        {
            const int frames = juce::jmin (remaining, scratchFrames);
            tsf_render_float (activeSoundFont, soundFontScratch.data(), frames, 0);
            const int destination = info.startSample + offset;
            const int channels = output->getNumChannels();
            for (int sample = 0; sample < frames; ++sample)
            {
                const float left = soundFontScratch[(size_t) sample * 2u];
                const float right = soundFontScratch[(size_t) sample * 2u + 1u];
                if (channels > 0) output->addSample (0, destination + sample, left);
                if (channels > 1) output->addSample (1, destination + sample, right);
            }
            offset += frames;
            remaining -= frames;
        }
    };
    for (const auto metadata : midi)
    {
        const auto& message = metadata.getMessage();
        const int position = juce::jlimit (renderedUntil, info.numSamples,
                                            metadata.samplePosition);
        renderRange (renderedUntil, position - renderedUntil);
        renderedUntil = position;
        const int channel = message.getChannel() - 1;
        if (channel < 0 || channel >= 16)
            continue;
        if (message.isNoteOn())
            tsf_channel_note_on (activeSoundFont, channel, message.getNoteNumber(),
                                  message.getFloatVelocity());
        else if (message.isNoteOff())
            tsf_channel_note_off (activeSoundFont, channel, message.getNoteNumber());
        else if (message.isProgramChange())
        {
            const int program = message.getProgramChangeNumber();
            const int bank = soundFontBankMsb[(size_t) channel] * 128
                           + soundFontBankLsb[(size_t) channel];
            int preset = -1;
            int resolvedBank = bank;
            bool usedFallback = false;

            // Yamaha XG percussion parts commonly use Bank Select MSB 127.
            // A number of XG SoundFonts store those kits as bank 127, while
            // GM/GS SoundFonts use bank 128. TinySoundFont's generic drum
            // fallback assumes bank 128 and otherwise falls back to a melodic
            // GM patch (which can make a drum part sound like a synth lead).
            const bool xgDrumBank = soundFontBankMsb[(size_t) channel] == 127;
            const bool percussionChannel = channel == 9;
            // GM channel 10 is percussion even when a style omits Bank Select
            // and sends bank 0 / PC 1. Prefer a SoundFont drum kit over the
            // exact bank-0 melodic patch (often Piano).
            const bool defaultGmPercussion = percussionChannel && bank == 0;
            if (defaultGmPercussion || xgDrumBank)
            {
                const int drumBanks[] = { xgDrumBank ? 127 : 128,
                                          xgDrumBank ? 128 : 127 };
                for (const int drumBank : drumBanks)
                {
                    preset = tsf_get_presetindex (activeSoundFont, drumBank, program);
                    if (preset >= 0)
                    {
                        resolvedBank = drumBank;
                        usedFallback = true;
                        break;
                    }
                }
                // If the style's PC is a melodic GM number but the selected
                // XG bank contains only drum kits, use its standard kit.
                if (preset < 0)
                    for (const int drumBank : drumBanks)
                    {
                        preset = tsf_get_presetindex (activeSoundFont, drumBank, 0);
                        if (preset >= 0)
                        {
                            resolvedBank = drumBank;
                            usedFallback = true;
                            break;
                        }
                    }
            }
            if (preset < 0 && ! defaultGmPercussion)
                preset = tsf_get_presetindex (activeSoundFont, bank, program);
            if (preset < 0)
            {
                if (! defaultGmPercussion)
                    preset = tsf_get_presetindex (activeSoundFont, 0, program);
                if (preset >= 0)
                {
                    resolvedBank = 0;
                    usedFallback = bank != 0;
                }
            }

            if (preset >= 0)
            {
                tsf_channel_set_presetindex (activeSoundFont, channel, preset);
                activeSoundFontBanks[(size_t) channel].store (resolvedBank, std::memory_order_relaxed);
                activeSoundFontPrograms[(size_t) channel].store (
                    tsf_channel_get_preset_number (activeSoundFont, channel), std::memory_order_relaxed);
                activeSoundFontFallbacks[(size_t) channel].store (usedFallback,
                                                                  std::memory_order_relaxed);
            }
            else
            {
                tsf_channel_set_presetnumber (activeSoundFont, channel, program,
                                              percussionChannel ? 1 : 0);
                activeSoundFontBanks[(size_t) channel].store (bank, std::memory_order_relaxed);
                activeSoundFontPrograms[(size_t) channel].store (program, std::memory_order_relaxed);
                activeSoundFontFallbacks[(size_t) channel].store (true, std::memory_order_relaxed);
            }
        }
        else if (message.isController())
        {
            const int controller = message.getControllerNumber();
            const int value = message.getControllerValue();
            if (controller == 0) soundFontBankMsb[(size_t) channel] = value;
            else if (controller == 32) soundFontBankLsb[(size_t) channel] = value;
            else if (controller == 121)
            {
                soundFontBankMsb[(size_t) channel] = 0;
                soundFontBankLsb[(size_t) channel] = 0;
            }
            tsf_channel_midi_control (activeSoundFont, channel, controller, value);
        }
        else if (message.isPitchWheel())
            tsf_channel_set_pitchwheel (activeSoundFont, channel, message.getPitchWheelValue());
        else if (message.isChannelPressure())
            tsf_channel_midi_control (activeSoundFont, channel, 11, message.getChannelPressureValue());
        else if (message.isAftertouch())
            tsf_channel_midi_control (activeSoundFont, channel, 11, message.getAfterTouchValue());
    }
    renderRange (renderedUntil, info.numSamples - renderedUntil);
}

void SmartArrangerStandaloneComponent::handleIncomingMidiMessage (juce::MidiInput*,
                                                                   const juce::MidiMessage& message)
{
    midiCollector.addMessageToQueue (message);
    midiInPulse.store (true, std::memory_order_relaxed);
    if (message.isNoteOn())
    {
        midiInNoteOnCount.fetch_add (1, std::memory_order_relaxed);
        if (midiThruEnabled.load (std::memory_order_relaxed))
            midiThruPulse.store (true, std::memory_order_relaxed);
    }
}

void SmartArrangerStandaloneComponent::refreshInputs()
{
    inputSelector.clear (juce::dontSendNotification);
    inputSelector.addItem ("No MIDI keyboard (style plays in C)", 1);
    const auto devices = juce::MidiInput::getAvailableDevices();
    int itemId = 2;
    for (int i = 0; i < devices.size(); ++i)
        if (! devices[i].name.containsIgnoreCase ("Smart Arranger MIDI Out"))
            inputSelector.addItem (devices[i].name, itemId++);
    inputSelector.setSelectedId (1, juce::dontSendNotification);
}

void SmartArrangerStandaloneComponent::refreshOutputs()
{
    const auto selectedIdentifier = selectedMidiOutputId == 1 || selectedMidiOutputId - 2 >= midiOutputDevices.size()
        ? juce::String ("SMART_ARRANGER_VIRTUAL")
        : midiOutputDevices.getReference (selectedMidiOutputId - 2).identifier;
    const auto availableDevices = juce::MidiOutput::getAvailableDevices();
    midiOutputDevices.clear();
    for (const auto& device : availableDevices)
        if (! device.name.containsIgnoreCase ("Smart Arranger MIDI Out"))
            midiOutputDevices.add (device);
    outputSelector.clear (juce::dontSendNotification);
    outputSelector.addItem ("Logic (Smart Arranger)", 1);
    int selectedId = 1;
    for (int i = 0; i < midiOutputDevices.size(); ++i)
    {
        const auto& device = midiOutputDevices.getReference (i);
        outputSelector.addItem (device.name, i + 2);
        if (device.identifier == selectedIdentifier)
            selectedId = i + 2;
    }
    outputSelector.setSelectedId (selectedId, juce::dontSendNotification);
    selectedMidiOutputId = selectedId;
}

void SmartArrangerStandaloneComponent::selectOutput (int comboId)
{
    if (comboId < 1 || comboId > midiOutputDevices.size() + 1)
        return;
    if (comboId != selectedMidiOutputId)
        sendAllNotesOff();

    if (comboId == 1)
    {
        const juce::ScopedLock lock (midiOutputLock);
        if (selectedMidiOutput != nullptr)
            selectedMidiOutput->stopBackgroundThread();
        selectedMidiOutput.reset();
        selectedMidiOutputId = 1;
        settingsFile->setValue ("midiOutputIdentifier", "SMART_ARRANGER_VIRTUAL");
        settingsFile->saveIfNeeded();
        outputStatus.setText (midiOutput != nullptr ? "MIDI OUT: Smart Arranger virtual port"
                                                   : "Smart Arranger MIDI output unavailable",
                             juce::dontSendNotification);
        outputStatus.setColour (juce::Label::textColourId,
            midiOutput != nullptr ? juce::Colour (0xff8fdb99) : juce::Colour (0xffff8c83));
        return;
    }

    const auto device = midiOutputDevices.getReference (comboId - 2);
    auto candidate = juce::MidiOutput::openDevice (device.identifier);
    if (candidate == nullptr)
    {
        outputStatus.setText ("Could not open MIDI output: " + device.name, juce::dontSendNotification);
        outputStatus.setColour (juce::Label::textColourId, juce::Colour (0xffff8c83));
        outputSelector.setSelectedId (selectedMidiOutputId, juce::dontSendNotification);
        return;
    }

    {
        const juce::ScopedLock lock (midiOutputLock);
        if (selectedMidiOutput != nullptr)
            selectedMidiOutput->stopBackgroundThread();
        selectedMidiOutput = std::move (candidate);
        selectedMidiOutput->startBackgroundThread();
        selectedMidiOutputId = comboId;
    }
    settingsFile->setValue ("midiOutputIdentifier", device.identifier);
    settingsFile->saveIfNeeded();
    outputStatus.setText ("MIDI OUT: " + device.name, juce::dontSendNotification);
    outputStatus.setColour (juce::Label::textColourId, juce::Colour (0xff8fdb99));
}

void SmartArrangerStandaloneComponent::selectInput (int selectedItemId)
{
    if (midiInput != nullptr)
        midiInput->stop();
    midiInput.reset();
    const auto devices = juce::MidiInput::getAvailableDevices();
    if (selectedItemId <= 1)
        return;
    int visibleIndex = 0;
    const juce::MidiDeviceInfo* selected = nullptr;
    for (const auto& device : devices)
    {
        if (device.name.containsIgnoreCase ("Smart Arranger MIDI Out"))
            continue;
        if (visibleIndex++ == selectedItemId - 2)
        {
            selected = &device;
            break;
        }
    }
    if (selected == nullptr)
        return;
    midiInput = juce::MidiInput::openDevice (selected->identifier, this);
    if (midiInput != nullptr)
        midiInput->start();
}

void SmartArrangerStandaloneComponent::updateStyleChannels (const juce::File& file)
{
    SmartArrangerStyleInfo style;
    juce::String error;
    if (! file.existsAsFile() || ! SmartArrangerStyleFile::inspect (file, style, error))
        return;
    lastInspectedStylePath = file.getFullPathName();
    if (viewMode == 102)
    {
        refreshViewContent();
        return;
    }

    std::array<int, 16> programs;
    programs.fill (-1);
    std::array<juce::String, 16> partNames;
    std::array<uint16_t, 16> mappedChannels {};
    for (const auto& segment : style.casmSegments)
        for (const auto& record : segment.records)
            if (record.hasCommonChannelFields && record.sourceChannel < 16)
            {
                auto& part = partNames[(size_t) record.sourceChannel];
                if (part.isEmpty())
                    part = nicePartName (record.partLabel);
                if (record.destinationChannel < 16)
                    mappedChannels[(size_t) record.sourceChannel]
                        |= static_cast<uint16_t> (1u << record.destinationChannel);
            }
    std::array<int, 16> bankMsb {}, bankLsb {};
    std::array<int, 16> banks {};
    for (const auto& event : style.events)
    {
        const int source = event.status & 0x0fu;
        const int command = event.status & 0xf0u;
        if (command == 0xb0u && event.data1 == 0)
            bankMsb[(size_t) source] = event.data2;
        else if (command == 0xb0u && event.data1 == 32)
            bankLsb[(size_t) source] = event.data2;
        else if (command == 0xc0u)
        {
            programs[(size_t) source] = event.data1;
            banks[(size_t) source] = bankMsb[(size_t) source] * 128 + bankLsb[(size_t) source];
        }
    }

    juce::StringArray entries;
    for (int source = 0; source < 16; ++source)
    {
        if ((style.usedChannels & (1u << source)) == 0)
            continue;
        const auto map = mappedChannels[(size_t) source];
        int mappedCount = 0;
        int mappedOutput = source;
        for (int destination = 0; destination < 16; ++destination)
            if ((map & (1u << destination)) != 0)
            {
                ++mappedCount;
                mappedOutput = destination;
            }
        const int output = mappedCount == 1 ? mappedOutput : source;
        auto part = partNames[(size_t) source];
        if (part.isEmpty()) part = "Unspecified part";
        auto instrument = juce::String();
        if (programs[(size_t) source] >= 0)
        {
            const auto bank = banks[(size_t) source];
            const bool defaultGmPercussion = output == 9 && bank == 0;
            if (defaultGmPercussion)
            {
                // Channel 10 is percussion in GM. Some Yamaha styles omit
                // drum Bank Select and send a nominal PC 1, which must not be
                // presented as a Piano preset just because bank 0 has one.
                for (const int drumBank : { 128, 127 })
                {
                    for (const int drumProgram : { programs[(size_t) source], 0 })
                    {
                        for (const auto& preset : soundFontPresets)
                            if (preset.bank == drumBank && preset.program == drumProgram)
                            {
                                instrument = preset.name;
                                break;
                            }
                        if (instrument.isNotEmpty())
                            break;
                    }
                    if (instrument.isNotEmpty())
                        break;
                }
                if (instrument.isEmpty())
                    instrument = "Standard Drum Kit";
            }
            else
                for (const auto& preset : soundFontPresets)
                    if (preset.bank == bank && preset.program == programs[(size_t) source])
                    {
                        instrument = preset.name;
                        break;
                    }
            if (instrument.isEmpty() && bank == 0)
                instrument = defaultGmPercussion ? "Standard Drum Kit"
                                                 : gmProgramName (programs[(size_t) source]);
            if (instrument.isEmpty())
                instrument = "Bank " + juce::String (bank) + ", PC "
                    + juce::String (programs[(size_t) source] + 1);
        }
        else
            instrument = "Not specified in MIDI data";
        auto entry = "MIDI " + juce::String (output + 1).paddedLeft (' ', 2)
            + "  " + part + "  " + instrument;
        constexpr int columnWidth = 54;
        if (entry.length() > columnWidth)
            entry = entry.substring (0, columnWidth - 1) + "…";
        entries.add (entry);
    }
    juce::String text ("OUTPUT CHANNEL / STYLE PART / INSTRUMENT\n");
    text << "------------------------------------------------------------\n";
    if (entries.isEmpty())
        text << "No channel events were found in this style.\n";
    else
    {
        constexpr int columnWidth = 56;
        const int columns = juce::jlimit (1, 4, partsList.getWidth() / (columnWidth * 8));
        const int rows = (entries.size() + columns - 1) / columns;
        for (int row = 0; row < rows; ++row)
        {
            for (int column = 0; column < columns; ++column)
            {
                const int index = column * rows + row;
                if (index < entries.size())
                    text << entries[index].paddedRight (' ', columnWidth);
            }
            text << "\n";
        }
    }
    text << "\nNames use the loaded SF2 bank when its Bank Select + Program Change match;"
         << " otherwise GM names are shown for bank 0.";
    text = text.trimEnd();
    partsList.setText (text, juce::dontSendNotification);
}

void SmartArrangerStandaloneComponent::updateChannelActivity()
{
    for (int channel = 0; channel < 16; ++channel)
    {
        const auto count = noteOnCounts[(size_t) channel].load (std::memory_order_relaxed);
        const bool active = channelActivityPulse[(size_t) channel].exchange (false,
                                                                            std::memory_order_relaxed);
        channelCounters[(size_t) channel].setText (juce::String (count), juce::dontSendNotification);
        const bool isMuted = channelMuted[(size_t) channel].load (std::memory_order_relaxed);
        auto& button = channelButtons[(size_t) channel];
        button.setColour (juce::TextButton::buttonColourId,
            isMuted ? juce::Colour (0xff923e37)
                    : active ? juce::Colour (0xffa97118) : juce::Colour (0xff252a30));
        button.setColour (juce::TextButton::textColourOffId,
            active || isMuted ? juce::Colour (0xffffe2a0) : muted);
        const bool manual = manualInstrumentEnabled[(size_t) channel].load (std::memory_order_relaxed);
        button.setTooltip (juce::String (isMuted ? "Muted" : "Active") + " MIDI channel "
                           + juce::String (channel + 1) + ". Click to "
                           + (isMuted ? "unmute" : "mute")
                           + ". Right-click to assign instrument. Assignment: "
                           + (manual ? "manual" : "from style") + ".");
        const int sf2Program = activeSoundFontPrograms[(size_t) channel].load (std::memory_order_relaxed);
        if (sf2Program >= 0)
        {
            const int sf2Bank = activeSoundFontBanks[(size_t) channel].load (std::memory_order_relaxed);
            const bool fallback = activeSoundFontFallbacks[(size_t) channel].load (std::memory_order_relaxed);
            auto presetName = gmProgramName (sf2Program);
            for (const auto& preset : soundFontPresets)
                if (preset.bank == sf2Bank && preset.program == sf2Program)
                {
                    presetName = preset.name;
                    break;
                }
        button.setTooltip (juce::String (isMuted ? "Muted" : "Active") + " MIDI channel "
                + juce::String (channel + 1) + ". Click to " + (isMuted ? "unmute" : "mute")
                + ". Right-click to assign instrument.\nSF2 preset: "
                + presetName + (manual ? " (manual assignment)" : "")
                + "\nSoundFont bank " + juce::String (sf2Bank)
                + (fallback ? " (fallback; requested style bank was unavailable)"
                            : " (exact style bank match)"));
        }
    }
}

void SmartArrangerStandaloneComponent::setChannelMuted (int channel, bool shouldMute)
{
    if (channel < 0 || channel >= 16)
        return;
    channelMuted[(size_t) channel].store (shouldMute, std::memory_order_relaxed);
    if (shouldMute)
    {
        pendingChannelPanic[(size_t) channel].store (true, std::memory_order_relaxed);
        pendingInternalSoundOff[(size_t) channel].store (true, std::memory_order_relaxed);
    }
    else if (manualInstrumentEnabled[(size_t) channel].load (std::memory_order_relaxed))
        pendingInstrumentChange[(size_t) channel].store (true, std::memory_order_relaxed);
    updateChannelActivity();
}

void SmartArrangerStandaloneComponent::showChannelInstrumentMenu (int channel)
{
    if (channel < 0 || channel >= 16)
        return;

    juce::PopupMenu menu;
    menu.addItem (1, "Auto — use instrument data from style",
                  true, ! manualInstrumentEnabled[(size_t) channel].load());

    juce::PopupMenu gmMenu;
    for (int program = 0; program < 128; ++program)
        gmMenu.addItem (100 + program, gmProgramName (program));
    menu.addSubMenu ("General MIDI (128 voices)", gmMenu);

    if (! soundFontPresets.empty())
    {
        std::map<int, juce::PopupMenu> banks;
        for (size_t i = 0; i < soundFontPresets.size(); ++i)
        {
            const auto& preset = soundFontPresets[i];
            const int id = 1000 + (int) i;
            const auto name = preset.name.isNotEmpty() ? preset.name
                : gmProgramName (preset.program) + " (PC " + juce::String (preset.program + 1) + ")";
            banks[preset.bank].addItem (id, name);
        }
        juce::PopupMenu sf2Menu;
        for (auto& entry : banks)
            sf2Menu.addSubMenu ("Bank " + juce::String (entry.first), entry.second);
        menu.addSubMenu ("Loaded SoundFont voices", sf2Menu);
    }
    else
        menu.addItem (false, "Load an SF2 in Settings to browse its voices");

    juce::Component::SafePointer<SmartArrangerStandaloneComponent> safeThis (this);
    juce::PopupMenu::Options options;
    options = options.withTargetComponent (&channelButtons[(size_t) channel]);
    menu.showMenuAsync (options, [safeThis, channel] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;
        if (result == 1)
        {
            safeThis->manualInstrumentEnabled[(size_t) channel].store (false, std::memory_order_relaxed);
            safeThis->pendingInstrumentChange[(size_t) channel].store (false, std::memory_order_relaxed);
            safeThis->outputStatus.setText ("MIDI channel " + juce::String (channel + 1)
                + " uses the style's instrument assignment again.", juce::dontSendNotification);
        }
        else if (result >= 100 && result < 228)
            safeThis->assignChannelInstrument (channel, 0, result - 100, true);
        else if (result >= 1000)
        {
            const size_t index = (size_t) (result - 1000);
            if (index < safeThis->soundFontPresets.size())
            {
                const auto& preset = safeThis->soundFontPresets[index];
                safeThis->assignChannelInstrument (channel, preset.bank, preset.program, true);
            }
        }
    });
}

void SmartArrangerStandaloneComponent::assignChannelInstrument (int channel, int bank,
                                                                 int program, bool manual)
{
    if (channel < 0 || channel >= 16 || program < 0 || program > 127)
        return;
    manualInstrumentEnabled[(size_t) channel].store (manual, std::memory_order_relaxed);
    manualInstrumentBank[(size_t) channel].store (juce::jlimit (0, 16383, bank),
                                                   std::memory_order_relaxed);
    manualInstrumentProgram[(size_t) channel].store (program, std::memory_order_relaxed);
    pendingInstrumentChange[(size_t) channel].store (manual, std::memory_order_relaxed);
    outputStatus.setText ("MIDI channel " + juce::String (channel + 1) + " assigned to "
        + gmProgramName (program) + " (bank " + juce::String (bank) + ").",
        juce::dontSendNotification);
}

bool SmartArrangerStandaloneComponent::keyPressed (const juce::KeyPress& key,
                                                     juce::Component* originatingComponent)
{
    if (key.getModifiers().isCommandDown() || key.getModifiers().isAltDown()
        || key.getModifiers().isCtrlDown() || key.getModifiers().isShiftDown())
        return false;

    // Leave text entry intact if a future text field is focused.
    if (dynamic_cast<juce::TextEditor*> (originatingComponent) != nullptr)
        return false;

    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        processor.setAccompanimentEnabled (! processor.isAccompanimentEnabled());
        return true;
    }

    const auto character = key.getTextCharacter();
    if (character >= '1' && character <= '9')
    {
        if (auto* arranger = dynamic_cast<SmartArrangerAudioProcessorEditor*> (editor.get()))
            return arranger->activateSectionByDisplayIndex (character - '1');
    }
    else if (character == '0')
    {
        if (auto* arranger = dynamic_cast<SmartArrangerAudioProcessorEditor*> (editor.get()))
            return arranger->activateSectionByDisplayIndex (9);
    }
    return false;
}

void SmartArrangerStandaloneComponent::sendAllNotesOff()
{
    const juce::ScopedLock lock (midiOutputLock);
    auto* activeMidiOutput = selectedMidiOutputId == 1 ? midiOutput.get() : selectedMidiOutput.get();
    if (activeMidiOutput == nullptr)
        return;
    juce::MidiBuffer panic;
    for (int channel = 1; channel <= 16; ++channel)
        panic.addEvent (juce::MidiMessage::controllerEvent (channel, 123, 0), 0);
    activeMidiOutput->sendBlockOfMessagesNow (panic);
}
