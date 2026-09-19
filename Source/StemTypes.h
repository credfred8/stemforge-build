#pragma once

#include <JuceHeader.h>
#include <array>

namespace stemforge
{
    enum class Stem : int
    {
        drums = 0,
        bass,
        other,
        vocals,
        guitar,
        piano,
        count
    };

    constexpr int stemCount = static_cast<int>(Stem::count);

    inline const std::array<juce::String, stemCount> stemNames {
        "drums", "bass", "other", "vocals", "guitar", "piano"
    };

    struct SeparationOptions
    {
        std::array<bool, stemCount> exportStem { false, false, false, false, false, false };
        bool exportCleanSample = true;
        juce::File outputDirectory;
    };
}
