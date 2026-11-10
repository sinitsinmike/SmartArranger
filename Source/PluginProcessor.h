#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>
#include "StyleFile.h"

class SmartArrangerAudioProcessor final : public juce::AudioProcessor
{
public:
    SmartArrangerAudioProcessor();

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destination) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    int getDetectedChordRoot() const noexcept;
    int getDetectedChordQuality() const noexcept;
    int getDetectedBassNote() const noexcept;
    void setAccompanimentEnabled (bool shouldEnable) noexcept;
    bool isAccompanimentEnabled() const noexcept;
    void setMidiThruEnabled (bool shouldEnable) noexcept;
    void setTempoRate (float rate) noexcept;
    float getTempoRate() const noexcept;
    // Standalone keyboard split: notes at/below the split control harmony and
    // are removed from the outgoing stream; upper notes remain playable.
    // A negative value disables split handling for the AU plugin.
    void setKeyboardSplitPoint (int midiNote) noexcept;
    bool setLoadedStyle (const SmartArrangerStyleInfo& style, int sectionIndex,
                        juce::String& error, const juce::File& sourceFile);
    void requestStyleSection (int sectionIndex);
    juce::String getStoredStylePath() const;
    int getStoredStyleSection() const noexcept;
    int getRequestedStyleSection() const noexcept;
    bool hasStylePlaybackFinished() const noexcept;
    uint64_t getStyleNoteOnCount() const noexcept;
    uint64_t getStyleMaskRejectedCount() const noexcept;
    uint64_t getStyleScannedEventCount() const noexcept;
    uint64_t getStyleRendererCallCount() const noexcept;
    void clearLoadedStyle (bool clearSavedSelection = true) noexcept;
    juce::AudioProcessorValueTreeState& getParameterState() noexcept;

    enum ChordQuality
    {
        noChord = 0,
        major,
        minor,
        diminished,
        augmented,
        suspendedSecond,
        suspendedFourth,
        dominantSeventh,
        majorSeventh,
        minorSeventh,
        halfDiminishedSeventh,
        diminishedSeventh
    };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateDetectedChord() noexcept;
    void renderAccompaniment (juce::MidiBuffer& midi, int blockSamples) noexcept;
    bool renderLoadedStyle (juce::MidiBuffer& midi, int blockSamples) noexcept;

    struct StylePlaybackData
    {
        struct SourcePart
        {
            // MIDI source channel from the style events (zero-based 0..15).
            uint8_t sourceChannel = 0;
            // Yamaha logical Style Part destination (MIDI 9..16 encoded 0x08..0x0F).
            // 0xFF means that no validated CASM mapping was found.
            uint8_t destinationChannel = 0xff;
            juce::String partLabel;
            bool hasEventsInSection = false;
            // Keep all matching CASM records: do not assume a single rule per
            // channel or collapse different records that share a destination.
            std::vector<SmartArrangerCasmRecord> rules;
        };

        struct Section
        {
            std::vector<SmartArrangerStyleEvent> events;
            std::vector<SourcePart> sourceParts;
            uint64_t lengthTicks = 0;
            int role = 0;
        };
        std::vector<Section> sections;
        // Keep the parsed Yamaha style metadata with the same immutable
        // playback snapshot as its MIDI sections for future CASM rendering.
        std::vector<SmartArrangerCasmRecord> casmRecords;
        std::vector<SmartArrangerCasmSegment> casmSegments;
        int ticksPerQuarter = 0;
        int tempo = 120;
        int numerator = 4;
        int denominator = 4;
    };

    // MIDI events are consumed and updated on the audio thread. The editor
    // reads only the atomics, so no UI lock is taken by processBlock().
    std::array<uint8_t, 16 * 128> heldNotes {};
    std::atomic<int> detectedChordRoot { -1 };
    std::atomic<int> detectedChordQuality { noChord };
    // Keep bass-note recognition separate from chord-root recognition so
    // inversions/slash chords do not force the bass pitch to become the root.
    std::atomic<int> detectedBassNote { -1 };
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* accompanimentEnabledParameter = nullptr;
    // AudioProcessor::getSampleRate() is populated by a plugin host. The
    // standalone diagnostic drives this processor directly, so retain the
    // sample rate supplied by prepareToPlay for both paths.
    double playbackSampleRate = 0.0;
    std::atomic<bool> midiThruEnabled { true };
    std::atomic<float> tempoRate { 1.0f };
    std::atomic<int> keyboardSplitPoint { -1 };
    std::array<uint8_t, 5> activeGeneratedNotes { 255, 255, 255, 255, 255 };
    int64_t lastGeneratedStep = -1;
    std::array<StylePlaybackData, 2> stylePlaybackBuffers;
    std::array<std::atomic<int>, 2> styleReaders { 0, 0 };
    std::atomic<int> activeStyleBuffer { -1 };
    std::atomic<int> requestedStyleSection { 0 };
    std::array<uint8_t, 16 * 128> activeStyleNotes {};
    std::array<uint8_t, 16 * 128> activeStyleOutputNotes {};
    std::array<uint8_t, 16 * 128> activeStyleOutputChannels {};
    int64_t styleHarmonyBar = -1;
    int styleHarmonyRoot = 0;
    int styleHarmonyQuality = major;
    bool styleHarmonyHasRecognizedChord = false;
    double lastStyleBlockEndPpq = 0.0;
    double styleClockPpq = 0.0;
    double styleSectionStartPpq = 0.0;
    double queuedStyleSwitchPpq = -1.0;
    double fallbackClockPpq = 0.0;
    bool styleWasPlaying = false;
    int styleBufferInUse = -1;
    int activeStyleSection = -1;
    int lastRequestedStyleSection = -1;
    int queuedStyleSection = -1;
    int mainStyleSection = -1;
    int oneShotReturnSection = -1;
    bool stopAfterOneShot = false;
    std::atomic<bool> stylePlaybackFinished { false };
    std::atomic<uint64_t> styleNoteOnCount { 0 };
    std::atomic<uint64_t> styleMaskRejectedCount { 0 };
    std::atomic<uint64_t> styleScannedEventCount { 0 };
    std::atomic<uint64_t> styleRendererCallCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmartArrangerAudioProcessor)
};
