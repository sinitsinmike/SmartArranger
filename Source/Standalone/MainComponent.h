#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include <vector>

struct tsf;

class MidiStatusLed final : public juce::Component
{
public:
    void setLit (bool shouldLight, juce::Colour activeColour = juce::Colour (0xff71e58b))
    {
        if (lit == shouldLight && colour == activeColour)
            return;
        lit = shouldLight;
        colour = activeColour;
        repaint();
    }

    void paint (juce::Graphics& graphics) override
    {
        const auto dot = getLocalBounds().toFloat().reduced (5.0f);
        graphics.setColour (juce::Colour (0xff59616a));
        graphics.fillEllipse (dot.expanded (1.0f));
        graphics.setColour (lit ? colour : juce::Colour (0xff252a30));
        graphics.fillEllipse (dot);
    }

private:
    bool lit = false;
    juce::Colour colour { 0xff71e58b };
};

class ChannelMuteButton final : public juce::TextButton
{
public:
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isRightButtonDown() || event.mods.isPopupMenu())
        {
            if (onRightClick != nullptr)
                onRightClick();
            return;
        }
        juce::TextButton::mouseDown (event);
    }
};

struct SmartArrangerSoundFontPreset
{
    int bank = 0;
    int program = 0;
    int presetIndex = -1;
    juce::String name;
};

class SmartArrangerStandaloneComponent final : public juce::AudioAppComponent,
                                                private juce::MidiInputCallback,
                                                private juce::KeyListener,
                                                private juce::Timer
{
public:
    SmartArrangerStandaloneComponent();
    ~SmartArrangerStandaloneComponent() override;

    void prepareToPlay (int expectedBlockSize, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override;
    void releaseResources() override;
    void paint (juce::Graphics& graphics) override;
    void resized() override;
    void childBoundsChanged (juce::Component* child) override;
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;
    void showSettings();
    void handleViewCommand (int commandId);
    bool isViewSectionVisible (int sectionId) const;
    int getViewMode() const noexcept { return viewMode; }
    std::function<void (int)> onPreferredHeightChanged;
    void refreshContentHeight();
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

private:
    void timerCallback() override;
    void followSystemDefaultAudioOutput();
    void refreshInputs();
    void refreshOutputs();
    void selectInput (int comboIndex);
    void selectOutput (int comboId);
    void updateStyleChannels (const juce::File& file);
    void updateChannelActivity();
    void setChannelMuted (int channel, bool shouldMute);
    void showChannelInstrumentMenu (int channel);
    void assignChannelInstrument (int channel, int bank, int program, bool manual);
    void sendAllNotesOff();
    void loadSoundFont (const juce::File& file);
    void renderInternalSound (const juce::AudioSourceChannelInfo& info, const juce::MidiBuffer& midi);
    void refreshViewContent();

    SmartArrangerAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::Viewport arrangerViewport { "Style Selection" };
    juce::MidiMessageCollector midiCollector;
    std::atomic<tsf*> pendingSoundFont { nullptr };
    std::atomic<tsf*> retiredSoundFont { nullptr };
    tsf* activeSoundFont = nullptr; // audio thread only
    std::array<int, 16> soundFontBankMsb {}; // audio thread only
    std::array<int, 16> soundFontBankLsb {}; // audio thread only
    std::array<std::atomic<int>, 16> activeSoundFontBanks {};
    std::array<std::atomic<int>, 16> activeSoundFontPrograms {};
    std::array<std::atomic<bool>, 16> activeSoundFontFallbacks {};
    std::vector<float> soundFontScratch;
    std::atomic<bool> internalSoundsEnabled { false };
    std::atomic<int> keyboardSplitPoint { 48 }; // MIDI 48 = C3
    bool wasInternalSoundEnabled = false; // audio thread only
    std::unique_ptr<juce::FileChooser> soundFontChooser;
    std::unique_ptr<juce::PropertiesFile> settingsFile;
    juce::Component::SafePointer<juce::Component> settingsPanel;
    std::unique_ptr<juce::MidiInput> midiInput;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    std::unique_ptr<juce::MidiOutput> selectedMidiOutput;
    juce::CriticalSection midiOutputLock;
    juce::Array<juce::MidiDeviceInfo> midiOutputDevices;
    int selectedMidiOutputId = 1;
    double currentSampleRate = 44100.0;
    juce::MidiBuffer outgoingMidi;
    std::array<std::atomic<uint32_t>, 16> noteOnCounts {};
    std::array<std::atomic<bool>, 16> channelActivityPulse {};
    std::array<std::atomic<bool>, 16> channelMuted {};
    std::array<std::atomic<bool>, 16> pendingChannelPanic {};
    std::array<std::atomic<bool>, 16> pendingInternalSoundOff {};
    std::array<std::atomic<bool>, 16> manualInstrumentEnabled {};
    std::array<std::atomic<int>, 16> manualInstrumentBank {};
    std::array<std::atomic<int>, 16> manualInstrumentProgram {};
    std::array<std::atomic<bool>, 16> pendingInstrumentChange {};
    std::atomic<uint64_t> audioCallbackCount { 0 };
    std::atomic<uint64_t> midiInNoteOnCount { 0 };
    std::atomic<uint64_t> midiOutEventCount { 0 };
    std::atomic<bool> midiThruEnabled { false };
    std::atomic<bool> midiInPulse { false };
    std::atomic<bool> midiOutPulse { false };
    std::atomic<bool> midiThruPulse { false };
    juce::String lastInspectedStylePath;
    juce::String lastSystemDefaultAudioOutput;
    uint32_t lastAudioOutputSwitchAttemptMs = 0;
    int preferredArrangerHeight = 431;
    int lastRequestedContentHeight = -1;
    int viewMode = 100; // All
    bool showMidiControls = true;
    bool showStyleParts = true;
    bool showMidiActivity = true;
    bool showArranger = true;

    juce::Label inputLabel;
    juce::ComboBox inputSelector;
    juce::Label outputLabel;
    juce::ComboBox outputSelector;
    MidiStatusLed midiInLamp;
    juce::Label midiInActivity;
    juce::TextButton midiThruButton { "MIDI THRU OFF" };
    MidiStatusLed midiThruLamp;
    MidiStatusLed midiOutLamp;
    juce::Label midiOutActivity;
    juce::Label outputStatus;
    juce::TextButton panicButton { "ALL NOTES OFF" };
    juce::Label partsHeader;
    juce::TextEditor partsList;
    juce::Label activityHeader;
    std::vector<SmartArrangerSoundFontPreset> soundFontPresets;
    std::array<ChannelMuteButton, 16> channelButtons;
    std::array<juce::Label, 16> channelCounters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmartArrangerStandaloneComponent)
};
