#include "PluginEditor.h"

using namespace juce;

namespace
{
constexpr float kStart = MathConstants<float>::pi * -0.78f, kEnd = MathConstants<float>::pi * 0.78f;
constexpr int kKnob = 58;

Font titleFont (float size) { return Font (FontOptions (size, Font::bold)).withExtraKerningFactor (0.18f); }
bool isLightPanel (const Theme& t) { return t.panel.getPerceivedBrightness() > 0.6f && t.panel.getFloatAlpha() > 0.8f; }
bool isLightBackground (const Theme& t) { return t.bgTop.getPerceivedBrightness() > 0.6f; }
}

// ============================================================================ record view
RecordView::RecordView (WaxPressProcessor& p) : proc (p)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
}

float RecordView::radiusFor (float pos) const
{
    const float R = getWidth() * 0.447f;
    return R * (0.96f - jlimit (0.0f, 1.0f, pos) * (0.96f - 0.42f));
}

Point<float> RecordView::stylusPoint (float r) const
{
    // the stylus sits where the arm circle (around the pivot) crosses the groove circle
    const float R = getWidth() * 0.447f;
    const Point<float> c (getWidth() * 0.5f, getHeight() * 0.51f);
    const Point<float> pivot = c + Point<float> (R * 0.98f, -R * 0.86f);
    const float L = R * 1.18f;
    const auto dv = pivot - c;
    const float d = dv.getDistanceFromOrigin();
    const float cosT = jlimit (-1.0f, 1.0f, (d * d + r * r - L * L) / (2.0f * d * r));
    const float base = std::atan2 (dv.y, dv.x), th = std::acos (cosT);
    const Point<float> a = c + Point<float> (std::cos (base + th), std::sin (base + th)) * r;
    const Point<float> b = c + Point<float> (std::cos (base - th), std::sin (base - th)) * r;
    return a.x > b.x ? a : b;
}

void RecordView::tick (float rpm, float needlePos, float level, bool autoTravel)
{
    angle += rpm / 60.0f * MathConstants<float>::twoPi / 30.0f;
    if (angle > MathConstants<float>::twoPi) angle -= MathConstants<float>::twoPi;
    if (! dragging) needle = needlePos;
    shine = shine * 0.8f + 0.2f * level;
    autoOn = autoTravel;
    repaint();
}

void RecordView::paint (Graphics& g)
{
    const auto& t = theme;
    const float R = getWidth() * 0.447f;
    const Point<float> c (getWidth() * 0.5f, getHeight() * 0.51f);
    auto circle = [&] (float r) { return Rectangle<float> (r * 2, r * 2).withCentre (c); };

    // plinth and platter mat
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (circle (R * 1.04f).translated (4, 7));
    g.setColour (t.vinyl.interpolatedWith (Colours::grey, 0.25f).darker (0.4f));
    g.fillEllipse (circle (R * 1.035f));

    // vinyl
    g.setColour (t.vinyl.withAlpha (t.vinylAlpha));
    g.fillEllipse (circle (R));
    if (t.splatter)
    {
        Random rnd (12);
        Graphics::ScopedSaveState s (g);
        Path disc; disc.addEllipse (circle (R));
        g.reduceClipRegion (disc);
        for (int i = 0; i < 70; ++i)
        {
            const float a = rnd.nextFloat() * MathConstants<float>::twoPi + angle;
            const float rr = R * (0.36f + 0.62f * std::sqrt (rnd.nextFloat()));
            const float sz = 3.0f + 22.0f * rnd.nextFloat() * rnd.nextFloat();
            const auto p = c + Point<float> (std::cos (a), std::sin (a)) * rr;
            g.setColour ((i % 3 == 0 ? t.splatterB : t.splatterA).withAlpha (0.85f));
            g.fillEllipse (Rectangle<float> (sz * (1.0f + rnd.nextFloat()), sz).withCentre (p));
        }
    }
    // grooves, with a few track gaps
    const float played = radiusFor (needle);
    for (float r = R * 0.97f; r > R * 0.40f; r -= 2.2f)
    {
        const float gap = std::fmod (r / R * 7.3f, 1.0f);
        const bool trackGap = gap < 0.035f;
        g.setColour ((trackGap ? t.vinylShine : t.vinylGroove).withAlpha (trackGap ? 0.12f : (r > played ? 0.55f : 0.35f)));
        g.drawEllipse (circle (r), trackGap ? 1.6f : 0.8f);
    }
    // run-out groove and lead-out spiral
    g.setColour (t.vinylGroove.withAlpha (0.5f));
    g.drawEllipse (circle (R * 0.39f), 1.0f);

    // light: two fixed wedges of sheen (the light does not turn with the record); brighter with level
    for (float baseA : { -0.9f, MathConstants<float>::pi - 0.9f })
    {
        Path wedge;
        wedge.addPieSegment (circle (R * 0.985f), baseA, baseA + 0.55f, 0.42f);
        g.setGradientFill (ColourGradient (t.vinylShine.withAlpha (0.0f), c.x, c.y, t.vinylShine.withAlpha (0.07f + 0.22f * shine),
                                           c.x + R * std::sin (baseA + 0.27f), c.y - R * std::cos (baseA + 0.27f), true));
        g.fillPath (wedge);
    }
    g.setColour (t.vinylShine.withAlpha (0.18f));
    g.drawEllipse (circle (R), 1.2f);

    // label, turning with the record
    {
        Graphics::ScopedSaveState s (g);
        g.addTransform (AffineTransform::rotation (angle, c.x, c.y));
        const float lr = R * 0.34f;
        g.setColour (t.label);
        g.fillEllipse (circle (lr));
        g.setColour (t.labelText.withAlpha (0.5f));
        g.drawEllipse (circle (lr * 0.93f), 1.0f);
        g.setColour (t.labelText);
        g.setFont (titleFont (lr * 0.2f));
        g.drawText ("wax-press", Rectangle<float> (lr * 2, lr * 0.3f).withCentre (c.translated (0, -lr * 0.45f)), Justification::centred);
        g.setFont (Font (FontOptions (lr * 0.12f, Font::bold)));
        const int sp = (int) proc.apvts.getRawParameterValue ("speed")->load();
        g.drawText (sp == 2 ? "78 RPM" : (sp == 1 ? "45 RPM" : "33 1/3 RPM"), Rectangle<float> (lr * 2, lr * 0.2f).withCentre (c.translated (0, lr * 0.42f)), Justification::centred);
        g.drawText ("SIDE A", Rectangle<float> (lr * 2, lr * 0.2f).withCentre (c.translated (0, lr * 0.62f)), Justification::centred);
        if (t.name.startsWith ("Test"))
        {
            g.setColour (t.accent2.withAlpha (0.75f));
            g.setFont (titleFont (lr * 0.16f));
            g.drawText ("TEST PRESSING", Rectangle<float> (lr * 1.6f, lr * 0.26f).withCentre (c.translated (0, lr * 0.1f)), Justification::centred);
        }
    }
    g.setColour (Colour (0xffb9bcc2));
    g.fillEllipse (circle (R * 0.022f));

    // tonearm
    const Point<float> pivot = c + Point<float> (R * 0.98f, -R * 0.86f);
    const auto tip = stylusPoint (radiusFor (needle));
    const auto dir = (tip - pivot) / jmax (1.0f, tip.getDistanceFrom (pivot));
    const auto cw = pivot - dir * (R * 0.16f);
    g.setColour (Colours::black.withAlpha (0.3f));
    g.drawLine (Line<float> (cw, tip).withShortenedStart (0).toFloat(), 7.0f);   // soft shadow below
    g.setColour (t.armColour.darker (0.5f));
    g.fillEllipse (Rectangle<float> (R * 0.2f, R * 0.2f).withCentre (pivot));
    g.setColour (t.armColour);
    g.drawLine (Line<float> (cw, tip - dir * (R * 0.08f)), 4.0f);
    g.fillRoundedRectangle (Rectangle<float> (R * 0.1f, R * 0.12f).withCentre (cw), 3.0f);
    g.setColour (t.armColour.brighter (0.3f));
    g.fillEllipse (Rectangle<float> (R * 0.1f, R * 0.1f).withCentre (pivot));
    {
        // headshell
        Path head;
        head.addRoundedRectangle (-R * 0.035f, -R * 0.09f, R * 0.07f, R * 0.12f, 2.0f);
        g.setColour (t.armColour.darker (0.2f));
        g.fillPath (head, AffineTransform::rotation (std::atan2 (dir.y, dir.x) - MathConstants<float>::halfPi).translated (tip));
        g.setColour (t.accent);
        g.fillEllipse (Rectangle<float> (4.5f, 4.5f).withCentre (tip));
    }

    // caption
    g.setColour (isLightBackground (t) ? t.textDim : t.textDim.withAlpha (0.9f));
    g.setFont (Font (FontOptions (11.0f)));
    g.drawText (autoOn ? "AUTO TRAVEL: needle follows the song position" : "drag the arm to move the needle",
                getLocalBounds().removeFromBottom (14), Justification::centred);
}

void RecordView::setNeedleFromMouse (Point<float> p)
{
    const float R = getWidth() * 0.447f;
    const Point<float> c (getWidth() * 0.5f, getHeight() * 0.51f);
    const float r = p.getDistanceFrom (c) / R;
    const float pos = jlimit (0.0f, 1.0f, (0.96f - r) / (0.96f - 0.42f));
    needle = pos;
    if (auto* prm = proc.apvts.getParameter ("needle")) prm->setValueNotifyingHost (prm->convertTo0to1 (pos));
    repaint();
}

void RecordView::mouseDown (const MouseEvent& e)
{
    if (auto* prm = proc.apvts.getParameter ("needle")) prm->beginChangeGesture();
    dragging = true;
    setNeedleFromMouse (e.position);
}

void RecordView::mouseDrag (const MouseEvent& e) { setNeedleFromMouse (e.position); }

void RecordView::mouseUp (const MouseEvent&)
{
    dragging = false;
    if (auto* prm = proc.apvts.getParameter ("needle")) prm->endChangeGesture();
}

// ============================================================================ content
WaxPressContent::WaxPressContent (WaxPressProcessor& p) : proc (p), record (p)
{
    setLookAndFeel (&lnf);
    using namespace wp;
    addAndMakeVisible (record);

    for (auto& [param, label] : std::initializer_list<std::pair<int, const char*>> {
             { MonoBass, "Mono Bass" }, { HfLimit, "HF Limit" }, { CutLevel, "Level" },
             { Needle, "Needle" }, { SideLength, "Side" },
             { Tracking, "Tracking" }, { CartReso, "Cart Reso" }, { Crosstalk, "Crosstalk" }, { Riaa, "RIAA" },
             { Crackle, "Crackle" }, { Pops, "Pops" }, { Hiss, "Hiss" }, { Rumble, "Rumble" }, { Dust, "Dust" }, { Warp, "Warp" },
             { Mix, "Mix" }, { Output, "Output" } })
        addKnob (param, label);
    byParam[CutLevel]->slider.getProperties().set ("bipolar", true);
    byParam[Output]->slider.getProperties().set ("bipolar", true);

    addCombo (speedBox, Speed, speedAttach);
    addCombo (stylusBox, Stylus, stylusAttach);
    addCombo (modeBox, Mode, modeAttach);
    speedBox.setTooltip ("Groove speed: faster grooves trace better at the inner groove");
    stylusBox.setTooltip ("Stylus shape: elliptical traces short wavelengths best, worn worst");
    modeBox.setTooltip ("Mastering Check: honest amounts. Character: exaggerated inner-groove distortion, lacquer saturation, wider RIAA error");

    autoButton.setTooltip ("Auto travel: the needle moves inward with the host's song position over one side");
    addAndMakeVisible (autoButton);
    autoAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "auto_travel", autoButton);

    presetButton.onClick = [this] { showPresetMenu(); };
    prevButton.onClick = [this] { proc.stepPreset (-1); };
    nextButton.onClick = [this] { proc.stepPreset (1); };
    saveButton.onClick = [this] { savePresetDialog(); };
    presetButton.setTooltip ("Browse presets");
    saveButton.setTooltip ("Save the current settings as a user preset");
    for (auto* b : { &presetButton, &prevButton, &nextButton, &saveButton }) addAndMakeVisible (*b);

    for (int i = 0; i < (int) themes().size(); ++i) themeBox.addItem (themes()[(size_t) i].name, i + 1);
    themeBox.onChange = [this] { applyTheme (themeBox.getSelectedId() - 1); };
    addAndMakeVisible (themeBox);

    panels = { { "Cut", { 502, 76, 290, 186 } }, { "Groove", { 802, 76, 302, 186 } },
               { "Playback", { 502, 272, 360, 186 } }, { "Global", { 872, 272, 232, 186 } },
               { "Surface", { 502, 468, 602, 176 } } };

    setSize (W, H);
    applyTheme (jlimit (0, (int) themes().size() - 1, proc.themeIndex));
    themeBox.setSelectedId (proc.themeIndex + 1, dontSendNotification);
    timerCallback();
    startTimerHz (30);
}

WaxPressContent::~WaxPressContent()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

WaxPressContent::Knob& WaxPressContent::addKnob (int param, const char* label)
{
    const auto* defs = wp::paramDefs();
    auto k = std::make_unique<Knob>();
    k->param = param;
    k->label = label;
    k->slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    k->slider.setRotaryParameters (MathConstants<float>::twoPi + kStart, MathConstants<float>::twoPi + kEnd, true);
    k->slider.setPopupDisplayEnabled (true, true, this, 1200);
    k->slider.setTooltip (defs[param].name);
    addAndMakeVisible (k->slider);
    k->attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, defs[param].id, k->slider);
    if (auto* prm = proc.apvts.getParameter (defs[param].id))
        k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    byParam[param] = k.get();
    knobs.push_back (std::move (k));
    return *knobs.back();
}

void WaxPressContent::addCombo (ComboBox& box, int param, std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment>& att)
{
    const auto* defs = wp::paramDefs();
    box.addItemList (StringArray::fromTokens (defs[param].choices, "|", ""), 1);
    box.setJustificationType (Justification::centred);
    addAndMakeVisible (box);
    att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, defs[param].id, box);
}

void WaxPressContent::applyTheme (int index)
{
    index = jlimit (0, (int) themes().size() - 1, index);
    proc.themeIndex = index;
    const auto& t = themes()[(size_t) index];
    lnf.setTheme (t);
    record.setTheme (t);

    // pre-render the background at 2x: gradient plus the theme's texture
    background = Image (Image::ARGB, W * 2, H * 2, true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (2.0f));
    g.setGradientFill (ColourGradient (t.bgTop, 0, 0, t.bgBottom, (float) W * 0.3f, (float) H, false));
    g.fillAll();
    Random rnd (7);
    if (t.glow)
    {
        g.setGradientFill (ColourGradient (t.accent.withAlpha (0.18f), 250, 300, t.accent.withAlpha (0.0f), 250 + 380, 300, true));
        g.fillAll();
    }
    if (t.grain)
        for (int i = 0; i < W * H / 6; ++i)
        {
            g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (t.grainAmount * rnd.nextFloat()));
            g.fillRect (rnd.nextFloat() * W, rnd.nextFloat() * H, 1.0f, 1.0f);
        }

    for (auto* b : { &speedBox, &stylusBox, &modeBox }) b->setColour (ComboBox::textColourId, t.text);
    themeBox.setColour (ComboBox::textColourId, t.text);
    sendLookAndFeelChange();
    repaint();
}

// ---------------------------------------------------------------- layout
void WaxPressContent::placeRow (Rectangle<int> area, std::initializer_list<Knob*> row)
{
    const int n = (int) row.size();
    const int cw = area.getWidth() / jmax (1, n);
    int i = 0;
    for (auto* k : row)
    {
        auto cell = Rectangle<int> (area.getX() + i * cw, area.getY(), cw, area.getHeight());
        k->slider.setBounds (cell.withTrimmedBottom (30).withSizeKeepingCentre (kKnob, kKnob));
        ++i;
    }
}

void WaxPressContent::resized()
{
    using namespace wp;
    const int hy = 18;
    prevButton.setBounds (520, hy, 30, 32);
    presetButton.setBounds (554, hy, 270, 32);
    nextButton.setBounds (828, hy, 30, 32);
    saveButton.setBounds (866, hy, 58, 32);
    themeBox.setBounds (936, hy + 2, 168, 28);

    record.setBounds (16, 66, 470, 478);
    meterArea = Rectangle<float> (16, 552, 470, 92);

    auto body = [this] (int i) { return panels[(size_t) i].bounds.withTrimmedTop (40).reduced (8, 6); };
    auto head = [this] (int i) { return panels[(size_t) i].bounds.withHeight (38).reduced (10, 7); };

    placeRow (body (0), { byParam[MonoBass], byParam[HfLimit], byParam[CutLevel] });

    speedBox.setBounds (head (1).removeFromRight (74));
    {
        auto b = body (1);
        const int cw = b.getWidth() / 3;
        placeRow (b.withWidth (cw * 2), { byParam[Needle], byParam[SideLength] });
        autoCell = Rectangle<int> (b.getX() + cw * 2, b.getY(), cw, b.getHeight());
        autoButton.setBounds (autoCell.withTrimmedBottom (30).withSizeKeepingCentre (52, 24));
    }

    stylusBox.setBounds (head (2).removeFromRight (120));
    placeRow (body (2), { byParam[Tracking], byParam[CartReso], byParam[Crosstalk], byParam[Riaa] });

    {
        auto b = body (3);
        modeBox.setBounds (b.removeFromTop (30).reduced (4, 2));
        placeRow (b, { byParam[Mix], byParam[Output] });
    }

    placeRow (body (4), { byParam[Crackle], byParam[Pops], byParam[Hiss], byParam[Rumble], byParam[Dust], byParam[Warp] });
}

// ---------------------------------------------------------------- painting
void WaxPressContent::paint (Graphics& g)
{
    const auto& t = lnf.theme();
    g.drawImage (background, getLocalBounds().toFloat());

    const bool lightBg = isLightBackground (t);
    const auto headText = (isLightPanel (t) && ! lightBg) ? t.panel : t.text;
    g.setColour (headText);
    g.setFont (titleFont (30.0f));
    g.drawText ("wax-press", 22, 8, 300, 36, Justification::centredLeft);
    g.setColour (headText.withAlpha (0.65f));
    g.setFont (titleFont (11.0f));
    g.drawText ("VINYL MASTERING & PLAYBACK", 24, 42, 320, 16, Justification::centredLeft);

    for (size_t i = 0; i < panels.size(); ++i)
    {
        const auto& p = panels[i];
        const auto r = p.bounds.toFloat();
        g.setColour (t.panel);
        g.fillRoundedRectangle (r, t.corner);
        g.setColour (t.panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), t.corner, 1.0f);
        g.setColour (t.knobStyle == 1 ? t.accent2 : t.text);
        g.setFont (titleFont (13.0f));
        g.drawText (p.title.toUpperCase(), p.bounds.withHeight (38).reduced (12, 8), Justification::centredLeft);
        g.setColour (t.panelEdge.withMultipliedAlpha (0.6f));
        g.drawHorizontalLine (p.bounds.getY() + 38, r.getX() + 10, r.getRight() - 10);
    }

    for (auto& kp : knobs)
    {
        auto* k = kp.get();
        const auto sb = k->slider.getBounds();
        const bool en = k->slider.getAlpha() > 0.9f;
        g.setColour (en ? t.text.withAlpha (0.9f) : t.textDim.withAlpha (0.6f));
        g.setFont (Font (FontOptions (12.0f, Font::bold)));
        String label = k->label;
        if (k->param == wp::Needle && autoButton.getToggleState()) label = "Start";
        g.drawText (label, sb.getX() - 16, sb.getBottom() + 1, sb.getWidth() + 32, 14, Justification::centred);
        g.setColour (t.textDim);
        g.setFont (Font (FontOptions (11.0f)));
        g.drawText (k->slider.getTextFromValue (k->slider.getValue()), sb.getX() - 16, sb.getBottom() + 14, sb.getWidth() + 32, 13, Justification::centred);
    }
    // auto travel switch caption
    {
        const auto ab = autoButton.getBounds();
        g.setColour (t.text.withAlpha (0.9f));
        g.setFont (Font (FontOptions (12.0f, Font::bold)));
        g.drawText ("Auto", ab.getX() - 20, ab.getBottom() + 18, ab.getWidth() + 40, 14, Justification::centred);
        g.setColour (t.textDim);
        g.setFont (Font (FontOptions (11.0f)));
        g.drawText (autoButton.getToggleState() ? "follows host" : "off", ab.getX() - 30, ab.getBottom() + 31, ab.getWidth() + 60, 13, Justification::centred);
    }
    drawMeters (g, meterArea);
}

void WaxPressContent::drawMeters (Graphics& g, Rectangle<float> area)
{
    // lathe-style meter strip: lateral / vertical excursion, HF limiter, tracing error, warning lamp
    const auto& t = lnf.theme();
    g.setColour (t.panel);
    g.fillRoundedRectangle (area, t.corner);
    g.setColour (t.panelEdge);
    g.drawRoundedRectangle (area.reduced (0.5f), t.corner, 1.0f);

    auto inner = area.reduced (12, 10);
    auto lampArea = inner.removeFromRight (104);
    const float rowH = inner.getHeight() / 4.0f;
    auto toMeter = [] (float lin) { return jlimit (0.0f, 1.0f, (float) (Decibels::gainToDecibels (lin, -48.0f) + 48.0f) / 48.0f); };
    auto bar = [&] (int row, const String& name, float v, Colour col, const String& value)
    {
        auto r = Rectangle<float> (inner.getX(), inner.getY() + row * rowH, inner.getWidth(), rowH).reduced (0, 3);
        auto lab = r.removeFromLeft (70);
        auto val = r.removeFromRight (62);
        g.setColour (t.textDim);
        g.setFont (titleFont (9.5f));
        g.drawText (name, lab, Justification::centredLeft);
        g.setFont (Font (FontOptions (10.5f)));
        g.drawText (value, val, Justification::centredRight);
        g.setColour (t.track);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (col);
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * jlimit (0.0f, 1.0f, v)), 2.0f);
        // tick marks
        g.setColour (t.panel.withAlpha (0.6f));
        for (int i = 1; i < 8; ++i) g.fillRect (r.getX() + r.getWidth() * i / 8.0f, r.getY(), 1.0f, r.getHeight());
    };
    auto dbText = [] (float lin) { return lin < 0.0001f ? String ("-inf") : String (Decibels::gainToDecibels (lin), 1) + " dB"; };
    bar (0, "LATERAL", toMeter (lat), t.accent2, dbText (lat));
    bar (1, "VERTICAL", toMeter (vert), warn ? t.accent : t.accent2.withMultipliedAlpha (0.8f), dbText (vert));
    bar (2, "HF LIMIT", hfGr / 12.0f, t.accent, String (hfGr, 1) + " dB");
    bar (3, "TRACING", std::sqrt (dist), t.accent.interpolatedWith (t.accent2, 0.4f), String (roundToInt (dist * 100.0f)) + "%");

    // lamp: out-of-phase bass would make the cutter lift (vertical excursion)
    auto lamp = lampArea.reduced (10, 4);
    const auto lampCol = warn ? Colour (0xffff4a2a) : Colour (0xff3fbf6a);
    g.setColour (lampCol.withAlpha (warn ? 0.95f : 0.35f));
    const auto dot = Rectangle<float> (22, 22).withCentre ({ lamp.getCentreX(), lamp.getY() + 18 });
    if (warn) { g.setColour (lampCol.withAlpha (0.3f)); g.fillEllipse (dot.expanded (6)); g.setColour (lampCol); }
    g.fillEllipse (dot);
    g.setColour (t.text);
    g.setFont (titleFont (9.5f));
    g.drawText (warn ? "CUTTER LIFT" : "CUT OK", lamp.withTrimmedTop (34).withHeight (14), Justification::centred);
    g.setColour (t.textDim);
    g.setFont (Font (FontOptions (10.0f)));
    g.drawText (warn ? "mono the bass" : "vertical bass ok", lamp.withTrimmedTop (48).withHeight (14), Justification::centred);
}

// ---------------------------------------------------------------- presets
void WaxPressContent::showPresetMenu()
{
    PopupMenu menu;
    const auto& ps = wp::factoryPresets();
    for (const auto& bank : wp::presetBanks())
    {
        PopupMenu sub;
        for (size_t i = 0; i < ps.size(); ++i)
            if (ps[i].bank == bank) sub.addItem ((int) i + 1, ps[i].name, true, (int) i == proc.getCurrentProgram() && proc.getPresetName() == String (ps[i].name));
        menu.addSubMenu (bank, sub);
    }
    const auto users = proc.userPresets();
    PopupMenu userMenu;
    for (int i = 0; i < users.size(); ++i) userMenu.addItem (10000 + i, users[i].getFileNameWithoutExtension());
    if (users.isEmpty()) userMenu.addItem (-1, "(none saved yet)", false);
    menu.addSubMenu ("User", userMenu);
    menu.addSeparator();
    menu.addItem (20000, "Save current as...");
    menu.addItem (20001, "Show preset folder");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetButton).withMinimumWidth (presetButton.getWidth()),
                        [this, users] (int r)
                        {
                            if (r <= 0) return;
                            if (r < 10000) proc.loadFactoryPreset (r - 1);
                            else if (r < 20000) proc.loadUserPreset (users[r - 10000]);
                            else if (r == 20000) savePresetDialog();
                            else if (r == 20001)
                            {
                                WaxPressProcessor::userPresetFolder().createDirectory();
                                WaxPressProcessor::userPresetFolder().revealToUser();
                            }
                        });
}

void WaxPressContent::savePresetDialog()
{
    saveWindow = std::make_unique<AlertWindow> ("Save preset", "Name for this preset:", MessageBoxIconType::NoIcon, this);
    saveWindow->setLookAndFeel (&lnf);
    saveWindow->addTextEditor ("name", proc.getPresetName());
    saveWindow->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    saveWindow->enterModalState (true, ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveWindow != nullptr) proc.saveUserPreset (saveWindow->getTextEditorContents ("name"));
        saveWindow.reset();
    }), false);
}

// ---------------------------------------------------------------- animation and live labels
void WaxPressContent::timerCallback()
{
    const auto& e = proc.getEngine();
    lat = e.lateral.load(); vert = e.vertical.load(); vertLow = e.verticalLow.load();
    hfGr = hfGr * 0.6f + 0.4f * e.hfGainReduction.load();
    dist = dist * 0.7f + 0.3f * e.distortion.load();
    inM = e.inLevel.load(); outM = e.outLevel.load();
    warn = e.cutterWarning.load();

    const bool autoOn = autoButton.getToggleState();
    const int speed = (int) proc.apvts.getRawParameterValue ("speed")->load();
    const float rpm = (float) wp::rpmFor (speed);
    const float needleShown = autoOn ? e.needle.load() : proc.apvts.getRawParameterValue ("needle")->load();
    record.tick (rpm, needleShown, jlimit (0.0f, 1.0f, std::sqrt (outM) * 1.3f), autoOn);

    if (shownPreset != proc.getPresetName())
    {
        shownPreset = proc.getPresetName();
        const int idx = proc.getCurrentProgram();
        const auto& ps = wp::factoryPresets();
        String bank = "User";
        if (isPositiveAndBelow (idx, (int) ps.size()) && String (ps[(size_t) idx].name) == shownPreset) bank = ps[(size_t) idx].bank;
        presetButton.setButtonText (shownPreset + "   ·   " + bank);
    }
    byParam[wp::SideLength]->slider.setAlpha (autoOn ? 1.0f : 0.45f);
    const bool monoOff = proc.apvts.getRawParameterValue ("mono_bass")->load() <= wp::kMonoBassOff;
    byParam[wp::MonoBass]->slider.setAlpha (monoOff ? 0.6f : 1.0f);
    repaint();
}

// ============================================================================ editor
WaxPressEditor::WaxPressEditor (WaxPressProcessor& p) : AudioProcessorEditor (p), proc (p), content (p)
{
    addAndMakeVisible (content);
    const double ratio = (double) WaxPressContent::W / WaxPressContent::H;
    setResizable (true, true);
    setResizeLimits ((int) (WaxPressContent::W * 0.7), (int) (WaxPressContent::H * 0.7), WaxPressContent::W * 2, WaxPressContent::H * 2);
    getConstrainer()->setFixedAspectRatio (ratio);
    const float s = jlimit (0.7f, 2.0f, proc.uiScale);
    setSize (roundToInt (WaxPressContent::W * s), roundToInt (WaxPressContent::H * s));
}

void WaxPressEditor::resized()
{
    // fit inside the editor even when a host ignores the aspect ratio
    const float s = jmin ((float) getWidth() / (float) WaxPressContent::W, (float) getHeight() / (float) WaxPressContent::H);
    content.setTransform (AffineTransform::scale (s).translated ((getWidth() - WaxPressContent::W * s) * 0.5f, 0.0f));
    content.setBounds (0, 0, WaxPressContent::W, WaxPressContent::H);
    proc.uiScale = s;
}
