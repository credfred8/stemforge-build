#include "PluginEditor.h"
#include <algorithm>

namespace
{
    const juce::Colour bg      = juce::Colour::fromRGB(15, 17, 20);
    const juce::Colour panel   = juce::Colour::fromRGB(25, 28, 33);
    const juce::Colour text    = juce::Colour::fromRGB(238, 240, 244);
    const juce::Colour muted   = juce::Colour::fromRGB(151, 158, 168);
    const juce::Colour accent  = juce::Colour::fromRGB(105, 178, 255);
    const juce::Colour success = juce::Colour::fromRGB(123, 214, 160);

    juce::String ru(const wchar_t* value)
    {
        return juce::String(value);
    }
}

StemForgeAudioProcessorEditor::ForgeLookAndFeel::ForgeLookAndFeel()
{
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::ToggleButton::textColourId, text);
    setColour(juce::ProgressBar::backgroundColourId, juce::Colour::fromRGB(40, 44, 50));
    setColour(juce::ProgressBar::foregroundColourId, accent);
}

void StemForgeAudioProcessorEditor::ForgeLookAndFeel::drawButtonBackground(
    juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    auto c = accent.withAlpha(b.isEnabled() ? (down ? 0.72f : over ? 0.58f : 0.42f) : 0.15f);
    g.setColour(c);
    g.fillRoundedRectangle(r, 10.0f);
    g.setColour(accent.withAlpha(b.isEnabled() ? 0.75f : 0.2f));
    g.drawRoundedRectangle(r, 10.0f, 1.0f);
}

void StemForgeAudioProcessorEditor::ForgeLookAndFeel::drawToggleButton(
    juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    const auto on = b.getToggleState();
    g.setColour(on ? accent.withAlpha(over ? 0.46f : 0.34f)
                   : juce::Colour::fromRGB(37, 41, 47).withAlpha(over ? 1.0f : 0.86f));
    g.fillRoundedRectangle(r, 9.0f);
    g.setColour(on ? accent : juce::Colour::fromRGB(70, 75, 84));
    g.drawRoundedRectangle(r, 9.0f, 1.0f);

    g.setColour(text);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawFittedText((on ? "[x]  " : "") + b.getButtonText(),
                     b.getLocalBounds().reduced(10, 2), juce::Justification::centred, 1);
}

StemForgeAudioProcessorEditor::StemForgeAudioProcessorEditor(StemForgeAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(760, 600, 1200, 900);
    setSize(900, 690);

    title.setText("STEMFORGE", juce::dontSendNotification);
    title.setColour(juce::Label::textColourId, text);
    title.setFont(juce::FontOptions(30.0f, juce::Font::bold));

    subtitle.setText("AI STEM SEPARATOR | LOCAL | OFFLINE | 6 STEMS", juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, accent);
    subtitle.setFont(juce::FontOptions(12.0f, juce::Font::bold));

    inputLabel.setColour(juce::Label::textColourId, text);
    inputLabel.setFont(juce::FontOptions(15.0f));
    outputLabel.setColour(juce::Label::textColourId, muted);
    outputLabel.setFont(juce::FontOptions(12.5f));
    statusLabel.setColour(juce::Label::textColourId, muted);
    statusLabel.setFont(juce::FontOptions(13.0f));
    hintLabel.setColour(juce::Label::textColourId, muted);
    hintLabel.setFont(juce::FontOptions(12.0f));

    loadButton.setButtonText(ru(L"\u0417\u0430\u0433\u0440\u0443\u0437\u0438\u0442\u044c \u0430\u0443\u0434\u0438\u043e"));
    outputButton.setButtonText(ru(L"\u041f\u0430\u043f\u043a\u0430 \u044d\u043a\u0441\u043f\u043e\u0440\u0442\u0430"));
    separateButton.setButtonText(ru(L"\u0420\u0410\u0417\u0414\u0415\u041b\u0418\u0422\u042c HQ"));
    openFolderButton.setButtonText(ru(L"\u041e\u0442\u043a\u0440\u044b\u0442\u044c \u0440\u0435\u0437\u0443\u043b\u044c\u0442\u0430\u0442"));
    cleanButton.setButtonText(ru(L"Clean Sample: \u0443\u0431\u0440\u0430\u0442\u044c \u043e\u0442\u043c\u0435\u0447\u0435\u043d\u043d\u044b\u0435 \u0441\u0442\u0435\u043c\u044b"));

    hintLabel.setText(
        ru(L"\u041e\u0442\u043c\u0435\u0447\u0435\u043d\u043d\u044b\u0435 \u0441\u0442\u0435\u043c\u044b \u044d\u043a\u0441\u043f\u043e\u0440\u0442\u0438\u0440\u0443\u044e\u0442\u0441\u044f \u043e\u0442\u0434\u0435\u043b\u044c\u043d\u043e. Clean Sample = \u0441\u0443\u043c\u043c\u0430 \u043e\u0441\u0442\u0430\u043b\u044c\u043d\u044b\u0445 \u0441\u0442\u0435\u043c\u043e\u0432."),
        juce::dontSendNotification);

    const std::array<juce::String, stemforge::stemCount> labels {
        ru(L"\u0423\u0434\u0430\u0440\u043d\u044b\u0435"),
        ru(L"\u0411\u0430\u0441\u0441"),
        "Other / Melody",
        ru(L"\u0412\u043e\u043a\u0430\u043b"),
        ru(L"\u0413\u0438\u0442\u0430\u0440\u0430"),
        ru(L"\u041f\u0438\u0430\u043d\u0438\u043d\u043e")
    };

    for (int i = 0; i < stemforge::stemCount; ++i)
    {
        stemButtons[static_cast<size_t>(i)].setButtonText(labels[static_cast<size_t>(i)]);
        addAndMakeVisible(stemButtons[static_cast<size_t>(i)]);
    }

    cleanButton.setToggleState(true, juce::dontSendNotification);

    loadButton.onClick = [this] { chooseInput(); };
    outputButton.onClick = [this] { chooseOutputFolder(); };
    separateButton.onClick = [this] { startSeparation(); };
    openFolderButton.onClick = [this]
    {
        if (latestOutputDirectory.isDirectory())
            latestOutputDirectory.startAsProcess();
    };
    openFolderButton.setEnabled(false);

    for (auto* c : std::initializer_list<juce::Component*> {
             &title, &subtitle, &inputLabel, &outputLabel, &statusLabel, &hintLabel,
             &loadButton, &outputButton, &separateButton, &openFolderButton,
             &cleanButton, &progressBar })
        addAndMakeVisible(c);

    if (processor.state.hasProperty("lastInput"))
    {
        const juce::File last(processor.state["lastInput"].toString());
        if (last.existsAsFile())
            inputFile = last;
    }

    if (processor.state.hasProperty("outputDir"))
    {
        const juce::File lastOut(processor.state["outputDir"].toString());
        if (lastOut.isDirectory())
            outputDirectory = lastOut;
    }

    updateFileLabel();
    statusLabel.setText(engine.getStatus(), juce::dontSendNotification);
    startTimerHz(12);
}

StemForgeAudioProcessorEditor::~StemForgeAudioProcessorEditor()
{
    stopTimer();
    engine.cancel();
    setLookAndFeel(nullptr);
}

void StemForgeAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(bg);
    auto r = getLocalBounds().toFloat().reduced(18.0f);
    auto body = r.withTrimmedTop(86.0f);
    g.setColour(panel);
    g.fillRoundedRectangle(body, 16.0f);
    g.setColour(juce::Colour::fromRGB(52, 57, 65));
    g.drawRoundedRectangle(body, 16.0f, 1.0f);

    auto drop = juce::Rectangle<float>(30.0f, 120.0f, getWidth() - 60.0f, 104.0f);
    g.setColour(juce::Colour::fromRGB(31, 35, 41));
    g.fillRoundedRectangle(drop, 12.0f);
    g.setColour(accent.withAlpha(0.35f));
    g.drawRoundedRectangle(drop, 12.0f, 1.0f);
}

void StemForgeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(28);
    auto header = area.removeFromTop(72);
    title.setBounds(header.removeFromTop(40));
    subtitle.setBounds(header.removeFromTop(24));

    area.removeFromTop(18);
    auto fileBox = area.removeFromTop(104).reduced(14, 10);
    auto fileText = fileBox.removeFromLeft(std::max(300, fileBox.getWidth() - 320));
    inputLabel.setBounds(fileText.removeFromTop(40));
    outputLabel.setBounds(fileText.removeFromTop(34));
    auto fileButtons = fileBox.reduced(4, 4);
    loadButton.setBounds(fileButtons.removeFromTop(38));
    fileButtons.removeFromTop(6);
    outputButton.setBounds(fileButtons.removeFromTop(38));

    area.removeFromTop(24);
    const int gap = 10;
    const int cellW = (area.getWidth() - gap * 2) / 3;
    const int cellH = 52;

    for (int i = 0; i < stemforge::stemCount; ++i)
    {
        const int row = i / 3;
        const int col = i % 3;
        stemButtons[static_cast<size_t>(i)].setBounds(area.getX() + col * (cellW + gap),
                                                      area.getY() + row * (cellH + gap),
                                                      cellW, cellH);
    }

    area.removeFromTop(cellH * 2 + gap + 18);
    cleanButton.setBounds(area.removeFromTop(48));
    hintLabel.setBounds(area.removeFromTop(42));

    area.removeFromTop(8);
    progressBar.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);
    statusLabel.setBounds(area.removeFromTop(36));
    area.removeFromTop(8);

    auto actions = area.removeFromTop(58);
    separateButton.setBounds(actions.removeFromLeft((actions.getWidth() * 2) / 3).reduced(0, 2));
    actions.removeFromLeft(10);
    openFolderButton.setBounds(actions.reduced(0, 2));
}

bool StemForgeAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (files.isEmpty())
        return false;

    const auto ext = juce::File(files[0]).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".aiff"
        || ext == ".aif" || ext == ".ogg";
}

void StemForgeAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    if (! files.isEmpty())
        setInputFile(juce::File(files[0]));
}

void StemForgeAudioProcessorEditor::setInputFile(const juce::File& file)
{
    inputFile = file;
    processor.state.setProperty("lastInput", file.getFullPathName(), nullptr);
    updateFileLabel();
}

void StemForgeAudioProcessorEditor::updateFileLabel()
{
    if (inputFile.existsAsFile())
    {
        inputLabel.setText(inputFile.getFileName(), juce::dontSendNotification);
        outputLabel.setText(outputDirectory.isDirectory()
                                ? ru(L"\u042d\u043a\u0441\u043f\u043e\u0440\u0442: ") + outputDirectory.getFullPathName()
                                : ru(L"\u042d\u043a\u0441\u043f\u043e\u0440\u0442: \u0440\u044f\u0434\u043e\u043c \u0441 \u0438\u0441\u0445\u043e\u0434\u043d\u0438\u043a\u043e\u043c / <\u0438\u043c\u044f>_StemForge"),
                            juce::dontSendNotification);
    }
    else
    {
        inputLabel.setText(ru(L"\u041f\u0435\u0440\u0435\u0442\u0430\u0449\u0438 \u0441\u044e\u0434\u0430 \u0442\u0440\u0435\u043a \u0438\u043b\u0438 \u0441\u0435\u043c\u043f\u043b"), juce::dontSendNotification);
        outputLabel.setText("WAV | MP3 | FLAC | AIFF | OGG", juce::dontSendNotification);
    }
}

void StemForgeAudioProcessorEditor::chooseInput()
{
    chooser = std::make_unique<juce::FileChooser>(
        ru(L"\u0412\u044b\u0431\u0435\u0440\u0438 \u0442\u0440\u0435\u043a \u0438\u043b\u0438 \u0441\u0435\u043c\u043f\u043b"),
        inputFile.getParentDirectory(),
        "*.wav;*.mp3;*.flac;*.aiff;*.aif;*.ogg");

    const int chooserFlags = juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        const auto result = fc.getResult();
        if (result.existsAsFile())
            setInputFile(result);
    });
}

void StemForgeAudioProcessorEditor::chooseOutputFolder()
{
    chooser = std::make_unique<juce::FileChooser>(
        ru(L"\u041a\u0443\u0434\u0430 \u0441\u043e\u0445\u0440\u0430\u043d\u044f\u0442\u044c \u0441\u0442\u0435\u043c\u044b?"),
        outputDirectory.isDirectory() ? outputDirectory : inputFile.getParentDirectory());

    const int chooserFlags = juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectDirectories;

    chooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        const auto result = fc.getResult();
        if (result.isDirectory())
        {
            outputDirectory = result;
            processor.state.setProperty("outputDir", result.getFullPathName(), nullptr);
            updateFileLabel();
        }
    });
}

stemforge::SeparationOptions StemForgeAudioProcessorEditor::currentOptions() const
{
    stemforge::SeparationOptions options;
    for (int i = 0; i < stemforge::stemCount; ++i)
        options.exportStem[static_cast<size_t>(i)] = stemButtons[static_cast<size_t>(i)].getToggleState();

    options.exportCleanSample = cleanButton.getToggleState();
    options.outputDirectory = outputDirectory;
    return options;
}

void StemForgeAudioProcessorEditor::startSeparation()
{
    if (! inputFile.existsAsFile())
    {
        statusLabel.setText(ru(L"\u0421\u043d\u0430\u0447\u0430\u043b\u0430 \u0437\u0430\u0433\u0440\u0443\u0437\u0438 \u0430\u0443\u0434\u0438\u043e\u0444\u0430\u0439\u043b."), juce::dontSendNotification);
        return;
    }

    auto options = currentOptions();
    const bool anyStem = std::any_of(options.exportStem.begin(), options.exportStem.end(), [](bool v) { return v; });

    if (! anyStem && ! options.exportCleanSample)
    {
        statusLabel.setText(ru(L"\u0412\u044b\u0431\u0435\u0440\u0438 \u0445\u043e\u0442\u044f \u0431\u044b \u043e\u0434\u0438\u043d \u0441\u0442\u0435\u043c \u0438\u043b\u0438 Clean Sample."), juce::dontSendNotification);
        return;
    }

    if (engine.start(inputFile, options))
    {
        separateButton.setEnabled(false);
        loadButton.setEnabled(false);
        outputButton.setEnabled(false);
        openFolderButton.setEnabled(false);
    }
}

void StemForgeAudioProcessorEditor::timerCallback()
{
    progressValue = engine.getProgress();
    statusLabel.setText(engine.getStatus(), juce::dontSendNotification);

    const bool running = engine.isBusy();
    separateButton.setEnabled(! running);
    loadButton.setEnabled(! running);
    outputButton.setEnabled(! running);

    if (! running)
    {
        const auto files = engine.getLastOutputs();
        if (! files.empty())
        {
            latestOutputDirectory = files.front().getParentDirectory();
            openFolderButton.setEnabled(latestOutputDirectory.isDirectory());
            statusLabel.setColour(juce::Label::textColourId, success);
        }
        else
        {
            statusLabel.setColour(juce::Label::textColourId, muted);
        }
    }
}
