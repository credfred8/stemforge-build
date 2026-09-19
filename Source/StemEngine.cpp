#include "StemEngine.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <array>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX 1
 #endif
 #include <windows.h>
#endif

namespace
{
#if JUCE_WINDOWS
    static int stemForgeModuleAnchor = 0;

    juce::File getStemForgeModuleFile()
    {
        HMODULE module = nullptr;
        const auto address = reinterpret_cast<LPCWSTR>(&stemForgeModuleAnchor);
        if (::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                              | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               address, &module) == 0 || module == nullptr)
            return {};

        std::array<wchar_t, 32768> modulePath {};
        const auto length = ::GetModuleFileNameW(module, modulePath.data(),
                                                  static_cast<DWORD>(modulePath.size()));
        if (length == 0 || length >= modulePath.size())
            return {};

        return juce::File(juce::String(modulePath.data(), static_cast<int>(length)));
    }

    juce::File getStemForgeContentsDirectory()
    {
        const auto moduleFile = getStemForgeModuleFile();
        if (moduleFile == juce::File())
            return {};

        return moduleFile.getParentDirectory().getParentDirectory();
    }
#endif
}

namespace stemforge
{
    StemEngine::StemEngine() : juce::Thread("StemForge separation worker") {}

    StemEngine::~StemEngine()
    {
        cancel();
        stopThread(-1);
        env.reset();
        ortLibrary.reset();
    }

    bool StemEngine::start(const juce::File& audioFile, const SeparationOptions& options)
    {
        if (busy.load() || ! audioFile.existsAsFile())
            return false;

        {
            const std::scoped_lock lock(stateMutex);
            pendingInput = audioFile;
            pendingOptions = options;
            lastError.clear();
            lastOutputs.clear();
            status = juce::String(L"\u041f\u043e\u0434\u0433\u043e\u0442\u043e\u0432\u043a\u0430...");
        }

        progress.store(0.0f);
        busy.store(true);
        if (! startThread(juce::Thread::Priority::normal))
        {
            busy.store(false);
            fail(juce::String(L"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u0437\u0430\u043f\u0443\u0441\u0442\u0438\u0442\u044c worker thread."));
            return false;
        }
        return true;
    }

    void StemEngine::cancel() { signalThreadShouldExit(); }

    juce::String StemEngine::getStatus() const
    {
        const std::scoped_lock lock(stateMutex);
        return status;
    }

    juce::String StemEngine::getLastError() const
    {
        const std::scoped_lock lock(stateMutex);
        return lastError;
    }

    std::vector<juce::File> StemEngine::getLastOutputs() const
    {
        const std::scoped_lock lock(stateMutex);
        return lastOutputs;
    }

    void StemEngine::setStatus(const juce::String& text)
    {
        const std::scoped_lock lock(stateMutex);
        status = text;
    }

    void StemEngine::fail(const juce::String& text)
    {
        const std::scoped_lock lock(stateMutex);
        lastError = text;
        status = juce::String(L"\u041e\u0448\u0438\u0431\u043a\u0430: ") + text;
    }

    juce::File StemEngine::findModelFile() const
    {
        const auto modelName = juce::String("htdemucs_6s_fp16weights.onnx");
        std::vector<juce::File> candidates;
       #if JUCE_WINDOWS
        const auto contentsDir = getStemForgeContentsDirectory();
        if (contentsDir != juce::File())
            candidates.push_back(contentsDir.getChildFile("Resources").getChildFile("models").getChildFile(modelName));
       #endif
        candidates.push_back(juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                                 .getChildFile("StemForge").getChildFile("models").getChildFile(modelName));
        candidates.push_back(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                 .getChildFile("StemForge").getChildFile("models").getChildFile(modelName));
        for (const auto& candidate : candidates)
            if (candidate.existsAsFile())
                return candidate;
        return candidates.empty() ? juce::File() : candidates.front();
    }

    juce::File StemEngine::findRuntimeFile() const
    {
        const auto runtimeName = juce::String("onnxruntime.dll");
        std::vector<juce::File> candidates;
       #if JUCE_WINDOWS
        const auto contentsDir = getStemForgeContentsDirectory();
        if (contentsDir != juce::File())
        {
            candidates.push_back(contentsDir.getChildFile("Resources").getChildFile("runtime").getChildFile(runtimeName));
            candidates.push_back(contentsDir.getChildFile("x86_64-win").getChildFile(runtimeName));
        }
       #endif
        candidates.push_back(juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                                 .getChildFile("StemForge").getChildFile("runtime").getChildFile(runtimeName));
        candidates.push_back(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                 .getChildFile("StemForge").getChildFile("runtime").getChildFile(runtimeName));
        for (const auto& candidate : candidates)
            if (candidate.existsAsFile())
                return candidate;
        return candidates.empty() ? juce::File() : candidates.front();
    }

    bool StemEngine::ensureRuntimeLoaded()
    {
        if (env != nullptr && ortLibrary != nullptr)
            return true;

        const auto runtimeFile = findRuntimeFile();
        if (! runtimeFile.existsAsFile())
        {
            fail(juce::String(L"\u041d\u0435 \u043d\u0430\u0439\u0434\u0435\u043d ONNX Runtime: ") + runtimeFile.getFullPathName());
            return false;
        }

        auto library = std::make_unique<juce::DynamicLibrary>();
        if (! library->open(runtimeFile.getFullPathName()))
        {
            fail(juce::String(L"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u0437\u0430\u0433\u0440\u0443\u0437\u0438\u0442\u044c ONNX Runtime: ") + runtimeFile.getFullPathName());
            return false;
        }

        using OrtGetApiBaseFn = const OrtApiBase* (ORT_API_CALL*)();
        auto* symbol = library->getFunction("OrtGetApiBase");
        if (symbol == nullptr)
        {
            fail(juce::String(L"ONNX Runtime \u043d\u0435 \u044d\u043a\u0441\u043f\u043e\u0440\u0442\u0438\u0440\u0443\u0435\u0442 OrtGetApiBase."));
            return false;
        }

        const auto getApiBase = reinterpret_cast<OrtGetApiBaseFn>(symbol);
        const auto* apiBase = getApiBase();
        if (apiBase == nullptr)
        {
            fail(juce::String(L"ONNX Runtime \u0432\u0435\u0440\u043d\u0443\u043b \u043f\u0443\u0441\u0442\u043e\u0439 API base."));
            return false;
        }

        const auto* api = apiBase->GetApi(ORT_API_VERSION);
        if (api == nullptr)
        {
            fail(juce::String(L"\u0412\u0435\u0440\u0441\u0438\u044f ONNX Runtime API \u043d\u0435\u0441\u043e\u0432\u043c\u0435\u0441\u0442\u0438\u043c\u0430 \u0441 StemForge."));
            return false;
        }

        try
        {
            Ort::InitApi(api);
            auto runtimeEnv = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "StemForge");
            ortLibrary = std::move(library);
            env = std::move(runtimeEnv);
        }
        catch (const Ort::Exception& e)
        {
            fail(juce::String(L"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u0438\u043d\u0438\u0446\u0438\u0430\u043b\u0438\u0437\u0438\u0440\u043e\u0432\u0430\u0442\u044c ONNX Runtime: ") + juce::String(e.what()));
            return false;
        }
        return true;
    }

    std::unique_ptr<Ort::Session> StemEngine::createSession(const juce::File& modelFile)
    {
        if (env == nullptr)
            return {};

        Ort::SessionOptions options;
        const auto hw = std::max(1u, std::thread::hardware_concurrency());
        options.SetIntraOpNumThreads(static_cast<int>(std::max(1u, hw - 1u)));
        options.SetInterOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
       #if JUCE_WINDOWS
        const auto modelPath = modelFile.getFullPathName();
        return std::make_unique<Ort::Session>(*env, modelPath.toWideCharPointer(), options);
       #else
        const auto modelPath = modelFile.getFullPathName();
        return std::make_unique<Ort::Session>(*env, modelPath.toRawUTF8(), options);
       #endif
    }

    void StemEngine::run()
    {
        juce::File input;
        SeparationOptions options;
        {
            const std::scoped_lock lock(stateMutex);
            input = pendingInput;
            options = pendingOptions;
        }

        const auto finish = [this]
        {
            busy.store(false);
            if (! threadShouldExit() && getLastError().isEmpty())
            {
                progress.store(1.0f);
                setStatus(juce::String(L"\u0413\u043e\u0442\u043e\u0432\u043e. \u0421\u0442\u0435\u043c\u044b \u0441\u043e\u0445\u0440\u0430\u043d\u0435\u043d\u044b."));
            }
            else if (threadShouldExit() && getLastError().isEmpty())
                setStatus(juce::String(L"\u041e\u0441\u0442\u0430\u043d\u043e\u0432\u043b\u0435\u043d\u043e."));
        };

        const auto model = findModelFile();
        if (! model.existsAsFile())
        {
            fail(juce::String(L"\u041d\u0435 \u043d\u0430\u0439\u0434\u0435\u043d\u0430 \u043c\u043e\u0434\u0435\u043b\u044c ") + model.getFullPathName());
            finish();
            return;
        }

        juce::AudioBuffer<float> mix;
        setStatus(juce::String(L"\u0427\u0438\u0442\u0430\u044e \u0430\u0443\u0434\u0438\u043e \u0438 \u043f\u0440\u0438\u0432\u043e\u0436\u0443 \u043a 44.1 \u043a\u0413\u0446 stereo..."));
        if (! loadAndResample(input, mix)) { finish(); return; }
        if (threadShouldExit()) { finish(); return; }

        progress.store(0.05f);
        std::vector<juce::AudioBuffer<float>> stems;
        if (! separate(mix, stems)) { finish(); return; }
        if (threadShouldExit()) { finish(); return; }

        setStatus(juce::String(L"\u042d\u043a\u0441\u043f\u043e\u0440\u0442\u0438\u0440\u0443\u044e 32-bit float WAV..."));
        progress.store(0.95f);
        if (! writeOutputs(mix, stems, input, options)) { finish(); return; }
        finish();
    }

    bool StemEngine::loadAndResample(const juce::File& file, juce::AudioBuffer<float>& destination)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (reader == nullptr)
        {
            fail(juce::String(L"\u0424\u043e\u0440\u043c\u0430\u0442 \u0444\u0430\u0439\u043b\u0430 \u043d\u0435 \u043f\u0440\u043e\u0447\u0438\u0442\u0430\u043d: ") + file.getFileName());
            return false;
        }
        if (reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        {
            fail(juce::String(L"\u0410\u0443\u0434\u0438\u043e\u0444\u0430\u0439\u043b \u043f\u0443\u0441\u0442\u043e\u0439 \u0438\u043b\u0438 \u043f\u043e\u0432\u0440\u0435\u0436\u0434\u0451\u043d."));
            return false;
        }

        const int sourceChannels = static_cast<int>(reader->numChannels);
        const double sourceRate = reader->sampleRate;
        const int64_t sourceLength = reader->lengthInSamples;
        const int outputLength = static_cast<int>(std::ceil(static_cast<double>(sourceLength)
                                                            * modelSampleRate / sourceRate));
        if (outputLength <= 0)
        {
            fail(juce::String(L"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u043e\u043f\u0440\u0435\u0434\u0435\u043b\u0438\u0442\u044c \u0434\u043b\u0438\u043d\u0443 \u0430\u0443\u0434\u0438\u043e."));
            return false;
        }

        auto readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
        juce::ResamplingAudioSource resampler(readerSource.get(), false, 2);
        resampler.setResamplingRatio(sourceRate / static_cast<double>(modelSampleRate));
        resampler.prepareToPlay(8192, modelSampleRate);

        destination.setSize(2, outputLength, false, true, false);
        destination.clear();

        constexpr int blockSize = 8192;
        juce::AudioBuffer<float> temp(2, blockSize);
        int written = 0;
        while (written < outputLength && ! threadShouldExit())
        {
            const int count = std::min(blockSize, outputLength - written);
            temp.clear();
            juce::AudioSourceChannelInfo info(&temp, 0, count);
            resampler.getNextAudioBlock(info);
            destination.copyFrom(0, written, temp, 0, 0, count);
            destination.copyFrom(1, written, temp, 1, 0, count);
            written += count;
        }
        resampler.releaseResources();

        if (sourceChannels == 1)
            destination.copyFrom(1, 0, destination, 0, 0, outputLength);
        return ! threadShouldExit();
    }

    std::vector<float> StemEngine::makeWindow() const
    {
        std::vector<float> window(static_cast<size_t>(segmentSamples), 1.0f);
        for (int i = 0; i < overlapSamples; ++i)
        {
            const float t = overlapSamples > 1
                ? static_cast<float>(i) / static_cast<float>(overlapSamples - 1) : 1.0f;
            window[static_cast<size_t>(i)] = t;
            window[static_cast<size_t>(segmentSamples - 1 - i)] = t;
        }
        return window;
    }

    bool StemEngine::separate(const juce::AudioBuffer<float>& mix,
                              std::vector<juce::AudioBuffer<float>>& stems)
    {
        const auto model = findModelFile();
        setStatus(juce::String(L"\u0417\u0430\u0433\u0440\u0443\u0436\u0430\u044e HT-Demucs 6-stem..."));
        if (! ensureRuntimeLoaded())
            return false;

        std::unique_ptr<Ort::Session> session;
        try
        {
            session = createSession(model);
            if (session == nullptr)
            {
                fail(juce::String(L"ONNX Runtime \u043d\u0435 \u0438\u043d\u0438\u0446\u0438\u0430\u043b\u0438\u0437\u0438\u0440\u043e\u0432\u0430\u043d."));
                return false;
            }
        }
        catch (const Ort::Exception& e)
        {
            fail("ONNX Runtime: " + juce::String(e.what()));
            return false;
        }

        const int total = mix.getNumSamples();
        stems.clear();
        stems.reserve(stemCount);
        for (int s = 0; s < stemCount; ++s)
        {
            stems.emplace_back(2, total);
            stems.back().clear();
        }

        std::vector<float> weight(static_cast<size_t>(total), 0.0f);
        const auto window = makeWindow();
        const int nChunks = total <= segmentSamples
            ? 1
            : 1 + (total - segmentSamples + strideSamples - 1) / strideSamples;
        std::vector<float> inputTensor(static_cast<size_t>(2 * segmentSamples), 0.0f);
        const std::array<int64_t, 3> inputShape { 1, 2, segmentSamples };
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const char* inputNames[] = { "mix" };
        const char* outputNames[] = { "stems" };

        for (int chunkIndex = 0; chunkIndex < nChunks; ++chunkIndex)
        {
            if (threadShouldExit())
                return false;

            const int start = chunkIndex * strideSamples;
            const int end = std::min(start + segmentSamples, total);
            const int chunkLength = end - start;
            std::fill(inputTensor.begin(), inputTensor.end(), 0.0f);

            std::memcpy(inputTensor.data(), mix.getReadPointer(0, start),
                        static_cast<size_t>(chunkLength) * sizeof(float));
            std::memcpy(inputTensor.data() + segmentSamples, mix.getReadPointer(1, start),
                        static_cast<size_t>(chunkLength) * sizeof(float));

            auto tensor = Ort::Value::CreateTensor<float>(memoryInfo, inputTensor.data(),
                inputTensor.size(), inputShape.data(), inputShape.size());

            std::vector<Ort::Value> result;
            try
            {
                result = session->Run(Ort::RunOptions { nullptr },
                                      inputNames, &tensor, 1, outputNames, 1);
            }
            catch (const Ort::Exception& e)
            {
                fail(juce::String(L"\u041e\u0448\u0438\u0431\u043a\u0430 \u043c\u043e\u0434\u0435\u043b\u0438: ") + juce::String(e.what()));
                return false;
            }

            if (result.empty() || ! result.front().IsTensor())
            {
                fail(juce::String(L"\u041c\u043e\u0434\u0435\u043b\u044c \u043d\u0435 \u0432\u0435\u0440\u043d\u0443\u043b\u0430 tensor stems."));
                return false;
            }

            const auto shape = result.front().GetTensorTypeAndShapeInfo().GetShape();
            if (shape.size() != 4 || shape[1] != stemCount || shape[2] != 2 || shape[3] != segmentSamples)
            {
                fail(juce::String(L"\u041d\u0435\u043e\u0436\u0438\u0434\u0430\u043d\u043d\u0430\u044f \u0444\u043e\u0440\u043c\u0430 \u0432\u044b\u0445\u043e\u0434\u0430 ONNX-\u043c\u043e\u0434\u0435\u043b\u0438."));
                return false;
            }

            const float* output = result.front().GetTensorData<float>();
            for (int i = 0; i < chunkLength; ++i)
            {
                float w = window[static_cast<size_t>(i)];
                if (start == 0 && i < overlapSamples)
                    w = 1.0f;
                if (end == total && i >= std::max(0, chunkLength - overlapSamples))
                    w = 1.0f;

                weight[static_cast<size_t>(start + i)] += w;
                for (int s = 0; s < stemCount; ++s)
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const size_t offset = (static_cast<size_t>(s) * 2u + static_cast<size_t>(ch))
                                              * static_cast<size_t>(segmentSamples) + static_cast<size_t>(i);
                        stems[static_cast<size_t>(s)].getWritePointer(ch)[start + i] += output[offset] * w;
                    }
            }

            const float fraction = static_cast<float>(chunkIndex + 1) / static_cast<float>(nChunks);
            progress.store(0.05f + 0.88f * fraction);
            setStatus(juce::String(L"\u0420\u0430\u0437\u0434\u0435\u043b\u0435\u043d\u0438\u0435 HQ: \u0431\u043b\u043e\u043a ") + juce::String(chunkIndex + 1) + " / " + juce::String(nChunks));
        }

        for (int i = 0; i < total; ++i)
        {
            const float inv = 1.0f / std::max(weight[static_cast<size_t>(i)], 1.0e-8f);
            for (auto& stem : stems)
            {
                stem.getWritePointer(0)[i] *= inv;
                stem.getWritePointer(1)[i] *= inv;
            }
        }
        return true;
    }

    bool StemEngine::writeFloatWav(const juce::File& file, const juce::AudioBuffer<float>& audio) const
    {
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        if (stream == nullptr)
            return false;

        juce::WavAudioFormat wav;
        const auto writerOptions = juce::AudioFormatWriterOptions()
            .withSampleRate(modelSampleRate)
            .withChannelLayout(juce::AudioChannelSet::stereo())
            .withBitsPerSample(32)
            .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);

        auto writer = wav.createWriterFor(stream, writerOptions);
        if (writer == nullptr)
            return false;

        return writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
    }

    bool StemEngine::writeOutputs(const juce::AudioBuffer<float>& mix,
                                  const std::vector<juce::AudioBuffer<float>>& stems,
                                  const juce::File& inputFile,
                                  const SeparationOptions& options)
    {
        juce::ignoreUnused(mix);
        auto outputDir = options.outputDirectory;
        if (outputDir == juce::File())
            outputDir = inputFile.getParentDirectory().getChildFile(inputFile.getFileNameWithoutExtension() + "_StemForge");

        if (outputDir.createDirectory().failed())
        {
            fail(juce::String(L"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u0441\u043e\u0437\u0434\u0430\u0442\u044c \u043f\u0430\u043f\u043a\u0443: ") + outputDir.getFullPathName());
            return false;
        }

        const auto base = inputFile.getFileNameWithoutExtension();
        std::vector<juce::File> writtenFiles;

        for (int s = 0; s < stemCount; ++s)
        {
            if (! options.exportStem[static_cast<size_t>(s)])
                continue;
            const auto target = outputDir.getChildFile(base + "_" + stemNames[static_cast<size_t>(s)] + ".wav");
            if (! writeFloatWav(target, stems[static_cast<size_t>(s)]))
            {
                fail(juce::String(L"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u0437\u0430\u043f\u0438\u0441\u0430\u0442\u044c ") + target.getFileName());
                return false;
            }
            writtenFiles.push_back(target);
        }

        if (options.exportCleanSample)
        {
            juce::AudioBuffer<float> clean(2, stems.front().getNumSamples());
            clean.clear();
            bool kept = false;
            for (int s = 0; s < stemCount; ++s)
            {
                if (options.exportStem[static_cast<size_t>(s)])
                    continue;
                clean.addFrom(0, 0, stems[static_cast<size_t>(s)], 0, 0, clean.getNumSamples());
                clean.addFrom(1, 0, stems[static_cast<size_t>(s)], 1, 0, clean.getNumSamples());
                kept = true;
            }
            if (! kept)
                clean.clear();

            juce::String suffix = "clean";
            juce::StringArray removed;
            for (int s = 0; s < stemCount; ++s)
                if (options.exportStem[static_cast<size_t>(s)])
                    removed.add(stemNames[static_cast<size_t>(s)]);
            if (! removed.isEmpty())
                suffix += "_no_" + removed.joinIntoString("-");

            const auto target = outputDir.getChildFile(base + "_" + suffix + ".wav");
            if (! writeFloatWav(target, clean))
            {
                fail(juce::String(L"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u0437\u0430\u043f\u0438\u0441\u0430\u0442\u044c ") + target.getFileName());
                return false;
            }
            writtenFiles.push_back(target);
        }

        if (writtenFiles.empty())
        {
            fail(juce::String(L"\u041d\u0435 \u0432\u044b\u0431\u0440\u0430\u043d \u043d\u0438 \u043e\u0434\u0438\u043d \u0440\u0435\u0437\u0443\u043b\u044c\u0442\u0430\u0442 \u0434\u043b\u044f \u044d\u043a\u0441\u043f\u043e\u0440\u0442\u0430."));
            return false;
        }

        {
            const std::scoped_lock lock(stateMutex);
            lastOutputs = std::move(writtenFiles);
        }
        return true;
    }
}
