#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "StyleFile.h"

class SmartArrangerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                private juce::Timer
{
public:
    explicit SmartArrangerAudioProcessorEditor (SmartArrangerAudioProcessor& processor);
    void paint (juce::Graphics& graphics) override;
    void resized() override;
    void timerCallback() override;
    bool activateSectionByDisplayIndex (int displayIndex);
    void loadStyleFile (const juce::File& file);

private:
    void chooseStyleFile();
    void activateStyleSection();
    void rebuildSectionButtons (int selectedIndex);
    void selectStyleSection (int index);

    SmartArrangerAudioProcessor& targetProcessor;
    juce::TextButton loadStyleButton { "Load Yamaha .STY" };
    juce::OwnedArray<juce::TextButton> sectionButtons;
    juce::Array<int> sectionButtonToStyleIndex;
    juce::TextButton accompanimentButton { "ACCOMPANIMENT  OFF" };
    juce::Slider tempoRateSlider;
    juce::Label tempoRateCaption;
    juce::Label tempoRateValue;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> accompanimentAttachment;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::String styleSummary { "No style loaded" };
    juce::String styleTitle { "NO STYLE LOADED" };
    juce::String styleDetails;
    juce::StringArray channelDetails;
    juce::String styleError;
    juce::String loadedStylePath;
    juce::String lastRestoreAttemptPath;
    SmartArrangerStyleInfo loadedStyleInfo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmartArrangerAudioProcessorEditor)
};
