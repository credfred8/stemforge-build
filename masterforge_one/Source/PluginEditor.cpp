#include "PluginEditor.h"
#include <MasterForgeWebAssets.h>

namespace
{
std::vector<std::byte> bytesFromResource (const char* data, int size)
{
    const auto* begin = reinterpret_cast<const std::byte*> (data);
    return { begin, begin + size };
}

juce::String mimeForPath (const juce::String& path)
{
    if (path.endsWithIgnoreCase (".css")) return "text/css";
    if (path.endsWithIgnoreCase (".js"))  return "text/javascript";
    return "text/html";
}

juce::AudioProcessorParameterWithID* findParameterById (juce::AudioProcessor& processor,
                                                         const juce::String& id)
{
    for (auto* p : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (withId->paramID == id)
                return withId;

    return nullptr;
}

float rawValueFor (juce::AudioProcessorParameter& parameter, float normalised)
{
    if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (&parameter))
        return p->convertFrom0to1 (normalised);

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (&parameter))
        return (float) p->getNormalisableRange().convertFrom0to1 (normalised);

    return normalised;
}

float normalisedForRaw (juce::AudioProcessorParameter& parameter, float raw)
{
    if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (&parameter))
        return p->convertTo0to1 (raw);

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (&parameter))
        return p->getNormalisableRange().convertTo0to1 (raw);

    return juce::jlimit (0.0f, 1.0f, raw);
}
}

bool MasterForgeAudioProcessorEditor::SinglePageBrowser::pageAboutToLoad (const juce::String& newURL)
{
    return newURL.startsWith (juce::WebBrowserComponent::getResourceProviderRoot());
}

MasterForgeAudioProcessorEditor::MasterForgeAudioProcessorEditor (MasterForgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    auto options = juce::WebBrowserComponent::Options{}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
            .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                .getChildFile ("MasterForgeOneWebView"))
            .withStatusBarDisabled()
            .withBuiltInErrorPageDisabled()
            .withBackgroundColour (juce::Colour (0xff05070a)))
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withEventListener ("uiReady", [this] (juce::var)
        {
            browserReady = true;
            sendState();
        })
        .withEventListener ("setParam", [this] (juce::var payload)
        {
            handleSetParam (std::move (payload));
        })
        .withEventListener ("gesture", [this] (juce::var payload)
        {
            handleGesture (std::move (payload));
        })
        .withEventListener ("preset", [this] (juce::var payload)
        {
            handlePreset (std::move (payload));
        })
        .withEventListener ("chain", [this] (juce::var payload)
        {
            handleChain (std::move (payload));
        })
        .withResourceProvider ([this] (const auto& url)
        {
            return getResource (url);
        });

    browser = std::make_unique<SinglePageBrowser> (options);
    addAndMakeVisible (*browser);
    browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (1050, 680, 1920, 1200);
    setSize (1360, 820);

    startTimerHz (60);
}

MasterForgeAudioProcessorEditor::~MasterForgeAudioProcessorEditor()
{
    stopTimer();
    browser.reset();
}

std::optional<juce::WebBrowserComponent::Resource>
MasterForgeAudioProcessorEditor::getResource (const juce::String& url)
{
    const auto path = url == "/" ? juce::String ("index.html")
                                 : url.trimCharactersAtStart ("/");

    juce::String resourceName;
    if (path == "index.html")      resourceName = "index_html";
    else if (path == "style.css") resourceName = "style_css";
    else if (path == "app.js")    resourceName = "app_js";
    else return std::nullopt;

    int size = 0;
    if (auto* data = MasterForgeWeb::getNamedResource (resourceName.toRawUTF8(), size))
        return juce::WebBrowserComponent::Resource { bytesFromResource (data, size), mimeForPath (path) };

    return std::nullopt;
}

void MasterForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff05070a));
}

void MasterForgeAudioProcessorEditor::resized()
{
    if (browser != nullptr)
        browser->setBounds (getLocalBounds());
}

void MasterForgeAudioProcessorEditor::timerCallback()
{
    if (browserReady)
        sendState();
}

juce::var MasterForgeAudioProcessorEditor::makeParameterSnapshot() const
{
    juce::DynamicObject::Ptr params = new juce::DynamicObject();

    for (auto* base : processor.getParameters())
    {
        auto* param = dynamic_cast<juce::AudioProcessorParameterWithID*> (base);
        if (param == nullptr)
            continue;

        const auto norm = param->getValue();
        const auto def = param->getDefaultValue();

        juce::DynamicObject::Ptr item = new juce::DynamicObject();
        item->setProperty ("norm", norm);
        item->setProperty ("raw", rawValueFor (*param, norm));
        item->setProperty ("def", def);
        item->setProperty ("defRaw", rawValueFor (*param, def));
        item->setProperty ("text", param->getText (norm, 64));

        params->setProperty (param->paramID, juce::var (item.get()));
    }

    return juce::var (params.get());
}

juce::var MasterForgeAudioProcessorEditor::makeMeterSnapshot() const
{
    juce::DynamicObject::Ptr meters = new juce::DynamicObject();
    meters->setProperty ("inputRms", processor.engine.inputRmsDb.load());
    meters->setProperty ("inputPeak", processor.engine.inputPeakDb.load());
    meters->setProperty ("outputRms", processor.engine.outputRmsDb.load());
    meters->setProperty ("outputPeak", processor.engine.outputPeakDb.load());
    meters->setProperty ("lufs", processor.engine.loudnessEstimate.load());
    meters->setProperty ("crest", processor.engine.crestDb.load());
    meters->setProperty ("correlation", processor.engine.correlation.load());
    meters->setProperty ("limiterGR", processor.engine.limiterReductionDb.load());
    meters->setProperty ("inputGain", processor.engine.smartGainAppliedDb.load());
    return juce::var (meters.get());
}

void MasterForgeAudioProcessorEditor::sendState()
{
    if (browser == nullptr)
        return;

    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("params", makeParameterSnapshot());
    root->setProperty ("meters", makeMeterSnapshot());

    juce::Array<juce::var> pre;
    juce::Array<juce::var> post;
    pre.ensureStorageAllocated (MasterEngine::spectrumBins);
    post.ensureStorageAllocated (MasterEngine::spectrumBins);

    for (int i = 0; i < MasterEngine::spectrumBins; ++i)
    {
        pre.add (processor.engine.preSpectrum[(size_t) i].load());
        post.add (processor.engine.postSpectrum[(size_t) i].load());
    }

    root->setProperty ("pre", juce::var (pre));
    root->setProperty ("post", juce::var (post));

    juce::Array<juce::var> presets;
    for (const auto& name : processor.presetNames)
        presets.add (name);

    root->setProperty ("presets", juce::var (presets));
    root->setProperty ("presetIndex", processor.getPresetIndex());

    juce::Array<juce::var> chain;
    for (auto index : processor.getModuleChain())
        chain.add (MasterForgeAudioProcessor::moduleIdForIndex (index));
    root->setProperty ("chain", juce::var (chain));

    browser->emitEventIfBrowserIsVisible ("state", juce::var (root.get()));
}

void MasterForgeAudioProcessorEditor::handleSetParam (juce::var payload)
{
    const auto id = payload.getProperty ("id", {}).toString();
    if (id.isEmpty())
        return;

    auto* param = findParameterById (processor, id);
    if (param == nullptr)
        return;

    float normalised = param->getValue();

    if (payload.hasProperty ("norm"))
        normalised = (float) payload.getProperty ("norm", normalised);
    else if (payload.hasProperty ("raw"))
        normalised = normalisedForRaw (*param, (float) payload.getProperty ("raw", 0.0));

    param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
}

void MasterForgeAudioProcessorEditor::handleGesture (juce::var payload)
{
    const auto id = payload.getProperty ("id", {}).toString();
    const auto phase = payload.getProperty ("phase", {}).toString();

    if (auto* param = findParameterById (processor, id))
    {
        if (phase == "begin") param->beginChangeGesture();
        if (phase == "end")   param->endChangeGesture();
    }
}

void MasterForgeAudioProcessorEditor::handlePreset (juce::var payload)
{
    processor.applyPreset (juce::jlimit (0, (int) processor.presetNames.size() - 1, (int) payload));
    sendState();
}


void MasterForgeAudioProcessorEditor::handleChain (juce::var payload)
{
    std::vector<int> chain;

    if (auto* array = payload.getArray())
    {
        chain.reserve ((size_t) array->size());

        for (const auto& item : *array)
        {
            const int index = MasterForgeAudioProcessor::moduleIndexForId (item.toString());
            if (index >= 0)
                chain.push_back (index);
        }
    }

    processor.setModuleChain (chain);
    sendState();
}
