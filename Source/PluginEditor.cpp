#include "PluginEditor.h"

#include <algorithm>

namespace
{
juce::File copyPickedUrlToLocalFile (const juce::URL& url, const juce::String& fallbackExtension)
{
#if JUCE_ANDROID
    auto document = juce::AndroidDocument::fromDocument (url);
    if (! document || ! document.getInfo().canRead())
        return {};

    auto stream = document.createInputStream();
    if (stream == nullptr)
        return {};

    juce::MemoryBlock data;
    if (stream->readIntoMemoryBlock (data) <= 0 || data.isEmpty())
        return {};

    auto name = document.getInfo().getName();
    auto extension = juce::File (name).getFileExtension();
    if (extension.isEmpty())
        extension = fallbackExtension;
    auto destination = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("SmartArranger-" + juce::String (juce::Time::getMillisecondCounter()) + extension);
    return destination.replaceWithData (data.getData(), data.getSize()) ? destination : juce::File();
#else
    return url.getLocalFile();
#endif
}
}

namespace
{
const juce::Colour background (0xff0b1118);
const juce::Colour panel (0xff171d23);
const juce::Colour panelEdge (0xff665238);
const juce::Colour muted (0xffb8aa91);
const juce::Colour cyan (0xff67b8d5);
const juce::Colour gold (0xffe9b85b);

juce::String getChordLabel (int root, int quality)
{
    if (root < 0 || root > 11)
        return "N/C";

    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F",
                                       "F#", "G", "G#", "A", "A#", "B" };
    static const char* suffixes[] = { "", "", "m", "dim", "aug", "sus2", "sus4",
                                      "7", "maj7", "m7", "m7b5", "dim7" };
    if (quality < 0 || quality >= 12)
        return "N/C";
    return juce::String (noteNames[root]) + suffixes[quality];
}

void styleButton (juce::TextButton& button, juce::Colour colour)
{
    button.setColour (juce::TextButton::buttonColourId, panel.brighter (0.12f));
    button.setColour (juce::TextButton::buttonOnColourId, colour.withAlpha (0.98f));
    button.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffeee7d8));
    button.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff21180c));
    button.setClickingTogglesState (true);
}

int sectionDisplayGroup (const juce::String& name)
{
    const auto folded = name.toLowerCase();
    if (folded.startsWith ("sint")) return 0;
    if (folded.startsWith ("intro")) return 1;
    if (folded.startsWith ("main")) return 2;
    if (folded.startsWith ("fill")) return 3;
    if (folded.startsWith ("break")) return 4;
    if (folded.startsWith ("ending")) return 5;
    return 6;
}

int getSectionPanelHeight (int sectionCount)
{
    constexpr int columns = 4;
    constexpr int gap = 9;
    constexpr int buttonHeight = 44;
    const int rows = juce::jmax (1, (sectionCount + columns - 1) / columns);
    return juce::jmax (114, 58 + rows * buttonHeight + (rows - 1) * gap);
}

bool usesPhoneLayout (int width)
{
#if JUCE_ANDROID
    return width < 760;
#else
    juce::ignoreUnused (width);
    return false;
#endif
}

int getPhoneSectionColumns (int width)
{
    return width < 420 ? 2 : (width < 760 ? 3 : 4);
}

int getPhoneEditorHeight (int width, int sectionCount)
{
    const int columns = getPhoneSectionColumns (width);
    const int rows = juce::jmax (1, (sectionCount + columns - 1) / columns);
    const int panelHeight = 52 + rows * 52;
    return 230 + panelHeight;
}

juce::String gmProgramName (int program)
{
    static const char* names[] = {
        "Piano", "Bright Piano", "Electric Grand", "Honky-tonk", "EP 1", "EP 2", "Harpsichord", "Clav",
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
    return program >= 0 && program < 128 ? juce::String (names[program]) : juce::String ("unknown");
}

juce::String friendlyPartName (juce::String label)
{
    label = label.trim();
    if (label.equalsIgnoreCase ("Bs")) return "Bass";
    if (label.equalsIgnoreCase ("Gt")) return "Guitar";
    if (label.equalsIgnoreCase ("Pf")) return "Piano";
    if (label.equalsIgnoreCase ("Vox")) return "Voice";
    if (label.equalsIgnoreCase ("Drs")) return "Drums";
    if (label.equalsIgnoreCase ("Sax")) return "Sax";
    if (label.equalsIgnoreCase ("NGt") || label.equalsIgnoreCase ("NIGt")) return "Nylon Guitar";
    return label;
}
}

SmartArrangerAudioProcessorEditor::SmartArrangerAudioProcessorEditor (
    SmartArrangerAudioProcessor& processorToUse)
    : juce::AudioProcessorEditor (processorToUse), targetProcessor (processorToUse)
{
    setResizable (true, false);
#if JUCE_ANDROID
    setResizeLimits (320, 400, 1600, 2400);
    setSize (400, getPhoneEditorHeight (400, 0));
#else
    setResizeLimits (760, 431, 1100, 1200);
    setSize (760, 431);
#endif
    addAndMakeVisible (loadStyleButton);
    loadStyleButton.onClick = [this] { chooseStyleFile(); };
    loadStyleButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff34291c));
    loadStyleButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffe6b2));

    tempoRateCaption.setText ("TEMPO", juce::dontSendNotification);
    tempoRateCaption.setJustificationType (juce::Justification::centred);
    tempoRateCaption.setColour (juce::Label::textColourId, muted);
    tempoRateCaption.setFont (juce::Font (10.0f, juce::Font::bold));
    addAndMakeVisible (tempoRateCaption);
    tempoRateValue.setJustificationType (juce::Justification::centred);
    tempoRateValue.setColour (juce::Label::textColourId, juce::Colour (0xffffe6b2));
    tempoRateValue.setFont (juce::Font (11.0f, juce::Font::bold));
    tempoRateValue.setText ("100%", juce::dontSendNotification);
    addAndMakeVisible (tempoRateValue);
    tempoRateSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    tempoRateSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    tempoRateSlider.setRange (0.5, 2.0, 0.01);
    tempoRateSlider.setValue (targetProcessor.getTempoRate(), juce::dontSendNotification);
    tempoRateSlider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                         juce::MathConstants<float>::pi * 2.75f, true);
    tempoRateSlider.setColour (juce::Slider::rotarySliderFillColourId, gold);
    tempoRateSlider.setColour (juce::Slider::rotarySliderOutlineColourId, panel.brighter (0.7f));
    tempoRateSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffffe6b2));
    tempoRateSlider.setTooltip ("Playback tempo multiplier; 100% follows the style or host tempo.");
    tempoRateSlider.onValueChange = [this]
    {
        const auto rate = (float) tempoRateSlider.getValue();
        targetProcessor.setTempoRate (rate);
        tempoRateValue.setText (juce::String (juce::roundToInt (rate * 100.0f)) + "%",
                                juce::dontSendNotification);
    };
    addAndMakeVisible (tempoRateSlider);

    addAndMakeVisible (accompanimentButton);
    accompanimentButton.setClickingTogglesState (true);
    accompanimentButton.setColour (juce::TextButton::buttonColourId, panel.brighter (0.12f));
    accompanimentButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe9b85b));
    accompanimentButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffeee7d8));
    accompanimentButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff21180c));
    accompanimentAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        targetProcessor.getParameterState(), "accompanimentEnabled", accompanimentButton);
    // Keep the engine's run state explicit for both the AU host and the
    // standalone diagnostic host. The attachment mirrors the parameter/UI;
    // this callback also writes the clicked state directly to the processor.
    accompanimentButton.onClick = [this]
    {
        targetProcessor.setAccompanimentEnabled (accompanimentButton.getToggleState());
    };
    startTimerHz (20);
}

void SmartArrangerAudioProcessorEditor::resized()
{
#if JUCE_ANDROID
    if (usesPhoneLayout (getWidth()))
    {
        const int desiredHeight = getPhoneEditorHeight (getWidth(), sectionButtons.size());
        if (getHeight() != desiredHeight)
            setSize (getWidth(), desiredHeight);
        auto bounds = getLocalBounds().reduced (12);
        auto header = bounds.removeFromTop (42);
        loadStyleButton.setBounds (header.removeFromRight (136).reduced (0, 2));
        tempoRateCaption.setVisible (false);
        tempoRateValue.setVisible (false);
        tempoRateSlider.setVisible (false);
        accompanimentButton.setBounds ({ });
        const int cardTop = 54;
        auto styleCard = juce::Rectangle<int> (bounds.getX(), cardTop, bounds.getWidth(), 72);
        const int sectionTop = styleCard.getBottom() + 10;
        const int columns = getPhoneSectionColumns (getWidth());
        const int gap = 8;
        const int gridWidth = bounds.getWidth() - 20;
        const int cellWidth = (gridWidth - gap * (columns - 1)) / columns;
        const int gridTop = sectionTop + 48;
        for (int i = 0; i < sectionButtons.size(); ++i)
        {
            const int row = i / columns;
            const int column = i % columns;
            sectionButtons[i]->setBounds (bounds.getX() + 10 + column * (cellWidth + gap),
                                           gridTop + row * 52, cellWidth, 44);
        }
        const int sectionRows = juce::jmax (1, (sectionButtons.size() + columns - 1) / columns);
        const int panelHeight = 52 + sectionRows * 52;
        const int chordTop = sectionTop + panelHeight + 10;
        accompanimentButton.setBounds (bounds.getRight() - 142, sectionTop + 6, 130, 36);
        return;
    }
#endif
    auto area = getLocalBounds().reduced (22);
    auto header = area.removeFromTop (54);
    loadStyleButton.setBounds (header.removeFromRight (174).reduced (0, 6));
    tempoRateCaption.setVisible (true);
    tempoRateValue.setVisible (true);
    tempoRateSlider.setVisible (true);
    // Keep the speed control beside the style BPM readout. The style summary
    // card reserves a narrow right column for it, leaving the metadata below
    // available for the full list of style parts.
    tempoRateCaption.setBounds (getWidth() - 122, 80, 92, 13);
    tempoRateValue.setBounds (getWidth() - 122, 94, 92, 15);
    tempoRateSlider.setBounds (getWidth() - 108, 108, 64, 62);

    const int columns = 4;
    const int gap = 9;
    const int buttonHeight = 44;
    const int sectionPanelHeight = getSectionPanelHeight (sectionButtons.size());
    auto sections = juce::Rectangle<int> (26, 206, getWidth() - 52, sectionPanelHeight);
    auto grid = sections.reduced (10, 0).withTrimmedTop (50);
    const int width = (grid.getWidth() - gap * (columns - 1)) / columns;
    for (int i = 0; i < sectionButtons.size(); ++i)
    {
        const int row = i / columns;
        const int column = i % columns;
        sectionButtons[i]->setBounds (grid.getX() + column * (width + gap),
                                      grid.getY() + row * (buttonHeight + gap),
                                      width, buttonHeight);
    }
    accompanimentButton.setBounds (sections.getRight() - 128, sections.getY() + 9, 112, 34);
}

void SmartArrangerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
    auto bounds = getLocalBounds().reduced (12);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a2026), 0.0f, 0.0f,
                                             juce::Colour (0xff0c1218), 0.0f,
                                             (float) getHeight(), false));
    g.fillRoundedRectangle (bounds.toFloat(), 16.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (bounds.toFloat(), 16.0f, 1.5f);
    g.setColour (gold.withAlpha (0.18f));
    g.drawRoundedRectangle (bounds.reduced (4).toFloat(), 13.0f, 0.7f);

#if JUCE_ANDROID
    if (usesPhoneLayout (getWidth()))
    {
        const auto content = getLocalBounds().reduced (12);
        g.setColour (gold);
        g.setFont (juce::Font (21.0f, juce::Font::bold));
        g.drawText ("STYLE SELECTION", content.getX() + 4, 15,
                    juce::jmax (100, content.getWidth() - 150), 34,
                    juce::Justification::centredLeft);

        const auto styleCard = juce::Rectangle<int> (content.getX(), 54, content.getWidth(), 72);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff242a30), styleCard.getX(), styleCard.getY(),
                                                 juce::Colour (0xff11171d), styleCard.getX(), styleCard.getBottom(), false));
        g.fillRoundedRectangle (styleCard.toFloat(), 10.0f);
        g.setColour (gold.withAlpha (0.48f));
        g.drawRoundedRectangle (styleCard.toFloat(), 10.0f, 1.2f);
        g.setColour (juce::Colour (0xfff2f7fb));
        g.setFont (juce::Font (16.0f, juce::Font::bold));
        g.drawFittedText (styleTitle, styleCard.getX() + 12, styleCard.getY() + 7,
                          styleCard.getWidth() - 24, 25, juce::Justification::centredLeft, 1);
        g.setColour (muted);
        g.setFont (juce::Font (12.0f));
        g.drawFittedText (styleSummary + "   " + styleDetails,
                          styleCard.getX() + 12, styleCard.getY() + 37,
                          styleCard.getWidth() - 24, 25, juce::Justification::centredLeft, 1);

        const int sectionTop = styleCard.getBottom() + 10;
        const int columns = getPhoneSectionColumns (getWidth());
        const int rows = juce::jmax (1, (sectionButtons.size() + columns - 1) / columns);
        const int sectionHeight = 52 + rows * 52;
        const auto sectionPanel = juce::Rectangle<int> (content.getX(), sectionTop,
                                                         content.getWidth(), sectionHeight);
        g.setColour (juce::Colour (0xff151b21));
        g.fillRoundedRectangle (sectionPanel.toFloat(), 11.0f);
        g.setColour (gold.withAlpha (0.43f));
        g.drawRoundedRectangle (sectionPanel.toFloat(), 11.0f, 1.1f);
        g.setColour (gold);
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText ("STYLE SECTIONS", sectionPanel.getX() + 12, sectionPanel.getY() + 10,
                    juce::jmax (100, sectionPanel.getWidth() - 170), 30,
                    juce::Justification::centredLeft);
        if (sectionButtons.isEmpty())
        {
            g.setColour (muted);
            g.setFont (juce::Font (13.0f));
            g.drawFittedText (styleError.isEmpty() ? "Load a style to show its sections." : styleError,
                              sectionPanel.reduced (12).withTrimmedTop (40),
                              juce::Justification::centred, 2);
        }

        const int chordTop = sectionPanel.getBottom() + 10;
        const auto chordCard = juce::Rectangle<int> (content.getX(), chordTop, content.getWidth(), 72);
        g.setColour (juce::Colour (0xff151b21));
        g.fillRoundedRectangle (chordCard.toFloat(), 11.0f);
        g.setColour (gold.withAlpha (0.43f));
        g.drawRoundedRectangle (chordCard.toFloat(), 11.0f, 1.1f);
        g.setColour (gold);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText ("LIVE CHORD", chordCard.getX() + 12, chordCard.getY() + 7, 110, 18,
                    juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xfff1f6fa));
        g.setFont (juce::Font (27.0f, juce::Font::bold));
        g.drawText (getChordLabel (targetProcessor.getDetectedChordRoot(),
                                   targetProcessor.getDetectedChordQuality()),
                    chordCard.getX() + 12, chordCard.getY() + 25, 110, 34,
                    juce::Justification::centredLeft);
        g.setColour (muted);
        g.setFont (juce::Font (11.0f));
        g.drawFittedText ("Chord input controls the accompaniment",
                          chordCard.getX() + 130, chordCard.getY() + 20,
                          chordCard.getWidth() - 142, 34, juce::Justification::centredLeft, 1);
        return;
    }
#endif

    g.setColour (gold);
    g.setFont (juce::Font (22.0f, juce::Font::bold));
    g.drawText ("STYLE SELECTION", 28, 17, 270, 34, juce::Justification::centredLeft);
    const float titleCardWidth = (float) getWidth() - 518.0f;
    const juce::Rectangle<float> titleCard (310.0f, 28.0f, titleCardWidth, 42.0f);
    g.setColour (juce::Colour (0xff211c15));
    g.fillRoundedRectangle (titleCard, 9.0f);
    g.setColour (gold.withAlpha (0.60f));
    g.drawRoundedRectangle (titleCard, 9.0f, 1.0f);
    g.setColour (muted);
    g.setFont (juce::Font (9.0f, juce::Font::bold));
    g.drawText ("STYLE", 323, 30, titleCardWidth - 26.0f, 12, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffedf5fb));
    g.setFont (juce::Font (13.0f, juce::Font::bold));
    g.drawFittedText (styleTitle, 323, 43, titleCardWidth - 26.0f, 21,
                      juce::Justification::centredLeft, 1);

    auto styleCard = juce::Rectangle<int> (26, 76, getWidth() - 52, 112);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff242a30), 26.0f, 76.0f,
                                             juce::Colour (0xff11171d), 26.0f,
                                             188.0f, false));
    g.fillRoundedRectangle (styleCard.toFloat(), 12.0f);
    g.setColour (gold.withAlpha (0.48f));
    g.drawRoundedRectangle (styleCard.toFloat(), 12.0f, 1.2f);
    g.setColour (juce::Colour (0xfff2f7fb));
    g.setFont (juce::Font (19.0f, juce::Font::bold));
    g.drawText (styleSummary, 42, 86, getWidth() - 190, 30,
                juce::Justification::centredLeft, true);
    g.setColour (styleError.isEmpty() ? muted : juce::Colour (0xffff8c83));
    g.setFont (juce::Font (12.0f));
    g.drawText (styleError.isNotEmpty() ? styleError : styleDetails,
                42, 116, getWidth() - 150, 17, juce::Justification::centredLeft, true);
    if (styleError.isEmpty() && ! channelDetails.isEmpty())
    {
        g.setColour (muted);
        constexpr int columns = 5;
        constexpr int rowHeight = 15;
        const int columnWidth = (getWidth() - 164) / columns;
        g.setFont (juce::Font (11.0f));
        for (int i = 0; i < channelDetails.size(); ++i)
        {
            const int column = i % columns;
            const int row = i / columns;
            g.drawFittedText (channelDetails[i], 42 + column * columnWidth, 136 + row * rowHeight,
                              columnWidth - 6, rowHeight,
                              juce::Justification::centredLeft, 1);
        }
    }

    g.setColour (juce::Colour (0xff151b21));
    const int sectionPanelHeight = getSectionPanelHeight (sectionButtons.size());
    g.fillRoundedRectangle (26.0f, 206.0f, (float) getWidth() - 52.0f,
                            (float) sectionPanelHeight, 12.0f);
    g.setColour (gold.withAlpha (0.40f));
    g.drawRoundedRectangle (26.0f, 206.0f, (float) getWidth() - 52.0f,
                            (float) sectionPanelHeight, 12.0f, 1.2f);
    g.setColour (gold);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText ("STYLE SECTIONS", 42, 216, 200, 22, juce::Justification::centredLeft);
    if (sectionButtons.isEmpty())
    {
        g.setColour (muted);
        g.setFont (juce::Font (15.0f));
        g.drawFittedText ("LOAD A STYLE FILE TO SHOW INTRO, MAIN, FILL AND ENDING SECTIONS",
                          42, 267, getWidth() - 84, 42, juce::Justification::centred, 1);
    }

    const int chordTop = 206 + sectionPanelHeight + 21;
    g.setColour (juce::Colour (0xff151b21));
    g.fillRoundedRectangle (26.0f, (float) chordTop, (float) getWidth() - 52.0f, 64.0f, 12.0f);
    g.setColour (gold.withAlpha (0.43f));
    g.drawRoundedRectangle (26.0f, (float) chordTop, (float) getWidth() - 52.0f, 64.0f, 12.0f, 1.1f);
    g.setColour (gold);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText ("LIVE CHORD", 42, chordTop + 8, 100, 18, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xfff1f6fa));
    g.setFont (juce::Font (27.0f, juce::Font::bold));
    g.drawText (getChordLabel (targetProcessor.getDetectedChordRoot(),
                               targetProcessor.getDetectedChordQuality()),
                42, chordTop + 24, 105, 32, juce::Justification::centredLeft);
    g.setColour (muted);
    g.setFont (juce::Font (12.0f));
    g.drawFittedText ("Play a chord in Logic. The latest recognized harmony stays active.",
                      160, chordTop + 18, getWidth() - 206, 32,
                      juce::Justification::centredLeft, 1);
}

void SmartArrangerAudioProcessorEditor::chooseStyleFile()
{
#if JUCE_ANDROID
    juce::FileChooser::registerCustomMimeTypeForFileExtension ("application/octet-stream", ".sty");
    juce::FileChooser::registerCustomMimeTypeForFileExtension ("application/octet-stream", ".prs");
    // Android's document providers commonly report Yamaha style files with
    // provider-specific/unknown MIME types. SAF filters by MIME, not by the
    // filename wildcard, so a *.sty filter can leave valid files disabled.
    // Allow selection of any document here; loadStyleFile validates the data.
    constexpr auto chooserWildcard = "*";
#else
    constexpr auto chooserWildcard = "*.sty;*.STY;*.prs;*.PRS;*.bcs;*.BCS;*.sst;*.SST;*.scp;*.SCP;*.fps;*.FPS;*.aus;*.AUS";
#endif
    fileChooser = std::make_unique<juce::FileChooser> ("Choose a Yamaha Style file",
        juce::File(), chooserWildcard);
    juce::Component::SafePointer<SmartArrangerAudioProcessorEditor> safeThis (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser)
                              {
                                  if (safeThis != nullptr)
                                  {
                                      const auto file = copyPickedUrlToLocalFile (chooser.getURLResult(), ".sty");
                                      if (file.existsAsFile())
                                          safeThis->loadStyleFile (file);
                                  }
                              });
}

void SmartArrangerAudioProcessorEditor::loadStyleFile (const juce::File& file)
{
    lastRestoreAttemptPath = file.getFullPathName();
    SmartArrangerStyleInfo info;
    juce::String error;
    if (! SmartArrangerStyleFile::inspect (file, info, error))
    {
        const bool retryingSavedStyle = file.getFullPathName() == targetProcessor.getStoredStylePath();
        if (! retryingSavedStyle)
            targetProcessor.clearLoadedStyle();
        loadedStyleInfo = {};
        sectionButtons.clear (true);
        sectionButtonToStyleIndex.clear();
        setSize (getWidth(), 542);
        styleTitle = "STYLE NOT LOADED";
        styleSummary = "Could not load " + file.getFileName();
        styleDetails.clear();
        channelDetails.clear();
        styleError = error;
        resized(); repaint();
        return;
    }

    loadedStyleInfo = std::move (info);
    loadedStylePath = file.getFullPathName();
    const bool restoringSameStyle = loadedStylePath == targetProcessor.getStoredStylePath();
    const int sectionToLoad = restoringSameStyle
        ? juce::jlimit (0, (int) loadedStyleInfo.sections.size() - 1,
                        targetProcessor.getStoredStyleSection()) : 0;
    rebuildSectionButtons (sectionToLoad);
    if (! targetProcessor.setLoadedStyle (loadedStyleInfo, sectionToLoad, error, file))
    {
        targetProcessor.clearLoadedStyle();
        styleTitle = loadedStyleInfo.name;
        styleSummary = "Style parsed";
        styleDetails.clear();
        styleError = error;
        resized(); repaint();
        return;
    }

    int channels = 0;
    for (int channel = 0; channel < 16; ++channel)
        if ((loadedStyleInfo.usedChannels & (1u << channel)) != 0) ++channels;
    styleTitle = loadedStyleInfo.name;
    // JUCE 9 removed Font::getStringWidth; estimate typical bold glyph width at 13 pt.
    // The editor width then grows with the title, up to the resizable window limit.
    const int titleWidth = styleTitle.length() * 8;
#if JUCE_ANDROID
    if (! usesPhoneLayout (getWidth()))
        setSize (juce::jlimit (760, 1100, 518 + titleWidth + 32), getHeight());
#else
    setSize (juce::jlimit (760, 1100, 518 + titleWidth + 32), getHeight());
#endif
    styleSummary = loadedStyleInfo.format + " | "
                 + juce::String (loadedStyleInfo.tempo) + " BPM | "
                 + juce::String (loadedStyleInfo.numerator) + "/" + juce::String (loadedStyleInfo.denominator);
    styleDetails = juce::String ((int) loadedStyleInfo.sections.size()) + " sections | "
                 + juce::String (channels) + " MIDI channels | "
                 + juce::String ((int) loadedStyleInfo.events.size()) + " events";

    std::array<int, 16> programs;
    programs.fill (-1);
    std::array<juce::String, 16> parts;
    std::array<std::array<bool, 16>, 16> casmOutputChannels {};
    for (const auto& segment : loadedStyleInfo.casmSegments)
        for (const auto& record : segment.records)
            if (record.hasCommonChannelFields)
            {
                const int source = record.sourceChannel;
                if (source >= 0 && source < 16)
                {
                    if (parts[(size_t) source].isEmpty())
                        parts[(size_t) source] = record.partLabel.trim();
                    if (record.destinationChannel < 16)
                        casmOutputChannels[(size_t) source][record.destinationChannel] = true;
                }
            }
    for (const auto& event : loadedStyleInfo.events)
        if ((event.status & 0xf0u) == 0xc0u)
            programs[(size_t) (event.status & 0x0fu)] = event.data1;
    channelDetails.clear();
    for (int channel = 0; channel < 16; ++channel)
    {
        if ((loadedStyleInfo.usedChannels & (1u << channel)) == 0) continue;
        auto detail = "Ch " + juce::String (channel + 1);
        juce::StringArray outputLabels;
        for (int outputChannel = 0; outputChannel < 16; ++outputChannel)
            if (casmOutputChannels[(size_t) channel][(size_t) outputChannel])
                outputLabels.add (juce::String (outputChannel + 1));
        detail += " -> MIDI " + (outputLabels.isEmpty()
            ? juce::String (channel + 1) : outputLabels.joinIntoString ("/"));
        if (parts[(size_t) channel].isNotEmpty())
            detail += " " + friendlyPartName (parts[(size_t) channel]);
        if (programs[(size_t) channel] >= 0)
            detail += ": " + gmProgramName (programs[(size_t) channel]);
        else
            detail += ": instrument unspecified";
        channelDetails.add (detail);
    }
    styleError.clear();
    resized(); repaint();
}

void SmartArrangerAudioProcessorEditor::rebuildSectionButtons (int selectedIndex)
{
    sectionButtons.clear (true);
    sectionButtonToStyleIndex.clear();
    std::vector<int> displayOrder (loadedStyleInfo.sections.size());
    for (int i = 0; i < (int) displayOrder.size(); ++i) displayOrder[(size_t) i] = i;
    std::stable_sort (displayOrder.begin(), displayOrder.end(), [this] (int a, int b)
    {
        const auto& first = loadedStyleInfo.sections[(size_t) a].name;
        const auto& second = loadedStyleInfo.sections[(size_t) b].name;
        const int firstGroup = sectionDisplayGroup (first);
        const int secondGroup = sectionDisplayGroup (second);
        return firstGroup != secondGroup ? firstGroup < secondGroup
                                         : first.compareNatural (second) < 0;
    });
    for (const int i : displayOrder)
    {
        const auto& section = loadedStyleInfo.sections[(size_t) i];
        const auto name = section.name;
        auto* button = sectionButtons.add (new juce::TextButton (name));
        auto sectionLength = section.lengthTicks;
        if (sectionLength == 0 && loadedStyleInfo.totalTicks > section.startTick)
            sectionLength = loadedStyleInfo.totalTicks - section.startTick;
        const auto sectionEnd = section.startTick + sectionLength;
        const bool hasEvents = sectionLength > 0 && std::any_of (
            loadedStyleInfo.events.begin(), loadedStyleInfo.events.end(), [i, &section, sectionEnd] (const auto& event)
            {
                const bool fallbackTrack = section.name == "Track" && event.sectionIndex < 0;
                return (fallbackTrack || event.sectionIndex == i)
                    && event.tick >= section.startTick && event.tick < sectionEnd;
            });
        button->setEnabled (hasEvents);
        const auto folded = name.toLowerCase();
        const auto accent = folded.contains ("intro") ? cyan
                          : (folded.contains ("main") ? gold
                          : (folded.contains ("fill") ? juce::Colour (0xff529aff)
                          : (folded.contains ("break") ? juce::Colour (0xffc27bff)
                          : (folded.contains ("end") ? juce::Colour (0xffff776e) : cyan))));
        styleButton (*button, accent);
        button->setRadioGroupId (7001);
        button->setToggleState (i == selectedIndex, juce::dontSendNotification);
        sectionButtonToStyleIndex.add (i);
        button->onClick = [this, i] { selectStyleSection (i); };
        addAndMakeVisible (button);
    }
    const int panelHeight = getSectionPanelHeight (sectionButtons.size());
#if JUCE_ANDROID
    if (usesPhoneLayout (getWidth()))
        setSize (getWidth(), getPhoneEditorHeight (getWidth(), sectionButtons.size()));
    else
        setSize (getWidth(), 317 + panelHeight);
#else
    setSize (getWidth(), 317 + panelHeight);
#endif
    resized();
}

void SmartArrangerAudioProcessorEditor::selectStyleSection (int index)
{
    if (index < 0 || index >= (int) loadedStyleInfo.sections.size()) return;
    targetProcessor.requestStyleSection (index);
    styleError.clear();
    repaint();
}

void SmartArrangerAudioProcessorEditor::activateStyleSection()
{
    selectStyleSection (0);
}

bool SmartArrangerAudioProcessorEditor::activateSectionByDisplayIndex (int displayIndex)
{
    if (displayIndex < 0 || displayIndex >= sectionButtonToStyleIndex.size())
        return false;
    const int styleIndex = sectionButtonToStyleIndex.getUnchecked (displayIndex);
    if (styleIndex < 0 || styleIndex >= loadedStyleInfo.sections.size()
        || ! sectionButtons[displayIndex]->isEnabled())
        return false;
    selectStyleSection (styleIndex);
    return true;
}

void SmartArrangerAudioProcessorEditor::timerCallback()
{
    if (targetProcessor.hasStylePlaybackFinished() && targetProcessor.isAccompanimentEnabled())
        targetProcessor.setAccompanimentEnabled (false);
    const int displayedSection = targetProcessor.getRequestedStyleSection();
    if (displayedSection >= 0 && displayedSection < loadedStyleInfo.sections.size())
    {
        for (int i = 0; i < sectionButtons.size(); ++i)
            sectionButtons[i]->setToggleState (sectionButtonToStyleIndex[i] == displayedSection,
                                               juce::dontSendNotification);
        if (displayedSection != targetProcessor.getStoredStyleSection())
            targetProcessor.requestStyleSection (displayedSection);
    }
    accompanimentButton.setButtonText (accompanimentButton.getToggleState() ? "STOP" : "START");
    const auto storedPath = targetProcessor.getStoredStylePath();
    if (storedPath.isNotEmpty() && storedPath != loadedStylePath
        && storedPath != lastRestoreAttemptPath)
    {
        loadStyleFile (juce::File (storedPath));
        return;
    }
    repaint();
}
