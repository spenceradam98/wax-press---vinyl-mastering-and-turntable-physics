#include "Themes.h"

using namespace juce;

const std::vector<Theme>& themes()
{
    static const std::vector<Theme> list = []
    {
        std::vector<Theme> v;
        Theme t;

        // Lacquer Black: the cutting room. Charcoal, black lacquer, red label, chrome.
        t = {};
        t.name = "Lacquer Black";
        t.bgTop = Colour (0xff1c1d20); t.bgBottom = Colour (0xff0c0c0e);
        t.panel = Colour (0xff1a1b1f); t.panelEdge = Colour (0xff34363c);
        t.text = Colour (0xffeceae6); t.textDim = Colour (0xff8d8f96);
        t.accent = Colour (0xffe0452e); t.accent2 = Colour (0xffd8dde4);
        t.knobFace = Colour (0xffcfd4da); t.knobRim = Colour (0xff6b7079); t.track = Colour (0xff2c2e33);
        t.knobStyle = 0; t.corner = 6.0f;
        t.vinyl = Colour (0xff09090a); t.vinylGroove = Colour (0xff1d1d20); t.vinylShine = Colour (0xffffffff);
        t.label = Colour (0xffc8331f); t.labelText = Colour (0xfff6efe2); t.armColour = Colour (0xffd5d9de);
        v.push_back (t);

        // Shellac Brown: a 78 in a wooden cabinet. Warm browns, cream label, bakelite knobs.
        t = {};
        t.name = "Shellac Brown";
        t.bgTop = Colour (0xff4a2f1d); t.bgBottom = Colour (0xff24150c);
        t.panel = Colour (0xffeee0c0); t.panelEdge = Colour (0xffb39561);
        t.text = Colour (0xff3a2616); t.textDim = Colour (0xff7d6247);
        t.accent = Colour (0xffa8402a); t.accent2 = Colour (0xff3f5f4a);
        t.knobFace = Colour (0xff1c1614); t.knobRim = Colour (0xffd9c9a3); t.track = Colour (0x40000000);
        t.knobStyle = 3; t.grain = true; t.grainAmount = 0.05f; t.corner = 5.0f;
        t.vinyl = Colour (0xff1b1210); t.vinylGroove = Colour (0xff2c1f1a); t.vinylShine = Colour (0xffffe6c4);
        t.label = Colour (0xff2c3d6b); t.labelText = Colour (0xffe9d39a); t.armColour = Colour (0xffb08a4c);
        v.push_back (t);

        // Coloured Vinyl: limited edition splatter pressing on a night-blue desk.
        t = {};
        t.name = "Coloured Vinyl";
        t.bgTop = Colour (0xff141a3a); t.bgBottom = Colour (0xff2a1240);
        t.panel = Colour (0xcc151231); t.panelEdge = Colour (0xffff8a3d).withAlpha (0.45f);
        t.text = Colour (0xfff7eefe); t.textDim = Colour (0xffa99cc8);
        t.accent = Colour (0xffff8a3d); t.accent2 = Colour (0xff45e0d0);
        t.knobFace = Colour (0xff1b1736); t.knobRim = Colour (0xff332d5c); t.track = Colour (0xff2e2852);
        t.knobStyle = 1; t.glow = true; t.corner = 10.0f;
        t.vinyl = Colour (0xffff7a2e); t.vinylGroove = Colour (0xffd9561a); t.vinylShine = Colour (0xffffffff);
        t.label = Colour (0xfff4f1e8); t.labelText = Colour (0xff2a1240); t.armColour = Colour (0xffe8e4f4);
        t.splatter = true; t.splatterA = Colour (0xff3fd6c8); t.splatterB = Colour (0xfffff3c4); t.vinylAlpha = 0.93f;
        v.push_back (t);

        // Test Pressing White: white label, white sleeve, rubber-stamp lettering.
        t = {};
        t.name = "Test Pressing White";
        t.bgTop = Colour (0xffeeece6); t.bgBottom = Colour (0xffd9d6cd);
        t.panel = Colour (0xfffbfaf6); t.panelEdge = Colour (0xffc4c0b4);
        t.text = Colour (0xff23221f); t.textDim = Colour (0xff7a776e);
        t.accent = Colour (0xff2a54c7); t.accent2 = Colour (0xffd2402b);
        t.knobFace = Colour (0xfff1efe9); t.knobRim = Colour (0xffbab5a8); t.track = Colour (0xffdedad0);
        t.knobStyle = 2; t.corner = 3.0f;
        t.vinyl = Colour (0xfff4f3ef); t.vinylGroove = Colour (0xffd6d3ca); t.vinylShine = Colour (0xff9e9a8f);
        t.label = Colour (0xffffffff); t.labelText = Colour (0xff2a54c7); t.armColour = Colour (0xff4a4844);
        v.push_back (t);
        return v;
    }();
    return list;
}

WaxLookAndFeel::WaxLookAndFeel() { setTheme (themes().front()); }

void WaxLookAndFeel::setTheme (const Theme& th)
{
    t = th;
    const bool lightPanel = (t.panel.getPerceivedBrightness() > 0.6f && t.panel.getFloatAlpha() > 0.8f);
    const auto popupBg = lightPanel ? Colour (0xfff6eedb) : t.bgTop.darker (0.3f).withAlpha (1.0f);
    setColour (ComboBox::backgroundColourId, Colours::transparentBlack);
    setColour (ComboBox::textColourId, t.text);
    setColour (ComboBox::outlineColourId, t.panelEdge);
    setColour (ComboBox::arrowColourId, t.textDim);
    setColour (PopupMenu::backgroundColourId, popupBg);
    setColour (PopupMenu::textColourId, lightPanel ? Colour (0xff3a2418) : t.text);
    setColour (PopupMenu::headerTextColourId, t.accent);
    setColour (PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha (0.35f));
    setColour (PopupMenu::highlightedTextColourId, lightPanel ? Colours::black : Colours::white);
    setColour (TextButton::textColourOffId, t.text);
    setColour (TextButton::textColourOnId, t.text);
    setColour (TextButton::buttonColourId, Colours::transparentBlack);
    setColour (Label::textColourId, t.text);
    setColour (TextEditor::backgroundColourId, popupBg);
    setColour (TextEditor::textColourId, lightPanel ? Colour (0xff3a2418) : t.text);
    setColour (TextEditor::outlineColourId, t.panelEdge);
    setColour (AlertWindow::backgroundColourId, popupBg);
    setColour (AlertWindow::textColourId, lightPanel ? Colour (0xff3a2418) : t.text);
    setColour (TooltipWindow::backgroundColourId, popupBg);
    setColour (TooltipWindow::textColourId, lightPanel ? Colour (0xff3a2418) : t.text);
}

void WaxLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float d = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float r = d * 0.5f;
    const float angle = start + pos * (end - start);
    const bool bipolar = s.getProperties()["bipolar"];
    const float from = bipolar ? (start + end) * 0.5f : start;
    const bool enabled = s.isEnabled();
    const auto acc = enabled ? t.accent : t.textDim;

    auto arc = [&] (float rad, float a0, float a1, Colour col, float thick)
    {
        Path p;
        p.addCentredArc (c.x, c.y, rad, rad, 0.0f, jmin (a0, a1), jmax (a0, a1), true);
        g.setColour (col);
        g.strokePath (p, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));
    };

    switch (t.knobStyle)
    {
        case 1:   // neon ring
        {
            arc (r - 3, start, end, t.track, 4.0f);
            if (t.glow && enabled)
                for (int i = 3; i >= 1; --i) arc (r - 3, from, angle, acc.withAlpha (0.12f), 4.0f + i * 4.0f);
            arc (r - 3, from, angle, acc, 3.0f);
            auto face = Rectangle<float> (d * 0.62f, d * 0.62f).withCentre (c);
            g.setColour (t.knobFace); g.fillEllipse (face);
            g.setColour (t.accent2.withAlpha (0.5f)); g.drawEllipse (face, 1.2f);
            g.setColour (t.accent2);
            g.drawLine (Line<float> (c.getPointOnCircumference (d * 0.12f, angle), c.getPointOnCircumference (d * 0.29f, angle)), 2.2f);
            break;
        }
        case 2:   // flat matte
        {
            arc (r - 3, start, end, t.track, 3.0f);
            arc (r - 3, from, angle, acc, 3.0f);
            auto face = Rectangle<float> (d * 0.68f, d * 0.68f).withCentre (c);
            g.setColour (t.knobFace); g.fillEllipse (face);
            g.setColour (t.knobRim); g.drawEllipse (face, 1.0f);
            g.setColour (t.text);
            g.fillEllipse (Rectangle<float> (4.5f, 4.5f).withCentre (c.getPointOnCircumference (d * 0.24f, angle)));
            break;
        }
        case 3:   // bakelite pointer knob with cream skirt and printed scale
        {
            for (int i = 0; i <= 10; ++i)
            {
                const float a = start + (end - start) * i / 10.0f;
                g.setColour (t.text.withAlpha (i % 5 == 0 ? 0.8f : 0.45f));
                g.drawLine (Line<float> (c.getPointOnCircumference (r - 1, a), c.getPointOnCircumference (r - (i % 5 == 0 ? 6.0f : 4.0f), a)), 1.2f);
            }
            auto skirt = Rectangle<float> (d * 0.78f, d * 0.78f).withCentre (c);
            g.setColour (Colours::black.withAlpha (0.35f)); g.fillEllipse (skirt.translated (1.5f, 2.5f));
            g.setGradientFill (ColourGradient (Colour (0xfff7efd9), skirt.getX(), skirt.getY(), Colour (0xffb9a679), skirt.getRight(), skirt.getBottom(), false));
            g.fillEllipse (skirt);
            auto cap = skirt.reduced (d * 0.12f);
            g.setGradientFill (ColourGradient (Colour (0xff4a3a32), cap.getX(), cap.getY(), Colour (0xff0d0908), cap.getRight(), cap.getBottom(), false));
            g.fillEllipse (cap);
            Path ptr;
            ptr.addTriangle (-d * 0.06f, -d * 0.12f, d * 0.06f, -d * 0.12f, 0.0f, -d * 0.39f);
            g.setColour (Colour (0xff15100e));
            g.fillPath (ptr, AffineTransform::rotation (angle).translated (c));
            g.setColour (t.accent);
            g.drawLine (Line<float> (c.getPointOnCircumference (d * 0.24f, angle), c.getPointOnCircumference (d * 0.37f, angle)), 2.0f);
            break;
        }
        default:  // soft glass with glowing arc
        {
            arc (r - 3, start, end, t.track, 3.5f);
            if (t.glow && enabled) arc (r - 3, from, angle, acc.withAlpha (0.25f), 9.0f);
            arc (r - 3, from, angle, acc, 3.5f);
            auto face = Rectangle<float> (d * 0.66f, d * 0.66f).withCentre (c);
            g.setColour (Colours::black.withAlpha (0.18f)); g.fillEllipse (face.translated (0, 2.5f));
            g.setGradientFill (ColourGradient (t.knobFace, face.getX(), face.getY(), t.knobRim, face.getRight(), face.getBottom(), false));
            g.fillEllipse (face);
            g.setColour (Colours::white.withAlpha (0.5f)); g.drawEllipse (face.reduced (1), 1.0f);
            g.setColour (t.bgTop);
            g.drawLine (Line<float> (c.getPointOnCircumference (d * 0.10f, angle), c.getPointOnCircumference (d * 0.28f, angle)), 2.5f);
            break;
        }
    }
}

void WaxLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    // power switch: a small pill with an LED dot
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    const float h = r.getHeight();
    auto pill = r.withWidth (h * 1.8f).withCentre (r.getCentre());
    g.setColour (on ? t.accent.withAlpha (0.35f) : t.track);
    g.fillRoundedRectangle (pill, h * 0.5f);
    g.setColour (t.panelEdge.withAlpha (highlighted ? 1.0f : 0.7f));
    g.drawRoundedRectangle (pill, h * 0.5f, 1.0f);
    auto dot = Rectangle<float> (h - 4, h - 4).withCentre ({ on ? pill.getRight() - h * 0.5f : pill.getX() + h * 0.5f, pill.getCentreY() });
    if (on && t.glow) { g.setColour (t.accent.withAlpha (0.3f)); g.fillEllipse (dot.expanded (3)); }
    g.setColour (on ? t.accent : t.textDim);
    g.fillEllipse (dot);
}

void WaxLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (t.panel.withMultipliedAlpha (down ? 2.0f : (highlighted ? 1.6f : 1.0f)));
    g.fillRoundedRectangle (r, jmin (t.corner, r.getHeight() * 0.5f));
    g.setColour (highlighted ? t.accent.withAlpha (0.8f) : t.panelEdge);
    g.drawRoundedRectangle (r, jmin (t.corner, r.getHeight() * 0.5f), 1.0f);
}

void WaxLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (t.panel.withMultipliedAlpha (box.isMouseOver() ? 1.6f : 1.0f));
    g.fillRoundedRectangle (r, jmin (t.corner * 0.6f, r.getHeight() * 0.5f));
    g.setColour (t.panelEdge);
    g.drawRoundedRectangle (r, jmin (t.corner * 0.6f, r.getHeight() * 0.5f), 1.0f);
    Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (t.textDim);
    g.fillPath (arrow);
}

Font WaxLookAndFeel::getComboBoxFont (ComboBox&) { return Font (FontOptions (13.0f)); }
Font WaxLookAndFeel::getTextButtonFont (TextButton&, int h) { return Font (FontOptions (jmin (15.0f, (float) h * 0.6f), Font::bold)); }
Font WaxLookAndFeel::getPopupMenuFont() { return Font (FontOptions (14.0f)); }
Font WaxLookAndFeel::getSliderPopupFont (Slider&) { return Font (FontOptions (13.0f, Font::bold)); }

void WaxLookAndFeel::drawBubble (Graphics& g, BubbleComponent&, const Point<float>&, const Rectangle<float>& body)
{
    g.setColour (findColour (TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle (body, 5.0f);
    g.setColour (t.accent);
    g.drawRoundedRectangle (body, 5.0f, 1.0f);
}
