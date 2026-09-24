#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../Source/OneKnobEngine.h"
#include <iostream>
#include <cmath>

int main()
{
    OneKnobEngine engine;
    constexpr double sr = 48000.0;
    constexpr int n = 4096;
    engine.prepare (sr, n, 2);
    engine.setAmount (0.75f);

    juce::AudioBuffer<float> b (2, n);
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) sr;
        const float s = 0.22f * std::sin (2.0f * juce::MathConstants<float>::pi * 55.0f * t)
                      + 0.12f * std::sin (2.0f * juce::MathConstants<float>::pi * 850.0f * t)
                      + 0.06f * std::sin (2.0f * juce::MathConstants<float>::pi * 9000.0f * t);
        b.setSample (0, i, s);
        b.setSample (1, i, s * 0.97f);
    }

    engine.process (b);

    float maxAbs = 0.0f;
    double sumSq = 0.0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < n; ++i)
        {
            const float x = b.getSample (ch, i);
            if (!std::isfinite (x))
            {
                std::cerr << "non-finite output" << std::endl;
                return 2;
            }
            maxAbs = juce::jmax (maxAbs, std::abs (x));
            sumSq += (double) x * x;
        }

    const float rms = (float) std::sqrt (sumSq / (2.0 * n));
    std::cout << "peak=" << maxAbs << " rms=" << rms << std::endl;

    if (maxAbs > 1.02f)
    {
        std::cerr << "peak safety failed" << std::endl;
        return 3;
    }

    if (rms <= 0.01f)
    {
        std::cerr << "unexpected silence" << std::endl;
        return 4;
    }

    return 0;
}
