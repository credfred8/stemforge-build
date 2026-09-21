#include <JuceHeader.h>
#include "VocalEngine.h"
#include "PremiumUI.h"

using namespace VocalForgeUI;

class StandaloneComponent : public juce::AudioAppComponent, private juce::Timer
{
public:
    StandaloneComponent()
    {
        setLookAndFeel (&laf);
        setOpaque (true);
        setSize (1280, 790);

        formatManager.registerBasicFormats();

        brand.setText ("VOCALFORGE ONE", juce::dontSendNotification);
        brand.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
        brand.setColour (juce::Label::textColourId, text);
        addAndMakeVisible (brand);

        edition.setText ("PRO VOCAL SUITE  /  STANDALONE v2.1", juce::dontSendNotification);
        edition.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        edition.setColour (juce::Label::textColourId, muted);
        addAndMakeVisible (edition);

        loadButton.setButtonText ("LOAD WAV / MP3");
        playButton.setButtonText ("PLAY");
        stopButton.setButtonText ("STOP");
        addAndMakeVisible (loadButton);
        addAndMakeVisible (playButton);
        addAndMakeVisible (stopButton);

        presetBox.addItemList ({
            "Male Rap - MID-TOP FORWARD",
            "Male Rap - AGGRESSIVE-GRIT",
            "Male Rap - LOW/CHEST",
            "Boom Bap - DUSTY-DARK",
            "Female - CLEAR AIR",
            "Female - SMOOTH DENSE",
            "Backs - WIDE GLUE",
            "Adlibs - SPACE CUT"
        }, 1);
        presetBox.setSelectedItemIndex (0);
        presetBox.setTooltip ("Factory vocal chains tuned for different voices and roles.");
        addAndMakeVisible (presetBox);

        chainLabel.setText ("SIGNAL CHAIN", juce::dontSendNotification);
        chainLabel.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        chainLabel.setColour (juce::Label::textColourId, muted);
        addAndMakeVisible (chainLabel);

        const std::array<juce::String, 12> names {
            "CLEAN","GATE","EQ","DE-ESS","PEAK COMP","LEVELER",
            "PARALLEL","SATURATION","DOUBLER","SPACE","WIDTH","LIMIT"
        };
        const std::array<juce::String, 12> tips {
            "Removes rumble and unnecessary low buildup.",
            "Reduces room noise between phrases.",
            "Shapes body, presence and air.",
            "Controls harsh consonants.",
            "Fast peak compression for punch and stability.",
            "Slow leveling for consistent phrases.",
            "Parallel compression for density.",
            "Harmonic colour and controlled grit.",
            "Blended short offset voices for width and thickness.",
            "Delay and short ambience.",
            "Stereo spread after modulation and space.",
            "Final peak protection and output control."
        };

        for (int i = 0; i < 12; ++i)
        {
            modules[(size_t)i] = std::make_unique<ModuleTile> (names[(size_t)i], tips[(size_t)i]);
            addAndMakeVisible (*modules[(size_t)i]);
        }

        status.setText ("Ready - load a WAV or MP3 vocal", juce::dontSendNotification);
        status.setColour (juce::Label::textColourId, muted);
        status.setFont (juce::Font (juce::FontOptions (10.5f)));
        addAndMakeVisible (status);

        setupKnob (lowCut, "LOW CUT", 45.0, 180.0, settings.lowCutHz, " Hz");
        setupKnob (gate, "GATE", -75.0, -25.0, settings.gateDb, " dB");
        setupKnob (body, "BODY", -8.0, 8.0, settings.bodyDb, " dB");
        setupKnob (presence, "PRESENCE", -8.0, 10.0, settings.presenceDb, " dB");
        setupKnob (air, "AIR", -4.0, 12.0, settings.airDb, " dB");
        setupKnob (deess, "DE-ESS", 0.0, 1.0, settings.deEss);
        setupKnob (comp, "PEAK COMP", 0.0, 1.0, settings.comp);
        setupKnob (parallel, "PARALLEL", 0.0, 1.0, settings.parallel);
        setupKnob (sat, "SATURATION", 0.0, 1.0, settings.saturation);
        setupKnob (doubler, "DOUBLER", 0.0, 1.0, settings.doubler);
        setupKnob (reverb, "AMBIENCE", 0.0, 1.0, settings.reverb);
        setupKnob (width, "WIDTH", 0.55, 1.65, settings.width);

        lowCut.onValueChange=[this]{settings.lowCutHz=(float)lowCut.getValue();sync();};
        gate.onValueChange=[this]{settings.gateDb=(float)gate.getValue();sync();};
        body.onValueChange=[this]{settings.bodyDb=(float)body.getValue();sync();};
        presence.onValueChange=[this]{settings.presenceDb=(float)presence.getValue();sync();};
        air.onValueChange=[this]{settings.airDb=(float)air.getValue();sync();};
        deess.onValueChange=[this]{settings.deEss=(float)deess.getValue();sync();};
        comp.onValueChange=[this]{settings.comp=(float)comp.getValue();sync();};
        parallel.onValueChange=[this]{settings.parallel=(float)parallel.getValue();sync();};
        sat.onValueChange=[this]{settings.saturation=(float)sat.getValue();sync();};
        doubler.onValueChange=[this]{settings.doubler=(float)doubler.getValue();sync();};
        reverb.onValueChange=[this]{settings.reverb=(float)reverb.getValue();sync();};
        width.onValueChange=[this]{settings.width=(float)width.getValue();sync();};

        loadButton.onClick = [this]{ chooseFile(); };
        playButton.onClick = [this]
        {
            if (readerSource)
            {
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
        presetBox.onChange = [this]{ applyPreset (presetBox.getSelectedItemIndex()); };

        applyPreset (0);
        setAudioChannels (0, 2);
        startTimerHz (30);
    }

    ~StandaloneComponent() override
    {
        shutdownAudio();
        transport.setSource (nullptr);
        readerSource.reset();
        setLookAndFeel (nullptr);
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        transport.prepareToPlay (samplesPerBlockExpected, sampleRate);
        engine.prepare (sampleRate, samplesPerBlockExpected, 2);
        engine.setSettings (settings);
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        if (!readerSource)
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

    void releaseResources() override { transport.releaseResources(); }

    void paint (juce::Graphics& g) override
    {
        juce::ColourGradient bg (bg0, 0.0f, 0.0f, bg1, 0.0f, (float)getHeight(), false);
        g.setGradientFill (bg); g.fillAll();

        g.setColour (cyan.withAlpha (0.85f));
        g.fillRoundedRectangle (18.0f, 17.0f, 4.0f, 35.0f, 2.0f);

        g.setColour (border.withAlpha (0.65f));
        g.drawHorizontalLine (72, 18.0f, (float)getWidth()-18.0f);

        auto chain = juce::Rectangle<float> (18.0f, 96.0f, (float)getWidth()-36.0f, 118.0f);
        g.setColour (panel.withAlpha (0.72f));
        g.fillRoundedRectangle (chain, 12.0f);
        g.setColour (border.withAlpha (0.8f));
        g.drawRoundedRectangle (chain, 12.0f, 1.0f);

        auto graph = juce::Rectangle<float> (18.0f, 226.0f, (float)getWidth()-36.0f, (float)getHeight()-460.0f);
        g.setColour (panel); g.fillRoundedRectangle (graph, 12.0f);

        auto plot = graph.reduced (18.0f);
        g.setColour (border.withAlpha (0.62f));
        for (int i=1;i<8;++i) g.drawVerticalLine ((int)(plot.getX()+plot.getWidth()*i/8.0f), plot.getY(), plot.getBottom());
        for (int i=1;i<6;++i) g.drawHorizontalLine ((int)(plot.getY()+plot.getHeight()*i/6.0f), plot.getX(), plot.getRight());

        juce::Path curve;
        for (int x=0; x<(int)plot.getWidth(); ++x)
        {
            const float n=(float)x/juce::jmax(1,(int)plot.getWidth()-1);
            const float f=20.0f*std::pow(1000.0f,n);
            auto bell=[](float ff,float c,float w){const float q=std::log2(ff/c)/w;return std::exp(-0.5f*q*q);};
            float db=0.0f;
            if(f<settings.lowCutHz) db-=juce::jlimit(0.0f,24.0f,24.0f*std::log2(settings.lowCutHz/juce::jmax(20.0f,f)));
            db+=settings.bodyDb*bell(f,180.0f,0.85f);
            db+=settings.presenceDb*bell(f,3300.0f,0.72f);
            db+=settings.airDb*juce::jlimit(0.0f,1.0f,std::log2(f/7000.0f)+0.5f);
            const float y=juce::jmap(juce::jlimit(-18.0f,18.0f,db),-18.0f,18.0f,plot.getBottom()-18.0f,plot.getY()+8.0f);
            const float px=plot.getX()+(float)x;
            if(x==0) curve.startNewSubPath(px,y); else curve.lineTo(px,y);
        }
        g.setColour(cyan.withAlpha(0.14f)); g.strokePath(curve,juce::PathStrokeType(8.0f));
        juce::ColourGradient cg(cyan2,plot.getX(),plot.getCentreY(),violet,plot.getRight(),plot.getCentreY(),false);
        g.setGradientFill(cg); g.strokePath(curve,juce::PathStrokeType(2.2f));

        auto bottom=juce::Rectangle<float>(18.0f,(float)getHeight()-214.0f,(float)getWidth()-36.0f,194.0f);
        g.setColour(panel.withAlpha(0.72f)); g.fillRoundedRectangle(bottom,12.0f);
        g.setColour(border); g.drawRoundedRectangle(bottom,12.0f,1.0f);

        const float rms=juce::jlimit(0.0f,1.0f,engine.meterRms.load()*3.0f);
        auto meter=juce::Rectangle<float>((float)getWidth()-39.0f,244.0f,8.0f,juce::jmax(40.0f,(float)getHeight()-498.0f));
        g.setColour(bg0);g.fillRoundedRectangle(meter,3.0f);
        g.setColour(green);g.fillRoundedRectangle(meter.withTop(meter.getBottom()-meter.getHeight()*rms),3.0f);
    }

    void resized() override
    {
        brand.setBounds (24, 12, 280, 32);
        edition.setBounds (25, 42, 320, 20);

        auto top = juce::Rectangle<int> (getWidth()-660, 16, 642, 38);
        loadButton.setBounds (top.removeFromLeft(150)); top.removeFromLeft(7);
        playButton.setBounds (top.removeFromLeft(72)); top.removeFromLeft(7);
        stopButton.setBounds (top.removeFromLeft(72)); top.removeFromLeft(8);
        presetBox.setBounds (top);

        chainLabel.setBounds (26, 78, 140, 18);
        auto chainArea=juce::Rectangle<int>(26,108,getWidth()-52,94);
        const int gap=7;
        const int tileW=(chainArea.getWidth()-gap*11)/12;
        for(int i=0;i<12;++i) modules[(size_t)i]->setBounds(chainArea.getX()+i*(tileW+gap),chainArea.getY(),tileW,58);

        status.setBounds (28, getHeight()-208, getWidth()-56, 22);

        auto controlsArea=juce::Rectangle<int>(28,getHeight()-176,getWidth()-56,145);
        std::array<juce::Slider*,12> arr{&lowCut,&gate,&body,&presence,&air,&deess,&comp,&parallel,&sat,&doubler,&reverb,&width};
        const int cellW=controlsArea.getWidth()/12;
        for(int i=0;i<12;++i) arr[(size_t)i]->setBounds(controlsArea.getX()+i*cellW,controlsArea.getY(),cellW-5,controlsArea.getHeight());
    }

private:
    void setupKnob(juce::Slider& s,const juce::String& name,double min,double max,double value,const juce::String& suffix={})
    {
        s.setName(name);
        s.setTooltip(name);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,78,21);
        s.setRange(min,max,0.001);
        s.setValue(value);
        s.setTextValueSuffix(suffix);
        addAndMakeVisible(s);
    }

    void chooseFile()
    {
        chooser=std::make_unique<juce::FileChooser>("Open vocal WAV or MP3",juce::File{},"*.wav;*.mp3;*.aif;*.aiff");
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto file=fc.getResult();
                if(!file.existsAsFile()) return;
                std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
                if(reader)
                {
                    transport.stop(); transport.setSource(nullptr);
                    readerSource=std::make_unique<juce::AudioFormatReaderSource>(reader.release(),true);
                    transport.setSource(readerSource.get(),0,nullptr,readerSource->getAudioFormatReader()->sampleRate);
                    status.setText("Loaded: "+file.getFileName(),juce::dontSendNotification);
                }
            });
    }

    void applyPreset(int i)
    {
        switch(juce::jlimit(0,7,i))
        {
            case 0: settings={1.0f,-54.0f,82.0f,-1.2f,4.2f,4.8f,0.52f,0.64f,0.46f,0.28f,0.18f,0.12f,0.18f,1.08f,0.035f,0.045f,-0.7f};break;
            case 1: settings={1.8f,-52.0f,88.0f,-1.8f,5.3f,4.2f,0.58f,0.76f,0.52f,0.33f,0.31f,0.34f,0.16f,1.04f,0.025f,0.035f,-0.8f};break;
            case 2: settings={0.7f,-55.0f,65.0f,3.4f,1.8f,1.9f,0.42f,0.58f,0.55f,0.31f,0.23f,0.08f,0.12f,1.02f,0.03f,0.05f,-0.7f};break;
            case 3: settings={1.2f,-50.0f,72.0f,1.8f,0.8f,-0.7f,0.40f,0.67f,0.50f,0.36f,0.38f,0.28f,0.14f,1.06f,0.055f,0.07f,-0.9f};break;
            case 4: settings={0.8f,-58.0f,95.0f,-0.8f,3.6f,6.8f,0.62f,0.54f,0.42f,0.22f,0.12f,0.07f,0.17f,1.10f,0.04f,0.075f,-0.8f};break;
            case 5: settings={0.5f,-57.0f,88.0f,0.8f,2.4f,4.2f,0.68f,0.65f,0.56f,0.30f,0.20f,0.09f,0.13f,1.05f,0.03f,0.06f,-0.8f};break;
            case 6: settings={0.0f,-54.0f,110.0f,-1.6f,2.0f,2.5f,0.52f,0.60f,0.43f,0.24f,0.13f,0.08f,0.48f,1.48f,0.08f,0.14f,-1.0f};break;
            case 7: settings={0.0f,-52.0f,125.0f,-2.6f,3.4f,5.0f,0.58f,0.66f,0.40f,0.18f,0.19f,0.20f,0.32f,1.34f,0.24f,0.22f,-1.0f};break;
        }
        syncKnobs(); sync();
    }

    void syncKnobs()
    {
        lowCut.setValue(settings.lowCutHz,juce::dontSendNotification);
        gate.setValue(settings.gateDb,juce::dontSendNotification);
        body.setValue(settings.bodyDb,juce::dontSendNotification);
        presence.setValue(settings.presenceDb,juce::dontSendNotification);
        air.setValue(settings.airDb,juce::dontSendNotification);
        deess.setValue(settings.deEss,juce::dontSendNotification);
        comp.setValue(settings.comp,juce::dontSendNotification);
        parallel.setValue(settings.parallel,juce::dontSendNotification);
        sat.setValue(settings.saturation,juce::dontSendNotification);
        doubler.setValue(settings.doubler,juce::dontSendNotification);
        reverb.setValue(settings.reverb,juce::dontSendNotification);
        width.setValue(settings.width,juce::dontSendNotification);
    }

    void sync(){engine.setSettings(settings);repaint();}
    void timerCallback() override { repaint(); }

    PremiumLookAndFeel laf;
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::FileChooser> chooser;
    VocalEngine engine;
    VocalSettings settings;

    juce::Label brand,edition,chainLabel,status;
    juce::TextButton loadButton,playButton,stopButton;
    juce::ComboBox presetBox;
    std::array<std::unique_ptr<ModuleTile>,12> modules;
    juce::Slider lowCut,gate,body,presence,air,deess,comp,parallel,sat,doubler,reverb,width;
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow():DocumentWindow("VocalForge ONE v2.1 PRO",bg0,DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setResizable(true,true);
        setResizeLimits(980,640,1680,1080);
        setContentOwned(new StandaloneComponent(),true);
        centreWithSize(1280,790);
        setVisible(true);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class VocalForgeApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "VocalForge ONE"; }
    const juce::String getApplicationVersion() override { return "2.1.0"; }
    void initialise(const juce::String&) override { window=std::make_unique<MainWindow>(); }
    void shutdown() override { window.reset(); }
private:
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION(VocalForgeApplication)
