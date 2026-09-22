#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>

struct MasterSettings
{
    bool masterBypass = false;
    bool smartGain = true;
    bool cleanEqOn = true;
    bool dynamicEqOn = true;
    bool resonanceOn = true;
    bool glueOn = true;
    bool multibandOn = true;
    bool impactOn = true;
    bool analogOn = true;
    bool exciterOn = true;
    bool bassMonoOn = true;
    bool imagerOn = true;
    bool clipperOn = true;
    bool limiterOn = true;
    bool ditherOn = true;

    float inputTrimDb = 0.0f;
    float targetInputRmsDb = -18.0f;
    float smartSpeed = 0.35f;
    float smartMaxGainDb = 9.0f;

    float lowShelfDb = 0.0f;
    float lowShelfHz = 105.0f;
    float lowMidDb = 0.0f;
    float lowMidHz = 320.0f;
    float lowMidQ = 0.85f;
    float presenceDb = 0.0f;
    float presenceHz = 3200.0f;
    float presenceQ = 0.90f;
    float airDb = 0.0f;
    float airHz = 10500.0f;

    float dynamicEq = 0.30f;
    float dynThresholdDb = -20.0f;
    float dynAttackMs = 18.0f;
    float dynReleaseMs = 160.0f;
    float dynLowHz = 260.0f;
    float dynHighHz = 5200.0f;

    float resonance = 0.22f;
    float resonanceHz = 2850.0f;
    float resonanceQ = 2.6f;

    float glue = 0.35f;
    float glueThresholdDb = -16.0f;
    float glueRatio = 2.0f;
    float glueAttackMs = 24.0f;
    float glueReleaseMs = 180.0f;
    float glueMakeupDb = 0.0f;
    float glueMix = 0.72f;

    float multiband = 0.30f;
    float mbLowHz = 150.0f;
    float mbHighHz = 4500.0f;
    float mbLowAmount = 0.40f;
    float mbMidAmount = 0.30f;
    float mbHighAmount = 0.24f;

    float impact = 0.30f;
    float impactSpeed = 0.45f;
    float impactMix = 0.70f;

    float analog = 0.16f;
    float analogTone = 0.58f;
    float analogMix = 0.55f;

    float exciter = 0.12f;
    float exciterHz = 6500.0f;
    float exciterMix = 0.45f;

    float bassMonoHz = 115.0f;
    float bassMonoAmount = 1.0f;

    float widthLow = 0.92f;
    float widthMid = 1.02f;
    float widthHigh = 1.08f;
    float imagerLowHz = 180.0f;
    float imagerHighHz = 5000.0f;
    float imagerSafety = 0.80f;

    float dryWet = 1.0f;

    float clipDriveDb = 1.5f;
    float clipMix = 1.0f;
    float clipCeilingDb = -0.35f;
    float clipShape = 0.55f;

    float limiterDriveDb = 3.0f;
    float limiterCeilingDb = -0.9f;
    float limiterReleaseMs = 120.0f;

    float outputTrimDb = 0.0f;
};

class MasterEngine
{
public:
    static constexpr int spectrumBins = 64;

    void prepare (double newSampleRate, int maximumBlockSize, int channels)
    {
        sampleRate = juce::jmax (8000.0, newSampleRate);
        numChannels = juce::jlimit (1, 2, channels);
        maxBlock = juce::jmax (16, maximumBlockSize);

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, (juce::uint32) numChannels };
        lowCut.prepare (spec);
        lowShelf.prepare (spec);
        lowMid.prepare (spec);
        presence.prepare (spec);
        air.prepare (spec);
        resonanceFilter.prepare (spec);
        glueComp.prepare (spec);

        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) numChannels, 3,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true, true);
        oversampling->initProcessing ((size_t) maxBlock);

        dry.setSize (numChannels, maxBlock);
        glueDry.setSize (numChannels, maxBlock);
        reset();
        updateFilters();
        updateCompressor();
    }

    void reset()
    {
        lowCut.reset(); lowShelf.reset(); lowMid.reset(); presence.reset(); air.reset();
        resonanceFilter.reset(); glueComp.reset();
        if (oversampling) oversampling->reset();

        lowDynState.fill (0.0f);
        highDynState.fill (0.0f);
        lowDynEnv.fill (0.0f);
        highDynEnv.fill (0.0f);
        multibandLow.fill (0.0f);
        multibandHigh.fill (0.0f);
        bassLow.fill (0.0f);
        impactFast.fill (0.0f);
        impactSlow.fill (0.0f);
        analogLp.fill (0.0f);
        exciterLp.fill (0.0f);
        imagerLow.fill (0.0f);
        imagerHighLp.fill (0.0f);

        smartGainDb = 0.0f;
        lastInputGain = 1.0f;
        lastOutputGain = 1.0f;
        limiterGain = 1.0f;
        lastClipDriveGain = juce::Decibels::decibelsToGain (settings.clipDriveDb);
        lastClipCeilingGain = juce::Decibels::decibelsToGain (settings.clipCeilingDb);
        lastLimiterDriveGain = juce::Decibels::decibelsToGain (settings.limiterDriveDb);
        lastLimiterCeilingGain = juce::Decibels::decibelsToGain (settings.limiterCeilingDb - 0.25f);
        meterCounter = 0;
        limiterReductionDb.store (0.0f);
    }

    void setSettings (const MasterSettings& s)
    {
        settings = s;
        updateFilters();
        updateCompressor();
    }

    int getLatencySamples() const
    {
        return oversampling ? (int) std::ceil (oversampling->getLatencyInSamples()) : 0;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumSamples() <= 0)
            return;

        measureInput (buffer);
        analyzeSpectrum (buffer, preSpectrum);

        if (settings.masterBypass)
        {
            limiterReductionDb.store (0.0f);
            measureOutput (buffer);
            analyzeSpectrum (buffer, postSpectrum);
            return;
        }

        dry.setSize (juce::jmin (2, buffer.getNumChannels()), buffer.getNumSamples(), false, false, true);
        dry.makeCopyOf (buffer, true);

        applyInputCoach (buffer);

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);

        if (settings.cleanEqOn)
        {
            lowCut.process (ctx);
            lowShelf.process (ctx);
            lowMid.process (ctx);
            presence.process (ctx);
            air.process (ctx);
        }

        if (settings.dynamicEqOn)
            processDynamicEq (buffer);

        if (settings.resonanceOn)
            resonanceFilter.process (ctx);

        if (settings.glueOn)
            processGlue (buffer);

        if (settings.multibandOn)
            processMultiband (buffer);

        if (settings.impactOn)
            processImpact (buffer);

        if (settings.analogOn)
            processAnalog (buffer);

        if (settings.exciterOn)
            processExciter (buffer);

        if (settings.bassMonoOn)
            processBassMono (buffer);

        if (settings.imagerOn)
            processImager (buffer);

        if (settings.dryWet < 0.999f)
        {
            const float wet = juce::jlimit (0.0f, 1.0f, settings.dryWet);
            const float dryMix = 1.0f - wet;
            for (int ch = 0; ch < juce::jmin (buffer.getNumChannels(), dry.getNumChannels()); ++ch)
            {
                buffer.applyGain (ch, 0, buffer.getNumSamples(), wet);
                buffer.addFrom (ch, 0, dry, ch, 0, buffer.getNumSamples(), dryMix);
            }
        }

        if ((settings.clipperOn || settings.limiterOn) && oversampling)
            processOversampledFinal (buffer);
        else
            limiterReductionDb.store (0.0f);

        applyOutputTrim (buffer);

        if (settings.ditherOn)
            applyDither (buffer);

        sanitize (buffer);
        measureOutput (buffer);
        analyzeSpectrum (buffer, postSpectrum);
    }

    std::atomic<float> inputRmsDb { -100.0f };
    std::atomic<float> inputPeakDb { -100.0f };
    std::atomic<float> outputRmsDb { -100.0f };
    std::atomic<float> outputPeakDb { -100.0f };
    std::atomic<float> loudnessEstimate { -100.0f };
    std::atomic<float> crestDb { 0.0f };
    std::atomic<float> correlation { 1.0f };
    std::atomic<float> smartGainAppliedDb { 0.0f };
    std::atomic<float> limiterReductionDb { 0.0f };
    std::array<std::atomic<float>, spectrumBins> preSpectrum {};
    std::array<std::atomic<float>, spectrumBins> postSpectrum {};

private:
    using Filter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    static float coeffForHz (float hz, double sr)
    {
        hz = juce::jlimit (2.0f, (float) (sr * 0.45), hz);
        return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sr);
    }

    static float timeCoeff (float ms, double sr)
    {
        ms = juce::jmax (0.05f, ms);
        return std::exp (-1.0f / (0.001f * ms * (float) sr));
    }

    void updateFilters()
    {
        if (sampleRate <= 0.0)
            return;

        const float lowHz = juce::jlimit (35.0f, 350.0f, settings.lowShelfHz);
        const float lowMidHz = juce::jlimit (90.0f, 1200.0f, settings.lowMidHz);
        const float presenceHz = juce::jlimit (900.0f, 8000.0f, settings.presenceHz);
        const float airHz = juce::jlimit (5000.0f, 18000.0f, settings.airHz);

        *lowCut.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 20.0, 0.707);
        *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, lowHz, 0.72, juce::Decibels::decibelsToGain (settings.lowShelfDb));
        *lowMid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, lowMidHz, juce::jlimit (0.25f, 4.0f, settings.lowMidQ),
            juce::Decibels::decibelsToGain (settings.lowMidDb));
        *presence.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, presenceHz, juce::jlimit (0.25f, 4.0f, settings.presenceQ),
            juce::Decibels::decibelsToGain (settings.presenceDb));
        *air.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, airHz, 0.70, juce::Decibels::decibelsToGain (settings.airDb));

        const float notchDb = -juce::jlimit (0.0f, 8.0f, settings.resonance * 8.0f);
        *resonanceFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate,
            juce::jlimit (500.0f, 12000.0f, settings.resonanceHz),
            juce::jlimit (0.4f, 10.0f, settings.resonanceQ),
            juce::Decibels::decibelsToGain (notchDb));
    }

    void updateCompressor()
    {
        glueComp.setThreshold (juce::jlimit (-36.0f, -2.0f, settings.glueThresholdDb));
        glueComp.setRatio (juce::jlimit (1.1f, 10.0f, settings.glueRatio));
        glueComp.setAttack (juce::jlimit (0.5f, 100.0f, settings.glueAttackMs));
        glueComp.setRelease (juce::jlimit (30.0f, 600.0f, settings.glueReleaseMs));
    }

    void applyInputCoach (juce::AudioBuffer<float>& buffer)
    {
        float appliedDb = settings.inputTrimDb;

        if (settings.smartGain)
        {
            const float current = inputRmsDb.load();
            const float maxGain = juce::jlimit (1.0f, 18.0f, settings.smartMaxGainDb);
            const float desired = current < -65.0f
                                ? 0.0f
                                : juce::jlimit (-maxGain, maxGain, settings.targetInputRmsDb - current);
            const float speed = juce::jlimit (0.0f, 1.0f, settings.smartSpeed);
            const float tauSeconds = juce::jmap (speed, 2.5f, 0.18f);
            const float alpha = 1.0f - std::exp (-(float) buffer.getNumSamples() / ((float) sampleRate * tauSeconds));
            smartGainDb += alpha * (desired - smartGainDb);
            appliedDb += smartGainDb;
        }
        else
        {
            smartGainDb *= 0.995f;
        }

        smartGainAppliedDb.store (appliedDb);
        const float targetGain = juce::Decibels::decibelsToGain (appliedDb);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.applyGainRamp (ch, 0, buffer.getNumSamples(), lastInputGain, targetGain);

        lastInputGain = targetGain;
    }

    void applyOutputTrim (juce::AudioBuffer<float>& buffer)
    {
        const float targetGain = juce::Decibels::decibelsToGain (settings.outputTrimDb);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.applyGainRamp (ch, 0, buffer.getNumSamples(), lastOutputGain, targetGain);
        lastOutputGain = targetGain;
    }

    void processDynamicEq (juce::AudioBuffer<float>& buffer)
    {
        const float amount = juce::jlimit (0.0f, 1.0f, settings.dynamicEq);
        if (amount <= 0.0001f)
            return;

        const float lowA = coeffForHz (settings.dynLowHz, sampleRate);
        const float highA = coeffForHz (juce::jmax (settings.dynLowHz + 300.0f, settings.dynHighHz), sampleRate);
        const float threshold = juce::Decibels::decibelsToGain (settings.dynThresholdDb);
        const float attack = timeCoeff (settings.dynAttackMs, sampleRate);
        const float release = timeCoeff (settings.dynReleaseMs, sampleRate);
        const float minGain = juce::Decibels::decibelsToGain (-12.0f * amount);

        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            float lowState = lowDynState[(size_t) ch];
            float highState = highDynState[(size_t) ch];
            float lowEnv = lowDynEnv[(size_t) ch];
            float highEnv = highDynEnv[(size_t) ch];

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float x = d[i];
                lowState += lowA * (x - lowState);
                highState += highA * (x - highState);

                const float low = lowState;
                const float high = x - highState;
                const float mid = x - low - high;

                const float lowAbs = std::abs (low);
                const float highAbs = std::abs (high);
                lowEnv = (lowAbs > lowEnv ? attack : release) * lowEnv
                       + (1.0f - (lowAbs > lowEnv ? attack : release)) * lowAbs;
                highEnv = (highAbs > highEnv ? attack : release) * highEnv
                        + (1.0f - (highAbs > highEnv ? attack : release)) * highAbs;

                auto gainFor = [threshold, minGain, amount] (float env, float tilt)
                {
                    if (env <= threshold || threshold <= 0.0f)
                        return 1.0f;
                    const float ratioPower = 0.22f + amount * tilt;
                    return juce::jmax (minGain, std::pow (threshold / juce::jmax (env, 1.0e-7f), ratioPower));
                };

                const float gl = gainFor (lowEnv, 0.95f);
                const float gh = gainFor (highEnv, 0.78f);
                d[i] = low * gl + mid + high * gh;
            }

            lowDynState[(size_t) ch] = lowState;
            highDynState[(size_t) ch] = highState;
            lowDynEnv[(size_t) ch] = lowEnv;
            highDynEnv[(size_t) ch] = highEnv;
        }
    }

    void processGlue (juce::AudioBuffer<float>& buffer)
    {
        glueDry.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
        glueDry.makeCopyOf (buffer, true);

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        glueComp.process (ctx);

        const float makeup = juce::Decibels::decibelsToGain (settings.glueMakeupDb);
        buffer.applyGain (makeup);

        const float amount = juce::jlimit (0.0f, 1.0f, settings.glue);
        const float wet = juce::jlimit (0.0f, 1.0f, settings.glueMix * (0.45f + 0.55f * amount));
        const float dryMix = 1.0f - wet;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.applyGain (ch, 0, buffer.getNumSamples(), wet);
            buffer.addFrom (ch, 0, glueDry, ch, 0, buffer.getNumSamples(), dryMix);
        }
    }

    void processMultiband (juce::AudioBuffer<float>& buffer)
    {
        const float global = juce::jlimit (0.0f, 1.0f, settings.multiband);
        const float lowA = coeffForHz (settings.mbLowHz, sampleRate);
        const float highA = coeffForHz (juce::jmax (settings.mbLowHz + 400.0f, settings.mbHighHz), sampleRate);

        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            float lowState = multibandLow[(size_t) ch];
            float highState = multibandHigh[(size_t) ch];

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float x = d[i];
                lowState += lowA * (x - lowState);
                highState += highA * (x - highState);

                const float low = lowState;
                const float high = x - highState;
                const float mid = x - low - high;

                auto tame = [global] (float v, float bandAmount, float threshold, float strength)
                {
                    const float amount = juce::jlimit (0.0f, 1.0f, bandAmount) * global;
                    const float av = std::abs (v);
                    const float excess = juce::jmax (0.0f, av - threshold);
                    const float g = 1.0f / (1.0f + amount * strength * excess);
                    return v * g;
                };

                d[i] = tame (low, settings.mbLowAmount, 0.19f, 3.2f)
                     + tame (mid, settings.mbMidAmount, 0.15f, 2.4f)
                     + tame (high, settings.mbHighAmount, 0.10f, 2.0f);
            }

            multibandLow[(size_t) ch] = lowState;
            multibandHigh[(size_t) ch] = highState;
        }
    }

    void processImpact (juce::AudioBuffer<float>& buffer)
    {
        const float amount = juce::jlimit (0.0f, 1.0f, settings.impact);
        const float mix = juce::jlimit (0.0f, 1.0f, settings.impactMix);
        const float speed = juce::jlimit (0.0f, 1.0f, settings.impactSpeed);
        const float fastReleaseMs = juce::jmap (speed, 28.0f, 5.0f);
        const float slowReleaseMs = juce::jmap (speed, 260.0f, 95.0f);
        const float fastRelease = timeCoeff (fastReleaseMs, sampleRate);
        const float slowRelease = timeCoeff (slowReleaseMs, sampleRate);

        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            float fast = impactFast[(size_t) ch];
            float slow = impactSlow[(size_t) ch];

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float x = d[i];
                const float a = std::abs (x);
                fast = juce::jmax (a, fast * fastRelease);
                slow = juce::jmax (a, slow * slowRelease);
                const float transient = juce::jmax (0.0f, fast - slow);
                const float boost = 1.0f + amount * juce::jlimit (0.0f, 0.35f, transient * 1.7f);
                const float shaped = x * boost;
                d[i] = x + (shaped - x) * mix;
            }

            impactFast[(size_t) ch] = fast;
            impactSlow[(size_t) ch] = slow;
        }
    }

    void processAnalog (juce::AudioBuffer<float>& buffer)
    {
        const float amount = juce::jlimit (0.0f, 1.0f, settings.analog);
        const float mix = juce::jlimit (0.0f, 1.0f, settings.analogMix);
        const float drive = 1.0f + amount * 4.0f;
        const float norm = juce::jmax (0.001f, std::tanh (drive));
        const float toneHz = juce::jmap (juce::jlimit (0.0f, 1.0f, settings.analogTone), 2600.0f, 18000.0f);
        const float a = coeffForHz (toneHz, sampleRate);

        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            float lp = analogLp[(size_t) ch];

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float x = d[i];
                const float sat = std::tanh (x * drive) / norm;
                lp += a * (sat - lp);
                const float coloured = lp + (sat - lp) * juce::jlimit (0.25f, 1.0f, settings.analogTone + 0.18f);
                d[i] = x + (coloured - x) * mix * (0.25f + 0.75f * amount);
            }

            analogLp[(size_t) ch] = lp;
        }
    }

    void processExciter (juce::AudioBuffer<float>& buffer)
    {
        const float amount = juce::jlimit (0.0f, 1.0f, settings.exciter);
        const float mix = juce::jlimit (0.0f, 1.0f, settings.exciterMix);
        const float a = coeffForHz (juce::jlimit (2500.0f, 14000.0f, settings.exciterHz), sampleRate);

        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            float lp = exciterLp[(size_t) ch];

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float x = d[i];
                lp += a * (x - lp);
                const float high = x - lp;
                const float harmonic = std::tanh (high * 6.5f) * 0.12f;
                d[i] = x + harmonic * amount * mix;
            }

            exciterLp[(size_t) ch] = lp;
        }
    }

    void processBassMono (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2)
            return;

        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        const float a = coeffForHz (juce::jlimit (45.0f, 250.0f, settings.bassMonoHz), sampleRate);
        const float amount = juce::jlimit (0.0f, 1.0f, settings.bassMonoAmount);
        float ll = bassLow[0];
        float rr = bassLow[1];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float inL = l[i];
            const float inR = r[i];
            ll += a * (inL - ll);
            rr += a * (inR - rr);
            const float mono = 0.5f * (ll + rr);
            const float procL = (inL - ll) + mono;
            const float procR = (inR - rr) + mono;
            l[i] = inL + (procL - inL) * amount;
            r[i] = inR + (procR - inR) * amount;
        }

        bassLow[0] = ll;
        bassLow[1] = rr;
    }

    void processImager (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2)
            return;

        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);

        const float lowA = coeffForHz (juce::jlimit (80.0f, 500.0f, settings.imagerLowHz), sampleRate);
        const float highA = coeffForHz (juce::jlimit (1800.0f, 12000.0f, settings.imagerHighHz), sampleRate);

        float lowL = imagerLow[0], lowR = imagerLow[1];
        float highLpL = imagerHighLp[0], highLpR = imagerHighLp[1];

        const float safety = juce::jlimit (0.0f, 1.0f, settings.imagerSafety);
        const float corr = juce::jlimit (-1.0f, 1.0f, correlation.load());
        const float corrFactor = juce::jmap (corr, -1.0f, 1.0f, 0.45f, 1.0f);
        const float safeFactor = juce::jmap (safety, 1.0f, corrFactor);

        const float wLow = 1.0f + (settings.widthLow - 1.0f) * safeFactor;
        const float wMid = 1.0f + (settings.widthMid - 1.0f) * safeFactor;
        const float wHigh = 1.0f + (settings.widthHigh - 1.0f) * safeFactor;

        auto widenPair = [] (float a, float b, float width, float& oa, float& ob)
        {
            const float m = 0.5f * (a + b);
            const float s = 0.5f * (a - b) * width;
            oa = m + s;
            ob = m - s;
        };

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float inL = l[i], inR = r[i];

            lowL += lowA * (inL - lowL);
            lowR += lowA * (inR - lowR);
            highLpL += highA * (inL - highLpL);
            highLpR += highA * (inR - highLpR);

            const float highL = inL - highLpL;
            const float highR = inR - highLpR;
            const float midL = inL - lowL - highL;
            const float midR = inR - lowR - highR;

            float loL, loR, miL, miR, hiL, hiR;
            widenPair (lowL, lowR, wLow, loL, loR);
            widenPair (midL, midR, wMid, miL, miR);
            widenPair (highL, highR, wHigh, hiL, hiR);

            l[i] = loL + miL + hiL;
            r[i] = loR + miR + hiR;
        }

        imagerLow[0] = lowL; imagerLow[1] = lowR;
        imagerHighLp[0] = highLpL; imagerHighLp[1] = highLpR;
    }

    void processOversampledFinal (juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto up = oversampling->processSamplesUp (block);

        const float clipDriveTarget = juce::Decibels::decibelsToGain (settings.clipDriveDb);
        const float clipCeilingTarget = juce::Decibels::decibelsToGain (settings.clipCeilingDb);
        const float clipMix = juce::jlimit (0.0f, 1.0f, settings.clipMix);
        const float shape = juce::jlimit (0.0f, 1.0f, settings.clipShape);
        const float k = juce::jmap (shape, 0.70f, 2.80f);
        const float tanhNorm = juce::jmax (0.001f, std::tanh (k));

        const float limDriveTarget = juce::Decibels::decibelsToGain (settings.limiterDriveDb);
        const float requestedCeiling = juce::Decibels::decibelsToGain (settings.limiterCeilingDb);
        const float ceilingTarget = requestedCeiling * juce::Decibels::decibelsToGain (-0.25f);
        const double osRate = sampleRate * 8.0;
        const float release = timeCoeff (settings.limiterReleaseMs, osRate);

        float maxReduction = 0.0f;
        const float denom = (float) juce::jmax ((size_t) 1, up.getNumSamples() - 1);

        for (size_t i = 0; i < up.getNumSamples(); ++i)
        {
            const float t = (float) i / denom;
            const float clipDrive = juce::jmap (t, lastClipDriveGain, clipDriveTarget);
            const float clipCeiling = juce::jmap (t, lastClipCeilingGain, clipCeilingTarget);
            const float limDrive = juce::jmap (t, lastLimiterDriveGain, limDriveTarget);
            const float ceiling = juce::jmap (t, lastLimiterCeilingGain, ceilingTarget);
            float peak = 0.0f;

            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                auto* d = up.getChannelPointer (ch);
                float x = d[i];

                if (settings.clipperOn)
                {
                    const float normalized = x * clipDrive / juce::jmax (0.05f, clipCeiling);
                    const float soft = std::tanh (normalized * k) / tanhNorm * clipCeiling;
                    x = x + (soft - x) * clipMix;
                }

                if (settings.limiterOn)
                    x *= limDrive;

                if (! std::isfinite (x))
                    x = 0.0f;

                d[i] = x;
                peak = juce::jmax (peak, std::abs (x));
            }

            if (settings.limiterOn)
            {
                const float target = peak > ceiling ? ceiling / juce::jmax (peak, 1.0e-9f) : 1.0f;
                if (target < limiterGain)
                    limiterGain = target;
                else
                    limiterGain = target + release * (limiterGain - target);

                const float reductionDb = -juce::Decibels::gainToDecibels (juce::jmax (limiterGain, 1.0e-6f));
                maxReduction = juce::jmax (maxReduction, reductionDb);

                for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
                {
                    auto* d = up.getChannelPointer (ch);
                    d[i] = juce::jlimit (-ceiling, ceiling, d[i] * limiterGain);
                }
            }
        }

        lastClipDriveGain = clipDriveTarget;
        lastClipCeilingGain = clipCeilingTarget;
        lastLimiterDriveGain = limDriveTarget;
        lastLimiterCeilingGain = ceilingTarget;

        limiterReductionDb.store (maxReduction);
        oversampling->processSamplesDown (block);

        // Reconstruction after oversampling can create tiny inter-sample overs.
        // Keep an inaudible final safety guard at the user-selected ceiling.
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                d[i] = juce::jlimit (-requestedCeiling, requestedCeiling, d[i]);
        }
    }

    void applyDither (juce::AudioBuffer<float>& buffer)
    {
        const float lsb = 1.0f / 8388608.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                d[i] += (random.nextFloat() - random.nextFloat()) * lsb;
        }
    }

    static void sanitize (juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite (d[i]))
                    d[i] = 0.0f;
        }
    }

    void measureInput (const juce::AudioBuffer<float>& buffer)
    {
        float rms = 0.0f, peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            rms += buffer.getRMSLevel (ch, 0, buffer.getNumSamples());
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
        }
        rms /= (float) juce::jmax (1, buffer.getNumChannels());
        inputRmsDb.store (juce::Decibels::gainToDecibels (juce::jmax (rms, 1.0e-8f)));
        inputPeakDb.store (juce::Decibels::gainToDecibels (juce::jmax (peak, 1.0e-8f)));
    }

    void measureOutput (const juce::AudioBuffer<float>& buffer)
    {
        float rms = 0.0f, peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            rms += buffer.getRMSLevel (ch, 0, buffer.getNumSamples());
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
        }
        rms /= (float) juce::jmax (1, buffer.getNumChannels());

        const float rmsDb = juce::Decibels::gainToDecibels (juce::jmax (rms, 1.0e-8f));
        const float peakDb = juce::Decibels::gainToDecibels (juce::jmax (peak, 1.0e-8f));
        outputRmsDb.store (rmsDb);
        outputPeakDb.store (peakDb);
        loudnessEstimate.store (rmsDb - 0.7f);
        crestDb.store (peakDb - rmsDb);

        if (buffer.getNumChannels() >= 2)
        {
            const auto* l = buffer.getReadPointer (0);
            const auto* r = buffer.getReadPointer (1);
            double lr = 0.0, ll = 0.0, rr = 0.0;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                lr += (double) l[i] * (double) r[i];
                ll += (double) l[i] * (double) l[i];
                rr += (double) r[i] * (double) r[i];
            }
            correlation.store ((float) juce::jlimit (-1.0, 1.0, lr / std::sqrt (juce::jmax (1.0e-12, ll * rr))));
        }
        else
        {
            correlation.store (1.0f);
        }
    }

    void analyzeSpectrum (const juce::AudioBuffer<float>& buffer,
                          std::array<std::atomic<float>, spectrumBins>& target)
    {
        if ((++meterCounter % 3) != 0 || buffer.getNumSamples() < 16)
            return;

        const int n = buffer.getNumSamples();
        for (int b = 0; b < spectrumBins; ++b)
        {
            const float norm = (float) b / (float) (spectrumBins - 1);
            const float freq = 24.0f * std::pow (18000.0f / 24.0f, norm);
            const float w = juce::MathConstants<float>::twoPi * freq / (float) sampleRate;
            double re = 0.0, im = 0.0;

            for (int i = 0; i < n; i += 2)
            {
                float mono = 0.0f;
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    mono += buffer.getSample (ch, i);
                mono /= (float) juce::jmax (1, buffer.getNumChannels());

                const float phase = w * (float) i;
                re += mono * std::cos (phase);
                im -= mono * std::sin (phase);
            }

            const float mag = (float) std::sqrt (re * re + im * im) / (float) juce::jmax (1, n / 2);
            const float db = juce::Decibels::gainToDecibels (juce::jmax (mag, 1.0e-7f));
            target[(size_t) b].store (db);
        }
    }

    double sampleRate = 44100.0;
    int numChannels = 2;
    int maxBlock = 512;
    int meterCounter = 0;

    float smartGainDb = 0.0f;
    float lastInputGain = 1.0f;
    float lastOutputGain = 1.0f;
    float limiterGain = 1.0f;
    float lastClipDriveGain = 1.0f;
    float lastClipCeilingGain = 1.0f;
    float lastLimiterDriveGain = 1.0f;
    float lastLimiterCeilingGain = 1.0f;
    MasterSettings settings;

    Filter lowCut, lowShelf, lowMid, presence, air, resonanceFilter;
    juce::dsp::Compressor<float> glueComp;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::AudioBuffer<float> dry;
    juce::AudioBuffer<float> glueDry;
    juce::Random random;

    std::array<float, 2> lowDynState {};
    std::array<float, 2> highDynState {};
    std::array<float, 2> lowDynEnv {};
    std::array<float, 2> highDynEnv {};
    std::array<float, 2> multibandLow {};
    std::array<float, 2> multibandHigh {};
    std::array<float, 2> bassLow {};
    std::array<float, 2> impactFast {};
    std::array<float, 2> impactSlow {};
    std::array<float, 2> analogLp {};
    std::array<float, 2> exciterLp {};
    std::array<float, 2> imagerLow {};
    std::array<float, 2> imagerHighLp {};
};
