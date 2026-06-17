#include "PluginEditor.h"

constexpr float       TremoloAudioProcessorEditor::kZoomFactors[];
constexpr const char* TremoloAudioProcessorEditor::kZoomLabels[];

//==============================================================================
//  Colour palette
//==============================================================================
static const juce::Colour kBg       {  20,  21,  32 };
static const juce::Colour kPanel    {  14,  15,  24 };
static const juce::Colour kHeader   {  22,  54,  98 };
static const juce::Colour kAccent   {  65, 145, 210 };
static const juce::Colour kTextMain { 225, 238, 255 };
static const juce::Colour kTextDim  { 115, 152, 195 };
static const juce::Colour kDivider  {  48,  82, 124 };

// LFO colours: Left=cyan-blue, Center=gold, Right=warm-orange
static const juce::Colour kLFOCol[3] = {
    juce::Colour ( 80, 185, 255),   // 0 – Left  (cyan-blue)
    juce::Colour (220, 190,  55),   // 1 – Center (gold)
    juce::Colour (255, 110,  80),   // 2 – Right  (warm orange)
};
static const char* kChannelNames[3] = { "LEFT", "CENTER", "RIGHT" };

//==============================================================================
//  Custom LookAndFeel
//==============================================================================
class TremoloLAF : public juce::LookAndFeel_V4
{
public:
    TremoloLAF()
    {
        setColour (juce::Slider::thumbColourId,               kAccent);
        setColour (juce::Slider::rotarySliderFillColourId,    kAccent);
        setColour (juce::Slider::rotarySliderOutlineColourId, kDivider);
        setColour (juce::Slider::textBoxTextColourId,         kTextDim);
        setColour (juce::Slider::textBoxBackgroundColourId,   kPanel.darker (0.3f));
        setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId,    kAccent.withAlpha (0.4f));
        setColour (juce::Label::textColourId,                 kTextMain);
        setColour (juce::Label::backgroundColourId,           juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId,              kTextMain);
        setColour (juce::ComboBox::backgroundColourId,        kPanel);
        setColour (juce::ComboBox::outlineColourId,           kDivider);
        setColour (juce::ComboBox::arrowColourId,             kTextDim);
        setColour (juce::PopupMenu::backgroundColourId,       juce::Colour (14, 16, 26));
        setColour (juce::PopupMenu::textColourId,             kTextMain);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha (0.28f));
        setColour (juce::TextButton::buttonColourId,          kPanel);
        setColour (juce::TextButton::buttonOnColourId,        kAccent.withAlpha (0.28f));
        setColour (juce::TextButton::textColourOnId,          kTextMain);
        setColour (juce::TextButton::textColourOffId,         kTextDim.withAlpha (0.55f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float startA, float endA, juce::Slider& s) override
    {
        const float cx = x + w * 0.5f, cy = y + h * 0.5f;
        const float r  = juce::jmin (w, h) * 0.5f - 4.f;

        // Determine colour from slider colour ID (we tint per-LFO knobs)
        juce::Colour fillCol = s.findColour (juce::Slider::rotarySliderFillColourId);

        // Track
        juce::Path arc;
        arc.addCentredArc (cx, cy, r, r, 0.f, startA, endA, true);
        g.setColour (kDivider.withAlpha (0.35f));
        g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        // Fill
        float fe = startA + pos * (endA - startA);
        juce::Path fill;
        fill.addCentredArc (cx, cy, r, r, 0.f, startA, fe, true);
        g.setColour (fillCol.withAlpha (0.80f));
        g.strokePath (fill, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        // Metallic body
        const float kr = r * 0.62f;
        juce::ColourGradient grad (
            juce::Colour (90, 98, 118), cx - kr * 0.4f, cy - kr * 0.5f,
            juce::Colour (28, 30, 42),  cx + kr * 0.4f, cy + kr * 0.5f, false);
        g.setGradientFill (grad); g.fillEllipse (cx-kr, cy-kr, kr*2, kr*2);
        g.setColour (kDivider.withAlpha (0.5f)); g.drawEllipse (cx-kr, cy-kr, kr*2, kr*2, 1.f);
        juce::ColourGradient rim (juce::Colours::white.withAlpha (0.18f), cx, cy-kr,
                                   juce::Colours::transparentBlack, cx, cy+kr, false);
        g.setGradientFill (rim); g.fillEllipse (cx-kr, cy-kr, kr*2, kr*2);
        // Thumb
        float a = startA + pos * (endA - startA);
        g.setColour (kTextMain.withAlpha (0.9f));
        g.drawLine (cx + (kr * 0.25f) * std::sin (a), cy - (kr * 0.25f) * std::cos (a),
                    cx + (kr - 4.f)  * std::sin (a), cy - (kr - 4.f)  * std::cos (a), 1.8f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                bool, bool) override
    {
        auto bounds = b.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (b.getToggleState() ? kAccent.withAlpha (0.28f) : kPanel);
        g.fillRoundedRectangle (bounds, 5.f);
        g.setColour (b.getToggleState() ? kAccent : kDivider.withAlpha (0.55f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.5f, 1.f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (juce::Font (10.f, juce::Font::bold));
        g.setColour (b.getToggleState() ? kTextMain : kTextDim.withAlpha (0.6f));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
    }

    juce::Font getLabelFont (juce::Label&) override { return juce::Font (11.5f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (11.5f, juce::Font::bold); }
    juce::Font getPopupMenuFont() override { return juce::Font (12.5f); }
};

//==============================================================================
//  LFO Waveform + Meter Display
//==============================================================================
class LFODisplay final : public juce::Component, private juce::Timer
{
public:
    explicit LFODisplay (TremoloAudioProcessor& p) : proc (p) { startTimerHz (60); }

private:
    void timerCallback() override { repaint(); }

    // Evaluate LFO waveform shape at phase (0..1), returns 0..1
    static float evalShape (float phase, int shape) noexcept
    {
        float p = phase - std::floor (phase);
        switch (shape) {
            case 0: return 0.5f + 0.5f * std::sin (2.f * juce::MathConstants<float>::pi * p);
            case 1: { float r = (p < 0.5f) ? (4.f*p - 1.f) : (3.f - 4.f*p); return 0.5f + 0.5f * r; }
            case 2: return p < 0.5f ? 1.f : 0.f;
            case 3: return p;
            case 4: return 1.f - p;
            case 5: return 0.5f;   // S&H shown as flat for display
            default: return 0.5f;
        }
    }

    // Draw a single VU bar
    static void drawVUBar (juce::Graphics& g, float x, float y, float w, float h, float db)
    {
        g.setColour (juce::Colour (3, 5, 12));
        g.fillRoundedRectangle (x, y, w, h, 2.f);
        g.setColour (kDivider.withAlpha (0.25f));
        g.drawRoundedRectangle (x, y, w, h, 2.f, 0.6f);
        float norm = juce::jlimit (0.f, 1.f, (juce::jlimit (-60.f, 6.f, db) + 60.f) / 66.f);
        if (norm > 0.002f)
        {
            float fillH = norm * h;
            juce::ColourGradient grad (
                juce::Colour (220, 55, 55), x, y,
                juce::Colour (65, 200, 80),  x, y + h, false);
            grad.addColour (0.55, juce::Colour (65, 200, 80));
            grad.addColour (0.70, juce::Colour (225, 200, 50));
            grad.addColour (0.84, juce::Colour (220, 120, 50));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (x, y + h - fillH, w, fillH, 2.f);
        }
        // 0dB tick
        float y0 = y + h * (1.f - 60.f / 66.f);
        g.setColour (kTextDim.withAlpha (0.30f));
        g.fillRect (x, y0, w, 0.8f);
    }

    void paint (juce::Graphics& g) override
    {
        const float bw = (float)getWidth();
        const float bh = (float)getHeight();

        // Background
        g.setColour (juce::Colour (3, 4, 10));
        g.fillRoundedRectangle (0, 0, bw, bh, 5.f);

        // Layout: VU left, waveform area, VU right
        const float vuW   = 18.f;
        const float vuPad = 5.f;
        const float vuY   = 8.f;
        const float vuH   = bh - 22.f;
        const float waveX = vuPad + vuW + 6.f;
        const float waveW = bw - 2.f * (vuPad + vuW + 6.f);
        const float waveY = 8.f, waveH = bh - 16.f;

        // ── Waveform display area ─────────────────────────────────────────────
        g.setColour (juce::Colour (1, 2, 8));
        g.fillRoundedRectangle (waveX, waveY, waveW, waveH, 4.f);

        // Centre line (0dB / 50% mod)
        const float midY = waveY + waveH * 0.5f;
        g.setColour (kDivider.withAlpha (0.28f));
        g.fillRect (waveX, midY, waveW, 0.8f);

        // Show 2 full cycles of waveform for each LFO
        const int   mode       = (int)proc.apvts.getRawParameterValue ("mode")->load();
        const int   kN         = 256;

        // Which LFOs are active per mode:
        // Mono(0)/PingPong(1): centre only; DualTremolo(2): L+R; TriTremolo(3): all
        const bool active[3] = {
            mode == 2 || mode == 3,   // i=0  LEFT
            mode == 0 || mode == 1 || mode == 3,  // i=1  CENTER
            mode == 2 || mode == 3    // i=2  RIGHT
        };

        // Per-LFO: draw waveform + animated playhead dot
        for (int i = 0; i < TremoloAudioProcessor::kNumLFOs; ++i)
        {
            if (!active[i]) continue;

            int   sh    = (int)proc.apvts.getRawParameterValue ("shape" + juce::String (i))->load();
            float ph0   = proc.apvts.getRawParameterValue ("phase" + juce::String (i))->load() / 360.f;
            float depth = proc.apvts.getRawParameterValue ("depth" + juce::String (i))->load() * 0.01f;
            float curPh = proc.lfoPhaseOut[i].load();   // live phase from audio thread

            juce::Colour col = kLFOCol[i];

            // Draw 2 cycles starting from current phase
            juce::Path waveLine;
            for (int p = 0; p <= kN; ++p)
            {
                float t   = (float)p / (float)kN * 2.0f;   // 0..2 cycles
                float ph  = curPh + ph0 + t;
                float val = evalShape (ph, sh);
                // Apply depth: centre around 0.5, scale by depth
                float mod = 1.f - depth * (1.f - val);
                float px  = waveX + (float)p / (float)kN * waveW;
                float py  = waveY + waveH * (1.f - mod);
                if (p == 0) waveLine.startNewSubPath (px, py);
                else        waveLine.lineTo (px, py);
            }

            // Glow + line
            g.setColour (col.withAlpha (0.12f));
            g.strokePath (waveLine, juce::PathStrokeType (5.f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
            g.setColour (col.withAlpha (0.80f));
            g.strokePath (waveLine, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));

            // Animated playhead dot at current phase (left edge of waveform)
            float curVal = evalShape (curPh + ph0, sh);
            float curMod = 1.f - depth * (1.f - curVal);
            float dotX   = waveX + 2.f;
            float dotY   = waveY + waveH * (1.f - curMod);
            g.setColour (col.withAlpha (0.28f));
            g.fillEllipse (dotX - 7.f, dotY - 7.f, 14.f, 14.f);
            g.setColour (col.withAlpha (0.90f));
            g.fillEllipse (dotX - 4.f, dotY - 4.f, 8.f, 8.f);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.fillEllipse (dotX - 2.f, dotY - 2.f, 4.f, 4.f);
        }

        // Channel / mode labels inside display
        static const char* kDualLabels[] = { "L",   "(off)", "R"   };
        static const char* kTriLabels[]  = { "L",   "C",     "R"   };
        static const char* kPPLabels[]   = { "(off)","C↔LR","(off)"};
        static const char* kMonoLabels[] = { "(off)","C+PAN","(off)"};
        const char** modeLabels = (mode == 0) ? kMonoLabels
                                : (mode == 1) ? kPPLabels
                                : (mode == 2) ? kDualLabels : kTriLabels;
        for (int i = 0; i < TremoloAudioProcessor::kNumLFOs; ++i)
        {
            g.setFont (juce::Font (8.f, juce::Font::bold));
            float alpha = (modeLabels[i][0] == '(') ? 0.28f : 0.55f;
            g.setColour (kLFOCol[i].withAlpha (alpha));
            g.drawText (modeLabels[i], (int)(waveX + 3), (int)(waveY + 3 + i * 13), 48, 12,
                        juce::Justification::centredLeft, false);
        }

        // Waveform border
        g.setColour (kDivider.withAlpha (0.30f));
        g.drawRoundedRectangle (waveX, waveY, waveW, waveH, 4.f, 0.8f);

        // ── VU meters ──────────────────────────────────────────────────────────
        drawVUBar (g, vuPad, vuY, vuW, vuH, proc.inputLevelDb.load());
        g.setFont (juce::Font (7.5f, juce::Font::bold));
        g.setColour (kTextDim.withAlpha (0.45f));
        g.drawText ("IN",  (int)vuPad,       (int)(vuY + vuH + 3), (int)vuW, 12, juce::Justification::centred, false);

        drawVUBar (g, bw - vuPad - vuW, vuY, vuW, vuH, proc.outputLevelDb.load());
        g.drawText ("OUT", (int)(bw-vuPad-vuW), (int)(vuY + vuH + 3), (int)vuW, 12, juce::Justification::centred, false);

        // ── Gain strip ─────────────────────────────────────────────────────────
        float gainDb = proc.gainAppliedDb.load();
        juce::String gainTxt = (gainDb > -119.f)
            ? (gainDb > 0.05f ? juce::String ("+") : juce::String (""))
              + juce::String (gainDb, 1) + " dB"
            : "-inf";
        g.setFont (juce::Font (7.5f));
        g.setColour (kTextDim.withAlpha (0.40f));
        g.drawText ("MOD: " + gainTxt,
                    (int)waveX, (int)(waveY + waveH - 13), (int)waveW, 12,
                    juce::Justification::centredRight, false);

        // ── Border ─────────────────────────────────────────────────────────────
        g.setColour (kDivider.withAlpha (0.28f));
        g.drawRoundedRectangle (0.5f, 0.5f, bw - 1.f, bh - 1.f, 4.5f, 0.8f);
    }

    TremoloAudioProcessor& proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LFODisplay)
};

//==============================================================================
//  Logo — 3 sine curves
//==============================================================================
static void drawLogoIcon (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (kAccent.withAlpha (0.15f));
    g.fillEllipse (r);
    g.setColour (kDivider.withAlpha (0.5f));
    g.drawEllipse (r.reduced (0.5f), 0.8f);
    const float cx = r.getCentreX(), cy = r.getCentreY();
    const float hw = r.getWidth() * 0.38f, hh = r.getHeight() * 0.22f;
    for (int i = 0; i < 3; ++i)
    {
        juce::Path p;
        float oy = cy + (i - 1) * hh * 1.2f;
        for (int k = 0; k <= 24; ++k)
        {
            float t = (float)k / 24.f;
            float x = cx - hw + t * 2.f * hw;
            float y = oy - hh * 0.55f * std::sin (t * 2.f * juce::MathConstants<float>::pi);
            if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (kLFOCol[i].withAlpha (0.80f));
        g.strokePath (p, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }
}

//==============================================================================
//  Editor constructor
//==============================================================================
TremoloAudioProcessorEditor::TremoloAudioProcessorEditor (TremoloAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    laf = std::make_unique<TremoloLAF>();
    setLookAndFeel (laf.get());

    lfoDisplay = std::make_unique<LFODisplay> (p);
    addAndMakeVisible (*lfoDisplay);

    // Mode combo
    modeCombo.addItemList ({ "Mono", "Ping Pong", "Dual Tremolo", "Tri Tremolo" }, 1);
    modeCombo.setTooltip ("Mono: one LFO on both channels with pan control | "
                          "Ping Pong: LFO sweeps between L and R | "
                          "Dual Tremolo: independent LFOs on L and R | "
                          "Tri Tremolo: all 3 LFOs independent (L / both / R)");
    addAndMakeVisible (modeCombo);
    modeAtt = std::make_unique<ComboAtt> (p.apvts, "mode", modeCombo);

    // Per-LFO controls
    static const char* kShapes[] = { "Sine", "Triangle", "Square", "Saw Up", "Saw Down", "S&H" };

    for (int i = 0; i < kNumLFOs; ++i)
    {
        auto& lc = lfos[i];
        juce::Colour col = kLFOCol[i];
        juce::String si (i);

        // Channel label
        lc.channelLabel.setText (kChannelNames[i], juce::dontSendNotification);
        lc.channelLabel.setFont (juce::Font (11.f, juce::Font::bold));
        lc.channelLabel.setColour (juce::Label::textColourId, col);
        lc.channelLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lc.channelLabel);

        // Speed
        lc.speedSlider.setTextValueSuffix (" Hz");
        lc.speedSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 13);
        lc.speedSlider.setTooltip ("LFO " + si + " speed in Hz");
        lc.speedSlider.setColour (juce::Slider::rotarySliderFillColourId, col);
        addAndMakeVisible (lc.speedSlider);
        lc.speedLabel.setText ("SPEED", juce::dontSendNotification);
        lc.speedLabel.setFont (juce::Font (8.5f)); lc.speedLabel.setColour (juce::Label::textColourId, kTextDim);
        lc.speedLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lc.speedLabel);
        speedAtts[i] = std::make_unique<SliderAtt> (p.apvts, "speed" + si, lc.speedSlider);

        // Depth
        lc.depthSlider.setTextValueSuffix (" %");
        lc.depthSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 13);
        lc.depthSlider.setTooltip ("LFO " + si + " depth (modulation intensity)");
        lc.depthSlider.setColour (juce::Slider::rotarySliderFillColourId, col);
        addAndMakeVisible (lc.depthSlider);
        lc.depthLabel.setText ("DEPTH", juce::dontSendNotification);
        lc.depthLabel.setFont (juce::Font (8.5f)); lc.depthLabel.setColour (juce::Label::textColourId, kTextDim);
        lc.depthLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lc.depthLabel);
        depthAtts[i] = std::make_unique<SliderAtt> (p.apvts, "depth" + si, lc.depthSlider);

        // Phase
        lc.phaseSlider.setTextValueSuffix (" deg");
        lc.phaseSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 13);
        lc.phaseSlider.setTooltip ("LFO " + si + " phase offset (0-360 deg)");
        lc.phaseSlider.setColour (juce::Slider::rotarySliderFillColourId, col.withAlpha (0.6f));
        addAndMakeVisible (lc.phaseSlider);
        lc.phaseLabel.setText ("PHASE", juce::dontSendNotification);
        lc.phaseLabel.setFont (juce::Font (8.5f)); lc.phaseLabel.setColour (juce::Label::textColourId, kTextDim);
        lc.phaseLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lc.phaseLabel);
        phaseAtts[i] = std::make_unique<SliderAtt> (p.apvts, "phase" + si, lc.phaseSlider);

        // Shape
        for (int s = 0; s < 6; ++s) lc.shapeCombo.addItem (kShapes[s], s + 1);
        lc.shapeCombo.setTooltip ("LFO " + si + " waveform shape");
        addAndMakeVisible (lc.shapeCombo);
        lc.shapeLabel.setText ("SHAPE", juce::dontSendNotification);
        lc.shapeLabel.setFont (juce::Font (8.5f)); lc.shapeLabel.setColour (juce::Label::textColourId, kTextDim);
        lc.shapeLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lc.shapeLabel);
        shapeAtts[i] = std::make_unique<ComboAtt> (p.apvts, "shape" + si, lc.shapeCombo);

        // Per-column Gain
        lc.gainSlider.setTextValueSuffix (" %");
        lc.gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 13);
        lc.gainSlider.setTooltip ("Gain for " + juce::String (kChannelNames[i]) + " column (0-100%)");
        lc.gainSlider.setColour (juce::Slider::rotarySliderFillColourId, col.withAlpha (0.75f));
        addAndMakeVisible (lc.gainSlider);
        lc.gainLabel.setText ("GAIN", juce::dontSendNotification);
        lc.gainLabel.setFont (juce::Font (8.5f)); lc.gainLabel.setColour (juce::Label::textColourId, kTextDim);
        lc.gainLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lc.gainLabel);
        gainAtts[i] = std::make_unique<SliderAtt> (p.apvts, "gain" + juce::String (i == 0 ? "L" : i == 1 ? "C" : "R"), lc.gainSlider);

        // Per-column Mix
        lc.mixSlider.setTextValueSuffix (" %");
        lc.mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 13);
        lc.mixSlider.setTooltip ("Wet/Dry mix for " + juce::String (kChannelNames[i]) + " column (0-100%)");
        lc.mixSlider.setColour (juce::Slider::rotarySliderFillColourId, col.withAlpha (0.5f));
        addAndMakeVisible (lc.mixSlider);
        lc.mixLabel.setText ("MIX", juce::dontSendNotification);
        lc.mixLabel.setFont (juce::Font (8.5f)); lc.mixLabel.setColour (juce::Label::textColourId, kTextDim);
        lc.mixLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lc.mixLabel);
        mixAtts[i] = std::make_unique<SliderAtt> (p.apvts, "mix" + juce::String (i == 0 ? "L" : i == 1 ? "C" : "R"), lc.mixSlider);
    }

    // Global controls
    mixSlider.setTextValueSuffix (" %");
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 13);
    mixSlider.setTooltip ("Wet/Dry mix: 0% = no tremolo, 100% = full tremolo");
    addAndMakeVisible (mixSlider);
    mixLabel.setText ("MIX", juce::dontSendNotification);
    mixLabel.setFont (juce::Font (8.5f)); mixLabel.setColour (juce::Label::textColourId, kTextDim);
    mixLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (mixLabel);
    mixAtt = std::make_unique<SliderAtt> (p.apvts, "mix", mixSlider);

    gainSlider.setTextValueSuffix (" dB");
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 13);
    gainSlider.setTooltip ("Output gain trim (-40 to +6 dB)");
    addAndMakeVisible (gainSlider);
    gainLabel.setText ("GAIN", juce::dontSendNotification);
    gainLabel.setFont (juce::Font (8.5f)); gainLabel.setColour (juce::Label::textColourId, kTextDim);
    gainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (gainLabel);
    gainAtt = std::make_unique<SliderAtt> (p.apvts, "outputGain", gainSlider);

    autoGainToggle.setClickingTogglesState (true);
    autoGainToggle.setTooltip ("Auto Gain: automatically compensates for volume reduction "
                               "caused by tremolo modulation");
    addAndMakeVisible (autoGainToggle);
    autoGainAtt = std::make_unique<ButtonAtt> (p.apvts, "autoGain", autoGainToggle);

    // Pan knob (Mono mode, shown in CENTER column)
    panSlider.setTextValueSuffix (" %");
    panSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 13);
    panSlider.setTooltip ("Pan the tremolo effect: left channel gets more modulation when panned left, right channel when panned right");
    panSlider.setColour (juce::Slider::rotarySliderFillColourId, kLFOCol[1]);
    addAndMakeVisible (panSlider);
    panLabel.setText ("PAN", juce::dontSendNotification);
    panLabel.setFont (juce::Font (8.5f));
    panLabel.setColour (juce::Label::textColourId, kTextDim);
    panLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (panLabel);
    panAtt = std::make_unique<SliderAtt> (p.apvts, "pan", panSlider);

    // Ping Width knob (Ping Pong mode, shown in CENTER column)
    pingWidthSlider.setTextValueSuffix (" %");
    pingWidthSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 13);
    pingWidthSlider.setTooltip ("Width of the ping-pong sweep: 0% = no L/R difference, 100% = full alternation");
    pingWidthSlider.setColour (juce::Slider::rotarySliderFillColourId, kLFOCol[1]);
    addAndMakeVisible (pingWidthSlider);
    pingWidthLabel.setText ("WIDTH", juce::dontSendNotification);
    pingWidthLabel.setFont (juce::Font (8.5f));
    pingWidthLabel.setColour (juce::Label::textColourId, kTextDim);
    pingWidthLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (pingWidthLabel);
    pingWidthAtt = std::make_unique<SliderAtt> (p.apvts, "pingWidth", pingWidthSlider);

    zoomIndex = p.editorZoomIndex;
    setSize (kBaseW, kBaseH);
    applyZoom();
    startTimerHz (60);
}

TremoloAudioProcessorEditor::~TremoloAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void TremoloAudioProcessorEditor::visibilityChanged()     { if (isVisible()) applyZoom(); }
void TremoloAudioProcessorEditor::parentHierarchyChanged(){ applyZoom(); }
void TremoloAudioProcessorEditor::applyZoom()             { if (getPeer()) setScaleFactor (kZoomFactors[zoomIndex]); }
void TremoloAudioProcessorEditor::timerCallback()
{
    const int mode = (int)processorRef.apvts.getRawParameterValue ("mode")->load();

    // Which LFO columns are active per mode
    // LFO0=Left, LFO1=Center, LFO2=Right
    bool active[3];
    active[0] = (mode == 2 || mode == 3);  // Dual + Tri
    active[1] = (mode == 0 || mode == 1 || mode == 3);  // Mono, PingPong, Tri
    active[2] = (mode == 2 || mode == 3);  // Dual + Tri

    for (int i = 0; i < kNumLFOs; ++i)
    {
        auto& lc = lfos[i];
        lc.speedSlider .setEnabled (active[i]);
        lc.depthSlider .setEnabled (active[i]);
        lc.phaseSlider .setEnabled (active[i]);
        lc.shapeCombo  .setEnabled (active[i]);
        lc.channelLabel.setEnabled (active[i]);
        lc.gainSlider  .setEnabled (active[i]);
        lc.mixSlider   .setEnabled (active[i]);
    }

    // Show pan knob only in Mono mode; width only in Ping Pong
    panSlider    .setVisible (mode == 0);
    panLabel     .setVisible (mode == 0);
    pingWidthSlider.setVisible (mode == 1);
    pingWidthLabel .setVisible (mode == 1);

    repaint();
}

//==============================================================================
void TremoloAudioProcessorEditor::resized()
{
    const int W = kBaseW;

    zoomButtonBounds = { 8, 12, 38, 26 };

    // Mode combo: right of zoom, in header
    modeCombo.setBounds (W - 160, 12, 128, 26);

    // LFO display: y=52, height=220
    lfoDisplay->setBounds (8, 52, W - 16, 220);

    // Band section: y=278 to kBaseH-8
    // 3 LFO columns across W-16px
    const int   BS   = 278;           // band section top y
    const float cw   = (W - 16.f) / 3.f;
    const int   kW   = 64;            // knob width
    const int   half = kW / 2;

    for (int i = 0; i < kNumLFOs; ++i)
    {
        auto& lc = lfos[i];
        int cx = (int)(8.f + (i + 0.5f) * cw);

        lc.channelLabel .setBounds (cx - 40,   BS + 2,   80, 14);
        lc.shapeLabel   .setBounds (cx - 40,   BS + 16,  80, 12);
        lc.shapeCombo   .setBounds (cx - 40,   BS + 28,  80, 22);

        lc.speedLabel   .setBounds (cx - 76, BS + 56,  kW, 12);
        lc.speedSlider  .setBounds (cx - 76, BS + 68,  kW, 76);

        lc.depthLabel   .setBounds (cx + 12, BS + 56,  kW, 12);
        lc.depthSlider  .setBounds (cx + 12, BS + 68,  kW, 76);

        lc.phaseLabel   .setBounds (cx - 76, BS + 152, kW, 12);
        lc.phaseSlider  .setBounds (cx - 76, BS + 164, kW, 76);

        lc.gainSlider   .setBounds (cx - 76, BS + 246, kW, 64);
        lc.gainLabel    .setBounds (cx - 76, BS + 310, kW, 12);

        lc.mixSlider    .setBounds (cx + 12, BS + 246, kW, 64);
        lc.mixLabel     .setBounds (cx + 12, BS + 310, kW, 12);

        // Pan (Mono) / Width (PingPong) knob in CENTER column only (i==1)
        if (i == 1)
        {
            panSlider       .setBounds (cx - 32, BS + 246, kW, 64);
            panLabel        .setBounds (cx - 32, BS + 310, kW, 12);
            pingWidthSlider .setBounds (cx - 32, BS + 246, kW, 64);
            pingWidthLabel  .setBounds (cx - 32, BS + 310, kW, 12);
        }
    }

    // Global controls: bottom strip — below pan/width knob (ends at BS+322)
    const int GY = BS + 330;
    const int gcx = W / 2;

    mixSlider .setBounds (gcx - 130, GY, 64, 64);
    mixLabel  .setBounds (gcx - 130, GY + 64, 64, 12);
    gainSlider.setBounds (gcx +  66, GY, 64, 64);
    gainLabel .setBounds (gcx +  66, GY + 64, 64, 12);

    autoGainToggle.setBounds (gcx - 46, GY + 16, 92, 26);
}

//==============================================================================
void TremoloAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (zoomButtonBounds.contains (e.position.toInt()))
    {
        zoomIndex = (zoomIndex + 1) % 3;
        processorRef.editorZoomIndex = zoomIndex;
        applyZoom();
        repaint();
    }
}

//==============================================================================
void TremoloAudioProcessorEditor::paint (juce::Graphics& g)
{
    const int W = kBaseW, H = kBaseH;

    // ── Background ───────────────────────────────────────────────────────────
    g.setColour (kBg);
    g.fillAll();

    // ── Header ───────────────────────────────────────────────────────────────
    {
        juce::ColourGradient hdr (kHeader.brighter (0.05f), 0.f, 0.f,
                                   kHeader.darker   (0.25f), 0.f, 50.f, false);
        g.setGradientFill (hdr);
        g.fillRect (0, 0, W, 50);
        g.setColour (kDivider.withAlpha (0.45f));
        g.fillRect (0, 49, W, 1);
    }

    // ── Zoom button ──────────────────────────────────────────────────────────
    {
        auto& zb = zoomButtonBounds;
        g.setColour (kPanel.withAlpha (0.65f));
        g.fillRoundedRectangle (zb.toFloat(), 5.f);
        g.setColour (kDivider.withAlpha (0.55f));
        g.drawRoundedRectangle (zb.toFloat().reduced (0.5f), 4.5f, 0.8f);
        g.setFont (juce::Font (10.5f, juce::Font::bold));
        g.setColour (kTextMain);
        g.drawText (kZoomLabels[zoomIndex], zb, juce::Justification::centred, false);
    }

    // ── Title "TREMOLO" ───────────────────────────────────────────────────────
    {
        juce::Font tf (20.f, juce::Font::bold);
        g.setFont (tf);
        const juce::String p1 = "TREM", p2 = "OLO";
        float w1 = tf.getStringWidthFloat (p1), w2 = tf.getStringWidthFloat (p2);
        float sx = (W - w1 - w2) * 0.5f;
        g.setColour (kTextMain.withAlpha (0.72f));
        g.drawText (p1, (int)sx,       18, (int)w1 + 4, 18, juce::Justification::centredLeft, false);
        g.setColour (kAccent);
        g.drawText (p2, (int)(sx + w1), 18, (int)w2 + 4, 18, juce::Justification::centredLeft, false);
    }

    // ── Mode label ───────────────────────────────────────────────────────────
    g.setFont (juce::Font (9.5f));
    g.setColour (kTextDim.withAlpha (0.55f));
    g.drawText ("MODE", W - 170, 14, 10, 16, juce::Justification::centredLeft, false);

    // ── Version + Logo ───────────────────────────────────────────────────────
    g.setFont (juce::Font (8.5f));
    g.setColour (kTextDim.withAlpha (0.50f));
    g.drawText ("v1.1", W - 52, 38, 40, 10, juce::Justification::centredRight, false);
    drawLogoIcon (g, { (float)(W - 44), 6.f, 36.f, 36.f });

    // ── LFO column panels ─────────────────────────────────────────────────────
    const int   BS  = 278;
    const float cw  = (W - 16.f) / 3.f;
    {
        juce::Rectangle<float> panel (6.f, (float)(BS - 4), W - 12.f, kBaseH - BS);
        g.setColour (kPanel.withAlpha (0.55f));
        g.fillRoundedRectangle (panel, 6.f);
        g.setColour (kDivider.withAlpha (0.28f));
        g.drawRoundedRectangle (panel.reduced (0.5f), 5.5f, 0.8f);

        // Column separators
        for (int i = 1; i < 3; ++i)
        {
            float x = 8.f + i * cw;
            g.setColour (kDivider.withAlpha (0.18f));
            g.fillRect (x, (float)(BS + 4), 0.6f, (float)(kBaseH - BS - 12));
        }

        // Coloured top accent bar per column
        for (int i = 0; i < 3; ++i)
        {
            float cx2 = 8.f + i * cw;
            g.setColour (kLFOCol[i].withAlpha (0.35f));
            g.fillRoundedRectangle (cx2 + 4.f, (float)BS - 3.f, cw - 8.f, 3.f, 1.5f);
        }

        // ── Global strip divider ─────────────────────────────────────────────
        const int GY = BS + 258;
        g.setColour (kDivider.withAlpha (0.22f));
        g.fillRect (8.f, (float)(GY - 5), (float)(W - 16), 0.6f);

        // Global section labels
        g.setFont (juce::Font (8.5f, juce::Font::bold));
        g.setColour (kTextDim.withAlpha (0.45f));
        g.drawText ("GLOBAL", 8, GY - 15, W - 16, 12, juce::Justification::centred, false);
    }
}
