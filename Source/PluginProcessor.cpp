#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
TremoloAudioProcessor::buildLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Per-LFO params
    for (int i = 0; i < kNumLFOs; ++i)
    {
        juce::String si (i);

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            "speed" + si, "LFO " + si + " Speed",
            juce::NormalisableRange<float> (0.1f, 20.0f, 0.01f, 0.4f), 1.0f + i * 0.5f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            "depth" + si, "LFO " + si + " Depth",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 80.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            "phase" + si, "LFO " + si + " Phase",
            juce::NormalisableRange<float> (0.0f, 360.0f, 1.0f), (float)(i * 120),
            juce::AudioParameterFloatAttributes().withLabel ("deg")));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            "shape" + si, "LFO " + si + " Shape",
            juce::StringArray { "Sine", "Triangle", "Square", "Saw Up",
                                "Saw Down", "S&H" },
            0));
    }

    // Mode
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "mode", "Mode",
        juce::StringArray { "Mono", "Ping Pong", "Dual Tremolo", "Tri Tremolo" }, 0));

    // Mono-mode pan: −100 (full left) to +100 (full right)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "pan", "Pan",
        juce::NormalisableRange<float> (-100.f, 100.f, 1.f), 0.f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Ping-pong width: 0% = centred, 100% = full L↔R bounce
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "pingWidth", "Ping Width",
        juce::NormalisableRange<float> (0.f, 100.f, 1.f), 100.f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Global
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "mix", "Mix",
        juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "outputGain", "Output Gain",
        juce::NormalisableRange<float> (-40.f, 6.f, 0.1f), 0.f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "autoGain", "Auto Gain", false));

    // Per-column gain and mix (L, C, R)
    for (int i = 0; i < 3; ++i)
    {
        juce::String col = (i == 0) ? "L" : (i == 1) ? "C" : "R";
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            "gain" + col, "Gain " + col,
            juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            "mix" + col, "Mix " + col,
            juce::NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
    }

    return layout;
}

//==============================================================================
TremoloAudioProcessor::TremoloAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    , apvts (*this, nullptr, "Parameters", buildLayout())
{}

//==============================================================================
bool TremoloAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void TremoloAudioProcessor::prepareToPlay (double sr, int)
{
    currentSampleRate = sr;

    // Reset LFO phases (apply start-phase offsets after first block)
    for (int i = 0; i < kNumLFOs; ++i)
    {
        float phDeg = apvts.getRawParameterValue ("phase" + juce::String (i))->load();
        lfoPhase[i]    = phDeg / 360.f;
        shValue[i]     = 0.f;
        shPrevPhase[i] = lfoPhase[i];
    }

    // Crossover filter state (no longer used but reset for safety)
    for (int c = 0; c < 2; ++c) xLP1[c] = xLP2[c] = xHP1[c] = xHP2[c] = 0.f;

    // Auto-gain: 300ms RMS, 600ms smoothing
    rmsCoeff       = std::exp (-1.f / (0.30f * (float)sr));
    agCoeff        = std::exp (-1.f / (0.60f * (float)sr));
    rmsPowerIn     = rmsPowerOut = 0.f;
    autoGainSmooth = 1.f;
    inRmsSmooth    = outRmsSmooth = 0.f;

    gainHistory.fill (0.f);
    historyWritePos.store (0);
}

void TremoloAudioProcessor::releaseResources() {}

//==============================================================================
float TremoloAudioProcessor::evalLFO (int idx, float phase, int shape) noexcept
{
    // Returns unipolar value 0..1 (0 = silence, 1 = full level)
    // Phase is 0..1
    float p = phase - std::floor (phase);   // wrap to [0,1)

    float raw = 0.f;
    switch (shape)
    {
        case 0: // Sine: bipolar -1..1, map to unipolar
            raw = std::sin (2.f * juce::MathConstants<float>::pi * p);
            return 0.5f + 0.5f * raw;

        case 1: // Triangle
            raw = (p < 0.5f) ? (4.f * p - 1.f) : (3.f - 4.f * p);
            return 0.5f + 0.5f * raw;

        case 2: // Square (50% duty)
            return p < 0.5f ? 1.f : 0.f;

        case 3: // Saw Up
            return p;

        case 4: // Saw Down
            return 1.f - p;

        case 5: // S&H — new random value every cycle
        {
            // Detect phase wrap
            if (p < shPrevPhase[idx])
                shValue[idx] = juce::Random::getSystemRandom().nextFloat();
            shPrevPhase[idx] = p;
            return 0.5f + 0.5f * (shValue[idx] * 2.f - 1.f);
        }
        default: return 0.5f;
    }
}

//==============================================================================
void TremoloAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int   N         = buffer.getNumSamples();
    const int   numCh     = juce::jmin (buffer.getNumChannels(), 2);
    const float sr        = (float)currentSampleRate;
    const int   mode      = (int)apvts.getRawParameterValue ("mode")->load();
    const float mixFrac   = apvts.getRawParameterValue ("mix")->load() * 0.01f;
    const float outGainLin= std::pow (10.f, apvts.getRawParameterValue ("outputGain")->load() / 20.f);
    const bool  agOn      = apvts.getRawParameterValue ("autoGain")->load() > 0.5f;

    // Read per-LFO params once per block
    float speed[kNumLFOs], depth[kNumLFOs], phaseOffset[kNumLFOs];
    int   shape[kNumLFOs];
    for (int i = 0; i < kNumLFOs; ++i)
    {
        speed[i]       = apvts.getRawParameterValue ("speed" + juce::String (i))->load();
        depth[i]       = apvts.getRawParameterValue ("depth" + juce::String (i))->load() * 0.01f;
        phaseOffset[i] = apvts.getRawParameterValue ("phase" + juce::String (i))->load() / 360.f;
        shape[i]       = (int)apvts.getRawParameterValue ("shape" + juce::String (i))->load();
        lfoPhaseInc[i] = speed[i] / sr;
    }

    float inSumSq = 0.f, outSumSq = 0.f, gainSum = 0.f;

    // Per-mode extra params
    const float panNorm   = apvts.getRawParameterValue ("pan")->load() / 100.f;  // −1..+1
    const float pingWidth = apvts.getRawParameterValue ("pingWidth")->load() * 0.01f;

    // Per-column gain and mix (0-100% → 0-1 linear)
    const float gainCol[3] = {
        apvts.getRawParameterValue ("gainL")->load() * 0.01f,
        apvts.getRawParameterValue ("gainC")->load() * 0.01f,
        apvts.getRawParameterValue ("gainR")->load() * 0.01f
    };
    const float mixCol[3] = {
        apvts.getRawParameterValue ("mixL")->load() * 0.01f,
        apvts.getRawParameterValue ("mixC")->load() * 0.01f,
        apvts.getRawParameterValue ("mixR")->load() * 0.01f
    };

    float columnGainDrive = 1.f;
    if (mode == (int)Mono || mode == (int)PingPong)
        columnGainDrive = gainCol[1];
    else if (mode == (int)DualTremolo)
        columnGainDrive = 0.5f * (gainCol[0] + gainCol[2]);
    else
        columnGainDrive = (gainCol[0] + gainCol[1] + gainCol[2]) / 3.f;

    for (int n = 0; n < N; ++n)
    {
        // Read input
        float inL = buffer.getReadPointer (0)[n];
        float inR = (numCh > 1) ? buffer.getReadPointer (1)[n] : inL;
        inSumSq += 0.5f * (inL * inL + inR * inR);

        // Advance LFO phases
        float lfoVal[kNumLFOs];
        for (int i = 0; i < kNumLFOs; ++i)
        {
            lfoPhase[i] += lfoPhaseInc[i];
            if (lfoPhase[i] >= 1.f) lfoPhase[i] -= 1.f;
            float ph = lfoPhase[i] + phaseOffset[i];
            if (ph >= 1.f) ph -= 1.f;
            lfoVal[i] = evalLFO (i, ph, shape[i]);
        }

        // ── Per-sample gain envelope for each LFO ──────────────────────────────
        // gain = 1 - depth * (1 - lfoVal)   (lfoVal=1 => no cut, lfoVal=0 => full cut)
        float gainLFO[kNumLFOs];
        for (int i = 0; i < kNumLFOs; ++i)
            gainLFO[i] = 1.f - depth[i] * (1.f - lfoVal[i]);

        float outL = inL, outR = inR;

        // ── Mode processing ──────────────────────────────────────────────────────
        if (mode == (int)Mono)
        {
            // LFO1 (CENTER) only. Pan knob weights how much each channel is modulated.
            float depthL = depth[1] * (1.f - juce::jmax (0.f, panNorm));
            float depthR = depth[1] * (1.f - juce::jmax (0.f, -panNorm));
            float modAmt = 1.f - lfoVal[1];
            float gL = 1.f - depthL * modAmt;
            float gR = 1.f - depthR * modAmt;
            outL = inL * gL * gainCol[1];  // Apply col0 gain
            outR = inR * gR * gainCol[1];
            gainSum += (gL + gR) * 0.5f;
            // Apply col0 mix
            outL = inL + (outL - inL) * mixCol[1];
            outR = inR + (outR - inR) * mixCol[1];
        }
        else if (mode == (int)PingPong)
        {
            // LFO1 (CENTER) sweeps level between L and R.
            float depthScaled = depth[1] * pingWidth;
            float gL = 1.f - depthScaled * lfoVal[1];
            float gR = 1.f - depthScaled * (1.f - lfoVal[1]);
            outL = inL * gL * gainCol[1];  // Apply col1 gain
            outR = inR * gR * gainCol[1];
            gainSum += (gL + gR) * 0.5f;
            // Apply col1 mix
            outL = inL + (outL - inL) * mixCol[1];
            outR = inR + (outR - inR) * mixCol[1];
        }
        else if (mode == (int)DualTremolo)
        {
            // LFO0 → Left only, LFO2 → Right only, LFO1 (CENTER) inactive.
            float gL = gainLFO[0];
            float gR = gainLFO[2];
            float outL_wet = inL * gL * gainCol[0];  // Apply col0 gain
            float outR_wet = inR * gR * gainCol[2];  // Apply col2 gain
            gainSum += (gL + gR) * 0.5f;
            // Apply per-column mix
            outL = inL + (outL_wet - inL) * mixCol[0];
            outR = inR + (outR_wet - inR) * mixCol[2];
        }
        else  // TriTremolo
        {
            // LFO0 → Left, LFO1 → both (centre layer), LFO2 → Right
            float gL = gainLFO[0] * gainLFO[1];
            float gR = gainLFO[2] * gainLFO[1];
            // Apply per-column gains
            float outL_wet = inL * gL * gainCol[0] * gainCol[1];  // Left: col0 × col1
            float outR_wet = inR * gR * gainCol[2] * gainCol[1];  // Right: col2 × col1
            gainSum += (gL + gR) * 0.5f;
            // Apply per-column mixes (blend each column's contribution)
            outL = inL + (outL_wet - inL) * mixCol[0] * mixCol[1];
            outR = inR + (outR_wet - inR) * mixCol[2] * mixCol[1];
        }

        // ── Global mix + output gain ──────────────────────────────────────────────────
        outL = (inL + (outL - inL) * mixFrac) * outGainLin;
        outR = (inR + (outR - inR) * mixFrac) * outGainLin;

        // ── Auto-gain ──────────────────────────────────────────────────────────
        if (agOn)
        {
            rmsPowerIn  = rmsCoeff * rmsPowerIn  + (1.f - rmsCoeff) * 0.5f * (inL * inL + inR * inR);
            rmsPowerOut = rmsCoeff * rmsPowerOut + (1.f - rmsCoeff) * 0.5f * (outL * outL + outR * outR);
            float rmsIn  = std::sqrt (juce::jmax (rmsPowerIn,  1.0e-10f));
            float rmsOut = std::sqrt (juce::jmax (rmsPowerOut, 1.0e-10f));
            float desired = juce::jlimit (1.f / 100.f, 100.f, rmsIn / rmsOut);
            desired /= juce::jmax (columnGainDrive, 0.01f);
            autoGainSmooth = agCoeff * autoGainSmooth + (1.f - agCoeff) * desired;

            if (mode == (int)DualTremolo)
            {
                if (mixCol[0] > 0.f)
                    outL *= autoGainSmooth;
                if (mixCol[2] > 0.f)
                    outR *= autoGainSmooth;
            }
            else
            {
                outL *= autoGainSmooth;
                outR *= autoGainSmooth;
            }
        }
        else { autoGainSmooth = 1.f; }

        outSumSq += 0.5f * (outL * outL + outR * outR);

        buffer.getWritePointer (0)[n] = outL;
        if (numCh > 1) buffer.getWritePointer (1)[n] = outR;
        else if (buffer.getNumChannels() >= 2)
            buffer.getWritePointer (1)[n] = outR;
    }

    // ── Export LFO phases and mod values for UI ───────────────────────────────
    for (int i = 0; i < kNumLFOs; ++i)
    {
        lfoPhaseOut[i].store (lfoPhase[i]);
        float ph = lfoPhase[i] + phaseOffset[i]; if (ph >= 1.f) ph -= 1.f;
        float val = evalLFO (i, ph, shape[i]);
        modValue[i].store (val);
    }

    // ── Meters ───────────────────────────────────────────────────────────────
    const float rmsIn  = std::sqrt (inSumSq  / (float)N);
    const float rmsOut = std::sqrt (outSumSq / (float)N);
    const float mc = std::exp (-1.f / (0.05f * sr / juce::jmax (N, 1)));
    inRmsSmooth  = mc * inRmsSmooth  + (1.f - mc) * rmsIn;
    outRmsSmooth = mc * outRmsSmooth + (1.f - mc) * rmsOut;
    auto toDb = [](float x) { return x > 1e-6f ? 20.f * std::log10 (x) : -120.f; };
    inputLevelDb .store (juce::jlimit (-120.f, 6.f, toDb (inRmsSmooth)));
    outputLevelDb.store (juce::jlimit (-120.f, 6.f, toDb (outRmsSmooth)));

    float avgGain = gainSum / (float)N;
    float agDb    = agOn ? (autoGainSmooth > 1e-6f ? 20.f * std::log10 (autoGainSmooth) : 0.f) : 0.f;
    float modDb   = avgGain > 1e-6f ? 20.f * std::log10 (avgGain) : -60.f;
    gainAppliedDb.store (juce::jlimit (-40.f, 6.f, modDb + agDb));

    int wp = historyWritePos.load();
    gainHistory[wp] = juce::jlimit (-40.f, 6.f, modDb + agDb);
    historyWritePos.store ((wp + 1) % kHistorySize);
}

//==============================================================================
void TremoloAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("editorZoom", editorZoomIndex);
    copyXmlToBinary (*xml, destData);
}

void TremoloAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
    {
        editorZoomIndex = juce::jlimit (0, 2, xml->getIntAttribute ("editorZoom", 0));
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

juce::AudioProcessorEditor* TremoloAudioProcessor::createEditor()
{
    return new TremoloAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TremoloAudioProcessor();
}
