#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Ui/PlateLookAndFeel.h"

//==============================================================================
class TremoloAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit TremoloAudioProcessorEditor (TremoloAudioProcessor&);
    ~TremoloAudioProcessorEditor() override;

    void paint                  (juce::Graphics&) override;
    void resized                () override;
    void mouseDown              (const juce::MouseEvent&) override;
    void visibilityChanged      () override;
    void parentHierarchyChanged () override;

private:
    void timerCallback () override;
    void applyZoom     ();

    TremoloAudioProcessor& processorRef;
    std::unique_ptr<juce::LookAndFeel_V4> laf;
    std::unique_ptr<juce::Component>      lfoDisplay;   // LFODisplay defined in .cpp

    // ── Mode selector ─────────────────────────────────────────────────────────
    juce::ComboBox modeCombo;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<ComboAtt> modeAtt;

    // ── Per-LFO controls ──────────────────────────────────────────────────────
    static constexpr int kNumLFOs = TremoloAudioProcessor::kNumLFOs;

    struct LFOControls
    {
        juce::Slider     speedSlider { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label      speedLabel;
        juce::Slider     depthSlider { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label      depthLabel;
        juce::Slider     phaseSlider { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label      phaseLabel;
        juce::ComboBox   shapeCombo;
        juce::Label      shapeLabel;
        juce::Label      channelLabel;   // "LEFT" / "CENTER" / "RIGHT"
        juce::Slider     gainSlider { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label      gainLabel;
        juce::Slider     mixSlider { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label      mixLabel;
    };
    std::array<LFOControls, kNumLFOs> lfos;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::array<std::unique_ptr<SliderAtt>, kNumLFOs> speedAtts;
    std::array<std::unique_ptr<SliderAtt>, kNumLFOs> depthAtts;
    std::array<std::unique_ptr<SliderAtt>, kNumLFOs> phaseAtts;
    std::array<std::unique_ptr<SliderAtt>, kNumLFOs> gainAtts;
    std::array<std::unique_ptr<SliderAtt>, kNumLFOs> mixAtts;
    std::array<std::unique_ptr<ComboAtt>,  kNumLFOs> shapeAtts;

    // ── Global controls ────────────────────────────────────────────────────────
    juce::Slider     mixSlider      { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label      mixLabel;
    juce::Slider     gainSlider     { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label      gainLabel;
    juce::TextButton autoGainToggle { "AUTO GAIN" };

    std::unique_ptr<SliderAtt> mixAtt, gainAtt;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ButtonAtt> autoGainAtt;
    // ── Mode-dependent centre-column extras ───────────────────────────────
    juce::Slider panSlider      { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label  panLabel;
    juce::Slider pingWidthSlider { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label  pingWidthLabel;
    std::unique_ptr<SliderAtt> panAtt, pingWidthAtt;
    // ── Zoom ──────────────────────────────────────────────────────────────────
    int zoomIndex = 0;
    bool centred = false;
    static constexpr float       kZoomFactors[] = { 1.0f, 1.5f, 2.0f };
    static constexpr const char* kZoomLabels[]  = { "1x", "1.5x", "2x" };
    static constexpr int kBaseW = 860;
    static constexpr int kBaseH = 740;
    juce::Rectangle<int> zoomButtonBounds;

    juce::TooltipWindow tooltipWindow { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TremoloAudioProcessorEditor)
};
