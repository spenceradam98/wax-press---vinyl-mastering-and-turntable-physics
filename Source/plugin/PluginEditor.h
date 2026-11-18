#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Themes.h"

// The record seen from above, with a tonearm that follows (and sets) the needle position.
class RecordView : public juce::Component
{
public:
    explicit RecordView (WaxPressProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void setTheme (const Theme& t) { theme = t; repaint(); }
    void tick (float rpm, float needlePos, float level, bool autoTravel);

private:
    juce::Point<float> stylusPoint (float radius) const;
    float radiusFor (float needlePos) const;
    void setNeedleFromMouse (juce::Point<float>);

    WaxPressProcessor& proc;
    Theme theme;
    float angle = 0, needle = 0, shine = 0;
    bool autoOn = false, dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecordView)
};

// All controls live in a fixed-size content component that is scaled to the window size.
class WaxPressContent : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1120, H = 660;

    explicit WaxPressContent (WaxPressProcessor&);
    ~WaxPressContent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshNow() { timerCallback(); }

private:
    struct Knob
    {
        int param = 0;
        juce::String label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    struct Panel { juce::String title; juce::Rectangle<int> bounds; };

    Knob& addKnob (int param, const char* label);
    void addCombo (juce::ComboBox&, int param, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&);
    void placeRow (juce::Rectangle<int> area, std::initializer_list<Knob*> row);
    void applyTheme (int index);
    void showPresetMenu();
    void savePresetDialog();
    void timerCallback() override;
    void drawMeters (juce::Graphics&, juce::Rectangle<float>);

    WaxPressProcessor& proc;
    WaxLookAndFeel lnf;
    RecordView record;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Panel> panels;
    std::map<int, Knob*> byParam;

    juce::ComboBox speedBox, stylusBox, modeBox, themeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> speedAttach, stylusAttach, modeAttach;
    juce::ToggleButton autoButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoAttach;
    juce::Rectangle<int> autoCell;

    juce::TextButton presetButton, prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" };
    juce::Image background;
    juce::Rectangle<float> meterArea;
    float lat = 0, vert = 0, vertLow = 0, hfGr = 0, dist = 0, inM = 0, outM = 0;
    bool warn = false;
    juce::String shownPreset;
    std::unique_ptr<juce::AlertWindow> saveWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaxPressContent)
};

class WaxPressEditor : public juce::AudioProcessorEditor
{
public:
    explicit WaxPressEditor (WaxPressProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
    void refreshNow() { content.refreshNow(); }

private:
    WaxPressProcessor& proc;
    WaxPressContent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaxPressEditor)
};
