#pragma once

#include <JuceHeader.h>
#include <onnxruntime_cxx_api.h>
#include <array>
#include <atomic>
#include <mutex>
#include <vector>
#include <thread>
#include "StemTypes.h"

namespace stemforge
{
    class StemEngine final : private juce::Thread
    {
    public:
        StemEngine();
        ~StemEngine() override;

        bool start(const juce::File& audioFile, const SeparationOptions& options);
        void cancel();

        bool isBusy() const noexcept { return busy.load(); }
        float getProgress() const noexcept { return progress.load(); }
        juce::String getStatus() const;
        juce::String getLastError() const;
        std::vector<juce::File> getLastOutputs() const;
        juce::File findModelFile() const;
        juce::File findRuntimeFile() const;

        static constexpr int modelSampleRate = 44100;
        static constexpr int segmentSamples = 343980;
        static constexpr int overlapSamples = segmentSamples / 4;
        static constexpr int strideSamples = segmentSamples - overlapSamples;

    private:
        void run() override;
        void setStatus(const juce::String& text);
        void fail(const juce::String& text);

        bool ensureRuntimeLoaded();
        bool loadAndResample(const juce::File& file, juce::AudioBuffer<float>& destination);
        bool separate(const juce::AudioBuffer<float>& mix,
                      std::vector<juce::AudioBuffer<float>>& stems);
        bool writeOutputs(const juce::AudioBuffer<float>& mix,
                          const std::vector<juce::AudioBuffer<float>>& stems,
                          const juce::File& inputFile,
                          const SeparationOptions& options);
        bool writeFloatWav(const juce::File& file, const juce::AudioBuffer<float>& audio) const;
        std::vector<float> makeWindow() const;
        std::unique_ptr<Ort::Session> createSession(const juce::File& modelFile);

        mutable std::mutex stateMutex;
        juce::String status { "Готов. Перетащи WAV/MP3/FLAC/AIFF/OGG в окно." };
        juce::String lastError;
        std::vector<juce::File> lastOutputs;
        juce::File pendingInput;
        SeparationOptions pendingOptions;

        std::atomic<bool> busy { false };
        std::atomic<float> progress { 0.0f };

        std::unique_ptr<juce::DynamicLibrary> ortLibrary;
        std::unique_ptr<Ort::Env> env;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemEngine)
    };
}
