// UI themes. Each theme changes rendering (knob style, record finish, grain), not only colours.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

struct Theme
{
    juce::String name;
    juce::Colour bgTop, bgBottom, panel, panelEdge, text, textDim, accent, accent2, knobFace, knobRim, track;
    int knobStyle = 0;        // 0 soft glass, 1 neon ring, 2 flat matte, 3 bakelite pointer
    bool glow = false, grain = false, scanlines = false, stars = false;
    float corner = 10.0f, grainAmount = 0.0f;

    // the record
    juce::Colour vinyl, vinylGroove, vinylShine, label, labelText, armColour;
    bool splatter = false;                     // coloured vinyl with splatter
    juce::Colour splatterA, splatterB;
    float vinylAlpha = 1.0f;                   // translucent coloured vinyl < 1
};

const std::vector<Theme>& themes();

class WaxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    WaxLookAndFeel();
    void setTheme (const Theme&);
    const Theme& theme() const { return t; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Font getPopupMenuFont() override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;

private:
    Theme t;
};
