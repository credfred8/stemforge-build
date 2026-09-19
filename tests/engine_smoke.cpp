#include <JuceHeader.h>
#include "../Source/StemEngine.h"

#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <thread>

namespace
{
    bool writeTestWav(const juce::File& file)
    {
        constexpr int sampleRate = stemforge::StemEngine::modelSampleRate;
        constexpr int numSamples = sampleRate;

        juce::AudioBuffer<float> audio(2, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            const auto t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float left = 0.20f * std::sin(2.0f * juce::MathConstants<float>::pi * 220.0f * t)
                             + 0.10f * std::sin(2.0f * juce::MathConstants<float>::pi * 880.0f * t);
            const float right = 0.20f * std::sin(2.0f * juce::MathConstants<float>::pi * 330.0f * t)
                              + 0.08f * std::sin(2.0f * juce::MathConstants<float>::pi * 660.0f * t);
            audio.setSample(0, i, left);
            audio.setSample(1, i, right);
        }

        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        if (stream == nullptr)
            return false;

        juce::WavAudioFormat wav;
        const auto options = juce::AudioFormatWriterOptions()
            .withSampleRate(sampleRate)
            .withChannelLayout(juce::AudioChannelSet::stereo())
            .withBitsPerSample(32)
            .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);

        auto writer = wav.createWriterFor(stream, options);
        return writer != nullptr
            && writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
    }
}

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc != 3)
    {
        std::cerr << "Usage: StemForgeEngineSmoke <onnxruntime.dll> <model.onnx>\n";
        return 2;
    }

    const juce::File runtime(argv[1]);
    const juce::File model(argv[2]);
    if (! runtime.existsAsFile() || ! model.existsAsFile())
    {
        std::cerr << "Runtime or model file is missing.\n";
        return 3;
    }

    const auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                             .getChildFile("StemForge");
    const auto runtimeDir = appData.getChildFile("runtime");
    const auto modelDir = appData.getChildFile("models");
    if (runtimeDir.createDirectory().failed() || modelDir.createDirectory().failed())
    {
        std::cerr << "Could not create StemForge app-data folders.\n";
        return 4;
    }

    const auto installedRuntime = runtimeDir.getChildFile("onnxruntime.dll");
    const auto installedModel = modelDir.getChildFile("htdemucs_6s_fp16weights.onnx");
    installedRuntime.deleteFile();
    installedModel.deleteFile();

    if (! runtime.copyFileTo(installedRuntime) || ! model.copyFileTo(installedModel))
    {
        std::cerr << "Could not stage runtime/model for smoke test.\n";
        return 5;
    }

    const auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("StemForgeEngineSmoke");
    tempRoot.deleteRecursively();
    if (tempRoot.createDirectory().failed())
    {
        std::cerr << "Could not create smoke-test temp folder.\n";
        return 6;
    }

    const auto input = tempRoot.getChildFile("smoke_input.wav");
    const auto output = tempRoot.getChildFile("out");
    output.createDirectory();

    if (! writeTestWav(input))
    {
        std::cerr << "Could not create test WAV.\n";
        return 7;
    }

    stemforge::SeparationOptions options;
    options.exportStem.fill(true);
    options.exportCleanSample = true;
    options.outputDirectory = output;

    stemforge::StemEngine engine;

    const auto foundRuntime = engine.findRuntimeFile();
    const auto foundModel = engine.findModelFile();
    if (! foundRuntime.existsAsFile() || ! foundModel.existsAsFile())
    {
        std::cerr << "StemEngine resource discovery failed.\n";
        return 8;
    }

    if (! engine.start(input, options))
    {
        std::cerr << "StemEngine failed to start.\n";
        return 9;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    while (engine.isBusy() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (engine.isBusy())
    {
        engine.cancel();
        std::cerr << "StemEngine smoke test timed out.\n";
        return 10;
    }

    const auto error = engine.getLastError();
    if (error.isNotEmpty())
    {
        std::cerr << "StemEngine error: " << error << "\n";
        return 11;
    }

    const auto outputs = engine.getLastOutputs();
    if (outputs.size() != 7)
    {
        std::cerr << "Expected 7 output WAV files, got " << outputs.size() << "\n";
        return 12;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    for (const auto& file : outputs)
    {
        if (! file.existsAsFile() || file.getSize() <= 44)
        {
            std::cerr << "Invalid output file: " << file.getFullPathName() << "\n";
            return 13;
        }

        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (reader == nullptr || reader->sampleRate != stemforge::StemEngine::modelSampleRate
            || reader->numChannels != 2 || reader->lengthInSamples <= 0)
        {
            std::cerr << "Unreadable or malformed output WAV: " << file.getFullPathName() << "\n";
            return 14;
        }
    }

    std::cout << "STEMFORGE_ENGINE_SMOKE_SUCCESS outputs=" << outputs.size() << "\n";
    return 0;
}
