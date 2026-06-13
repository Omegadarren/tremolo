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
        juce::StringArray { "Mono", "Tri-Mono", "Ping Pong" }, 0));

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

    // Crossover filter state
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

    // For Tri-Mono crossover: 1-pole LP coefficients
    auto lp1Coeff = [&](float fc) { return 1.f - std::exp (-2.f * juce::MathConstants<float>::pi * fc / sr); };
    const float lpCoeffLow  = lp1Coeff (kLowCross);
    const float lpCoeffHigh = lp1Coeff (kHighCross);

    float inSumSq = 0.f, outSumSq = 0.f, gainSum = 0.f;

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
            // All three LFOs multiply the same stereo signal in series
            float g = gainLFO[0] * gainLFO[1] * gainLFO[2];
            outL = inL * g;
            outR = inR * g;
            gainSum += g;
        }
        else if (mode == (int)TriMono)
        {
            // Split into 3 bands using 1-pole crossovers, modulate each band
            // Low band: LP at kLowCross
            // Mid band: HP at kLowCross, LP at kHighCross
            // High band: HP at kHighCross

            // Process left channel
            auto processBands = [&](float x, float& lpL, float& lpH) -> float {
                float low  = lpL + lpCoeffLow  * (x   - lpL); lpL = low;
                float mid1 = lpH + lpCoeffHigh * (x   - lpH); lpH = mid1;
                float high = x - mid1;
                float mid  = mid1 - low;
                return low * gainLFO[0] + mid * gainLFO[1] + high * gainLFO[2];
            };
            outL = processBands (inL, xLP1[0], xLP2[0]);
            outR = processBands (inR, xLP1[1], xLP2[1]);
            gainSum += (gainLFO[0] + gainLFO[1] + gainLFO[2]) / 3.f;
        }
        else // PingPong
        {
            // LFO-1 -> Left, LFO-2 -> Right, LFO-3 -> Center (both)
            outL = inL * gainLFO[0] * gainLFO[2];
            outR = inR * gainLFO[1] * gainLFO[2];
            gainSum += (gainLFO[0] + gainLFO[1]) * 0.5f * gainLFO[2];
        }

        // ── Mix + output gain ──────────────────────────────────────────────────
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
            autoGainSmooth = agCoeff * autoGainSmooth + (1.f - agCoeff) * desired;
            outL *= autoGainSmooth;
            outR *= autoGainSmooth;
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
