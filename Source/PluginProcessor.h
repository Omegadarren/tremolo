#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
/**
 * Tremolo  -  Three independent LFO-driven amplitude modulators.
 *
 * Modes:
 *   Mono       - All three LFOs apply to both L and R channels combined.
 *   Tri-Mono   - Each LFO modulates a different frequency band (low/mid/high)
 *                split with 6dB/oct Linkwitz-Riley crossovers. Bands are
 *                recombined before stereo output.
 *   Ping-Pong  - LFO-1 drives Left, LFO-2 drives Right, LFO-3 drives the
 *                centre (both) with the LFO-1/2 offset 90 degrees.
 *
 * LFO shapes: Sine, Triangle, Square, Saw-Up, Saw-Down, S&H (random), Line.
 *
 * Per-channel:  speed (0.1–20 Hz), intensity (0–100%),
 *               phase offset (0–360 degrees), shape selector.
 *
 * Global:       mix (0–100%), output gain (-40 to +6 dB),
 *               auto-gain (matches output RMS to input RMS).
 *
 * Parameters  (IDs used in APVTS):
 *   "speed0"   "speed1"   "speed2"
 *   "depth0"   "depth1"   "depth2"
 *   "phase0"   "phase1"   "phase2"
 *   "shape0"   "shape1"   "shape2"
 *   "mode"     "mix"      "outputGain"    "autoGain"
 */
class TremoloAudioProcessor : public juce::AudioProcessor
{
public:
    // ── Constants ─────────────────────────────────────────────────────────────
    static constexpr int kNumLFOs     = 3;
    static constexpr int kWaveTableN  = 2048;
    static constexpr int kHistorySize = 512;

    enum Mode { Mono = 0, PingPong = 1, DualTremolo = 2, TriTremolo = 3 };

    TremoloAudioProcessor();
    ~TremoloAudioProcessor() override = default;

    void prepareToPlay    (double sampleRate, int samplesPerBlock) override;
    void releaseResources () override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock     (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName()    const override { return JucePlugin_Name; }
    bool  acceptsMidi()             const override { return false; }
    bool  producesMidi()            const override { return false; }
    bool  isMidiEffect()            const override { return false; }
    double getTailLengthSeconds()   const override { return 0.0; }

    int  getNumPrograms()  override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    int editorZoomIndex = 0;

    // ── Thread-safe state for UI ───────────────────────────────────────────────
    std::atomic<float> inputLevelDb  { -120.f };
    std::atomic<float> outputLevelDb { -120.f };
    std::atomic<float> gainAppliedDb { 0.f };

    // Current LFO phases (read by display for animated waveforms)
    std::atomic<float> lfoPhaseOut[kNumLFOs] = {};

    // Modulation values this block (0..1, read by UI for real-time depth shading)
    std::atomic<float> modValue[kNumLFOs] = {};

    // Gain history ring buffer (gain applied, dB) for the scrolling graph
    std::array<float, kHistorySize> gainHistory {};
    std::atomic<int> historyWritePos { 0 };

private:
    double currentSampleRate = 44100.0;

    // ── Per-LFO state ──────────────────────────────────────────────────────────
    float lfoPhase[kNumLFOs]      = {};
    float lfoPhaseInc[kNumLFOs]   = {};

    // S&H state
    float shValue[kNumLFOs]       = {};
    float shPrevPhase[kNumLFOs]   = {};

    // Crossover filter state (kept for legacy; unused in current modes)
    float xLP1[2] = {}, xLP2[2] = {}, xHP1[2] = {}, xHP2[2] = {};

    // ── Auto-gain state ────────────────────────────────────────────────────────
    float rmsPowerIn       = 0.f;
    float rmsPowerOut      = 0.f;
    float autoGainSmooth   = 1.f;
    float rmsCoeff         = 0.f;
    float agCoeff          = 0.f;
    float inRmsSmooth      = 0.f;
    float outRmsSmooth     = 0.f;

    // ── LFO evaluation ─────────────────────────────────────────────────────────
    // shape: 0=Sine  1=Triangle  2=Square  3=Saw-Up  4=Saw-Down  5=S&H  6=Line
    float evalLFO (int lfoIdx, float phase, int shape) noexcept;

    static juce::AudioProcessorValueTreeState::ParameterLayout buildLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TremoloAudioProcessor)
};
