#include <JuceHeader.h>
#include "VocalEngine.h"

class StandaloneComponent : public juce::AudioAppComponent, private juce::Timer
{
public:
    StandaloneComponent()
    {
        setOpaque (true);
        setSize (1050, 720);

        formatManager.registerBasicFormats();

        loadButton.setButtonText ("LOAD WAV / MP3");
        playButton.setButtonText ("PLAY");
        stopButton.setButtonText ("STOP");

        addAndMakeVisible (loadButton);
        addAndMakeVisible (playButton);
        addAndMakeVisible (stopButton);
        addAndMakeVisible (presetBox);
        addAndMakeVisible (status);

        const juce::StringArray presets {
            "Male Rap - MID-TOP FORWARD",
            "Male Rap - AGGRESSIVE-GRIT",
            "Male Rap - LOW/CHEST",
            "Boom Bap - DUSTY-DARK",
            "Female - CLEAR AIR",
            "Female - SMOOTH DENSE",
            "Backs - WIDE GLUE",
            "Adlibs - SPACE CUT"
        };
        presetBox.addItemList (presets, 1);
        presetBox.setSelectedItemIndex (0);
        applyPreset (0);

        loadButton.onClick = [this] { chooseFile(); };
        playButton.onClick = [this]
        {
            if (readerSource != nullptr)
            {
                transport.setPosition (0.0);
                transport.start();
                status.setText ("Playing", juce::dontSendNotification);
            }
        };
        stopButton.onClick = [this]
        {
            transport.stop();
            transport.setPosition (0.0);
            status.setText ("Stopped", juce::dontSendNotification);
        };
        presetBox.onChange = [this] { applyPreset (presetBox.getSelectedItemIndex()); };

        setupKnob (doubler, "DOUBLER", 0.0, 1.0, settings.doubler);
        setupKnob (parallel, "PARALLEL", 0.0, 1.0, settings.parallel);
        setupKnob (sat, "SATURATION", 0.0, 1.0, settings.saturation);
        setupKnob (grit, "GRIT", 0.0, 1.0, settings.grit);
        setupKnob (air, "AIR", -4.0, 12.0, settings.airDb);
        setupKnob (width, "WIDTH", 0.55, 1.65, settings.width);
        setupKnob (reverb, "REVERB", 0.0, 1.0, settings.reverb);
        setupKnob (delay, "DELAY", 0.0, 1.0, settings.delay);

        doubler.onValueChange = [this]{ settings.doubler=(float)doubler.getValue(); sync(); };
        parallel.onValueChange = [this]{ settings.parallel=(float)parallel.getValue(); sync(); };
        sat.onValueChange = [this]{ settings.saturation=(float)sat.getValue(); sync(); };
        grit.onValueChange = [this]{ settings.grit=(float)grit.getValue(); sync(); };
        air.onValueChange = [this]{ settings.airDb=(float)air.getValue(); sync(); };
        width.onValueChange = [this]{ settings.width=(float)width.getValue(); sync(); };
        reverb.onValueChange = [this]{ settings.reverb=(float)reverb.getValue(); sync(); };
        delay.onValueChange = [this]{ settings.delay=(float)delay.getValue(); sync(); };

        setAudioChannels (0, 2);
        startTimerHz (30);
    }

    ~StandaloneComponent() override
    {
        shutdownAudio();
        transport.setSource (nullptr);
        readerSource.reset();
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        transport.prepareToPlay (samplesPerBlockExpected, sampleRate);
        engine.prepare (sampleRate, samplesPerBlockExpected, 2);
        engine.setSettings (settings);
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        if (readerSource == nullptr)
        {
            info.clearActiveBufferRegion();
            return;
        }

        transport.getNextAudioBlock (info);
        juce::AudioBuffer<float> view (info.buffer->getArrayOfWritePointers(),
                                      info.buffer->getNumChannels(),
                                      info.startSample,
                                      info.numSamples);
        engine.setSettings (settings);
        engine.process (view);
    }

    void releaseResources() override
    {
        transport.releaseResources();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff090b10));
        g.setColour (juce::Colour (0xfff8fafc));
        g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
        g.drawText ("VOCALFORGE ONE", 24, 18, getWidth()-48, 34, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff64748b));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("STANDALONE PRINT-READY VOCAL PROCESSOR", 26, 52, getWidth()-52, 20, juce::Justification::centredLeft);

        auto graph = juce::Rectangle<float> (26.0f, 145.0f, (float)getWidth()-52.0f, 230.0f);
        g.setColour (juce::Colour (0xff12151b));
        g.fillRoundedRectangle (graph, 12.0f);
        g.setColour (juce::Colour (0xff2b313d));
        for (int i=1;i<7;++i)
            g.drawVerticalLine ((int)(graph.getX()+graph.getWidth()*i/7.0f), graph.getY()+8.0f, graph.getBottom()-8.0f);
        for (int i=1;i<4;++i)
            g.drawHorizontalLine ((int)(graph.getY()+graph.getHeight()*i/4.0f), graph.getX()+8.0f, graph.getRight()-8.0f);

        juce::Path curve;
        for (int x=0;x<(int)graph.getWidth();++x)
        {
            const float n=(float)x/juce::jmax(1,(int)graph.getWidth()-1);
            const float f=20.0f*std::pow(1000.0f,n);
            auto bell=[](float ff,float c,float w){ float q=std::log2(ff/c)/w; return std::exp(-0.5f*q*q); };
            float db=settings.bodyDb*bell(f,180.0f,0.85f)+settings.presenceDb*bell(f,3300.0f,0.72f);
            db += settings.airDb*juce::jlimit(0.0f,1.0f,std::log2(f/7000.0f)+0.5f);
            if (f<settings.lowCutHz) db -= juce::jlimit(0.0f,24.0f,24.0f*std::log2(settings.lowCutHz/juce::jmax(20.0f,f)));
            float y=juce::jmap(juce::jlimit(-18.0f,18.0f,db),-18.0f,18.0f,graph.getBottom()-8.0f,graph.getY()+8.0f);
            if(x==0) curve.startNewSubPath(graph.getX(),y); else curve.lineTo(graph.getX()+x,y);
        }
        g.setColour (juce::Colour (0xff38bdf8));
        g.strokePath (curve, juce::PathStrokeType (2.2f));

        const float rms=juce::jlimit(0.0f,1.0f,engine.meterRms.load()*3.0f);
        auto m=juce::Rectangle<float>((float)getWidth()-40.0f,155.0f,8.0f,210.0f);
        g.setColour(juce::Colour(0xff253041)); g.fillRect(m);
        g.setColour(juce::Colour(0xff38bdf8)); g.fillRect(m.withTop(m.getBottom()-m.getHeight()*rms));
    }

    void resized() override
    {
        auto top = juce::Rectangle<int> (26, 86, getWidth()-52, 42);
        loadButton.setBounds (top.removeFromLeft (190).reduced (0, 3));
        top.removeFromLeft (8);
        playButton.setBounds (top.removeFromLeft (90).reduced (0, 3));
        top.removeFromLeft (6);
        stopButton.setBounds (top.removeFromLeft (90).reduced (0, 3));
        top.removeFromLeft (10);
        presetBox.setBounds (top.reduced (0, 3));

        status.setBounds (26, 382, getWidth()-52, 22);

        auto area = juce::Rectangle<int> (26, 420, getWidth()-52, getHeight()-444);
        std::array<juce::Slider*,8> arr { &doubler,&parallel,&sat,&grit,&air,&width,&reverb,&delay };
        const int cellW = area.getWidth()/8;
        for (int i=0;i<8;++i)
            arr[(size_t)i]->setBounds (area.getX()+i*cellW, area.getY(), cellW-6, area.getHeight());
    }

private:
    void setupKnob (juce::Slider& s, const juce::String& name, double min, double max, double value)
    {
        s.setName(name);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 20);
        s.setRange (min, max, 0.001);
        s.setValue (value);
        s.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff38bdf8));
        s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff293241));
        addAndMakeVisible (s);
    }

    void chooseFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Open vocal WAV or MP3", juce::File{}, "*.wav;*.mp3;*.aif;*.aiff");
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (! file.existsAsFile()) return;

            std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
            if (reader != nullptr)
            {
                transport.stop();
                transport.setSource (nullptr);
                readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
                transport.setSource (readerSource.get(), 0, nullptr, readerSource->getAudioFormatReader()->sampleRate);
                status.setText ("Loaded: " + file.getFileName(), juce::dontSendNotification);
            }
        });
    }

    void applyPreset (int i)
    {
        switch (juce::jlimit (0,7,i))
        {
            case 0: settings={1.0f,-54.0f,82.0f,-1.2f,4.2f,4.8f,0.52f,0.64f,0.46f,0.28f,0.18f,0.12f,0.18f,1.08f,0.035f,0.045f,-0.7f}; break;
            case 1: settings={1.8f,-52.0f,88.0f,-1.8f,5.3f,4.2f,0.58f,0.76f,0.52f,0.33f,0.31f,0.34f,0.16f,1.04f,0.025f,0.035f,-0.8f}; break;
            case 2: settings={0.7f,-55.0f,65.0f,3.4f,1.8f,1.9f,0.42f,0.58f,0.55f,0.31f,0.23f,0.08f,0.12f,1.02f,0.03f,0.05f,-0.7f}; break;
            case 3: settings={1.2f,-50.0f,72.0f,1.8f,0.8f,-0.7f,0.40f,0.67f,0.50f,0.36f,0.38f,0.28f,0.14f,1.06f,0.055f,0.07f,-0.9f}; break;
            case 4: settings={0.8f,-58.0f,95.0f,-0.8f,3.6f,6.8f,0.62f,0.54f,0.42f,0.22f,0.12f,0.07f,0.17f,1.10f,0.04f,0.075f,-0.8f}; break;
            case 5: settings={0.5f,-57.0f,88.0f,0.8f,2.4f,4.2f,0.68f,0.65f,0.56f,0.30f,0.20f,0.09f,0.13f,1.05f,0.03f,0.06f,-0.8f}; break;
            case 6: settings={0.0f,-54.0f,110.0f,-1.6f,2.0f,2.5f,0.52f,0.60f,0.43f,0.24f,0.13f,0.08f,0.48f,1.48f,0.08f,0.14f,-1.0f}; break;
            case 7: settings={0.0f,-52.0f,125.0f,-2.6f,3.4f,5.0f,0.58f,0.66f,0.40f,0.18f,0.19f,0.20f,0.32f,1.34f,0.24f,0.22f,-1.0f}; break;
        }
        syncKnobs();
        sync();
    }

    void syncKnobs()
    {
        doubler.setValue(settings.doubler,juce::dontSendNotification);
        parallel.setValue(settings.parallel,juce::dontSendNotification);
        sat.setValue(settings.saturation,juce::dontSendNotification);
        grit.setValue(settings.grit,juce::dontSendNotification);
        air.setValue(settings.airDb,juce::dontSendNotification);
        width.setValue(settings.width,juce::dontSendNotification);
        reverb.setValue(settings.reverb,juce::dontSendNotification);
        delay.setValue(settings.delay,juce::dontSendNotification);
    }

    void sync() { engine.setSettings(settings); repaint(); }
    void timerCallback() override { repaint(); }

    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::FileChooser> chooser;
    VocalEngine engine;
    VocalSettings settings;

    juce::TextButton loadButton, playButton, stopButton;
    juce::ComboBox presetBox;
    juce::Label status;
    juce::Slider doubler, parallel, sat, grit, air, width, reverb, delay;
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow() : DocumentWindow ("VocalForge ONE v2.0 PRO",
                                   juce::Colour (0xff090b10),
                                   DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, true);
        setResizeLimits (820, 560, 1600, 1100);
        setContentOwned (new StandaloneComponent(), true);
        centreWithSize (1050, 720);
        setVisible (true);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class VocalForgeApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "VocalForge ONE"; }
    const juce::String getApplicationVersion() override { return "2.0.0"; }
    void initialise (const juce::String&) override { window = std::make_unique<MainWindow>(); }
    void shutdown() override { window.reset(); }
private:
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (VocalForgeApplication)
