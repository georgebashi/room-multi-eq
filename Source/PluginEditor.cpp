#include "PluginEditor.h"

//==============================================================================
// Custom cell components
//==============================================================================

class SliderCell : public juce::Component
{
public:
    SliderCell(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, int textBoxWidth)
    {
        slider.setSliderStyle(juce::Slider::LinearBar);
        slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, textBoxWidth, 20);
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        addAndMakeVisible(slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, paramID, slider);
    }

    void resized() override { slider.setBounds(getLocalBounds()); }

private:
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class TypeComboCell : public juce::Component
{
public:
    TypeComboCell(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
    {
        combo.addItem("PK", 1);
        combo.addItem("LS", 2);
        combo.addItem("HS", 3);
        addAndMakeVisible(combo);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, paramID, combo);
    }

    void resized() override { combo.setBounds(getLocalBounds()); }

private:
    juce::ComboBox combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class BypassButtonCell : public juce::Component,
                         public juce::AudioProcessorValueTreeState::Listener
{
public:
    BypassButtonCell(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
        : apvtsRef(apvts)
    {
        // Display shows filter state (ON/OFF), not bypass state
        // Toggle ON (lit) = filter enabled = bypass false
        button.setButtonText("");  // No text, just checkbox
        button.setClickingTogglesState(true);
        button.onClick = [this]() {
            // Invert: button ON means bypass OFF (filter enabled)
            bool filterEnabled = button.getToggleState();
            if (auto* param = apvtsRef.getParameter(parameterID))
                param->setValueNotifyingHost(filterEnabled ? 0.0f : 1.0f);
        };
        addAndMakeVisible(button);

        setParameterID(paramID);
    }

    ~BypassButtonCell() override
    {
        if (parameterID.isNotEmpty())
            apvtsRef.removeParameterListener(parameterID, this);
    }

    void setParameterID(const juce::String& paramID)
    {
        if (parameterID.isNotEmpty())
            apvtsRef.removeParameterListener(parameterID, this);

        parameterID = paramID;
        apvtsRef.addParameterListener(parameterID, this);
        updateButtonState();
    }

    void resized() override
    {
        // Center the checkbox in the cell
        auto bounds = getLocalBounds();
        int size = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 4;
        button.setBounds(bounds.withSizeKeepingCentre(size + 12, size));  // +12 for click area
    }

private:
    void parameterChanged(const juce::String&, float) override
    {
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<BypassButtonCell>(this)]() {
            if (safeThis != nullptr)
                safeThis->updateButtonState();
        });
    }

    void updateButtonState()
    {
        if (auto* param = apvtsRef.getRawParameterValue(parameterID))
        {
            bool bypassed = *param > 0.5f;
            button.setToggleState(!bypassed, juce::dontSendNotification);
        }
    }

    juce::String parameterID;
    juce::AudioProcessorValueTreeState& apvtsRef;
    juce::ToggleButton button;
};

//==============================================================================
// ChannelEQComponent
//==============================================================================

ChannelEQComponent::ChannelEQComponent(RoomMultiEQAudioProcessor& p, bool isLeft, SpectrumDataCollector& collector, double& sr)
    : processor(p), isLeftChannel(isLeft), sampleRateRef(sr)
{
    channelPrefix = isLeft ? "left" : "right";

    // Create spectrum analyzer
    spectrumAnalyzer = std::make_unique<SpectrumAnalyzer>(
        collector,
        isLeft ? p.getLeftChannel() : p.getRightChannel(),
        sampleRateRef
    );
    addAndMakeVisible(*spectrumAnalyzer);
    spectrumAnalyzer->startAnalysis();

    importButton.setButtonText("Import");
    importButton.onClick = [this]() { importFilterFile(); };
    addAndMakeVisible(importButton);

    clearButton.setButtonText("Clear All");
    clearButton.onClick = [this]() { clearAll(); };
    addAndMakeVisible(clearButton);

    table.setModel(this);
    table.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
    table.setRowHeight(24);

    auto& header = table.getHeader();
    header.addColumn("#", 1, 30, 30, 30);
    header.addColumn("On", 2, 32, 32, 32);      // Just checkbox, no text
    header.addColumn("Type", 3, 55, 55, 70);    // Wide enough for "LS" dropdown
    header.addColumn("Freq", 4, 85, 75, 110);
    header.addColumn("Gain", 5, 70, 60, 90);
    header.addColumn("Q", 6, 55, 45, 75);

    addAndMakeVisible(table);

    updateVisibleBands();
}

ChannelEQComponent::~ChannelEQComponent()
{
}

void ChannelEQComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff353535));

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText(isLeftChannel ? "LEFT CHANNEL" : "RIGHT CHANNEL",
               10, 5, getWidth() - 20, 25, juce::Justification::centred);
}

void ChannelEQComponent::resized()
{
    auto bounds = getLocalBounds().reduced(5);

    // Title area
    bounds.removeFromTop(30);
    bounds.removeFromTop(5);

    // Buttons at bottom
    auto buttonArea = bounds.removeFromBottom(30);
    importButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth() / 2).reduced(2));
    clearButton.setBounds(buttonArea.reduced(2));

    bounds.removeFromBottom(5);

    // Check if table should be visible
    auto* editor = findParentComponentOfClass<RoomMultiEQAudioProcessorEditor>();
    bool showTable = editor != nullptr && editor->areTablesVisible();

    if (showTable)
    {
        // Split: 40% spectrum, 60% table
        int spectrumHeight = static_cast<int>(bounds.getHeight() * 0.4f);
        spectrumAnalyzer->setBounds(bounds.removeFromTop(spectrumHeight));
        bounds.removeFromTop(5);
        table.setBounds(bounds);
        table.setVisible(true);
    }
    else
    {
        // Full spectrum
        spectrumAnalyzer->setBounds(bounds);
        table.setVisible(false);
    }
}

void ChannelEQComponent::updateLayout()
{
    resized();
}

int ChannelEQComponent::getNumRows()
{
    return static_cast<int>(visibleBands.size());
}

bool ChannelEQComponent::isBandActive(int bandIndex) const
{
    // A band is "active" (should be shown) if any parameter differs from defaults.
    // This means it was specified in an imported file.
    // We don't hide based on bypass state - user can toggle filters on/off and they stay visible.
    auto& apvts = processor.getAPVTS();

    constexpr float freqDefault = 1000.0f;
    constexpr float gainDefault = 0.0f;
    constexpr float qDefault = 1.0f;

    auto* freqParam = apvts.getParameter(RoomMultiEQAudioProcessor::getParamID(channelPrefix, bandIndex, "freq"));
    auto* gainParam = apvts.getParameter(RoomMultiEQAudioProcessor::getParamID(channelPrefix, bandIndex, "gain"));
    auto* qParam = apvts.getParameter(RoomMultiEQAudioProcessor::getParamID(channelPrefix, bandIndex, "q"));

    if (!freqParam || !gainParam || !qParam)
        return true;  // Show if we can't check

    // Note: These are approximations - the actual ranges use skew factors
    // For freq and Q, just check if normalized value is near default normalized value
    float freqNormDefault = freqParam->convertTo0to1(freqDefault);
    float gainNormDefault = gainParam->convertTo0to1(gainDefault);
    float qNormDefault = qParam->convertTo0to1(qDefault);

    float freqNorm = freqParam->getValue();
    float gainNorm = gainParam->getValue();
    float qNorm = qParam->getValue();

    if (std::abs(freqNorm - freqNormDefault) > 0.001f)
        return true;

    if (std::abs(gainNorm - gainNormDefault) > 0.001f)
        return true;

    if (std::abs(qNorm - qNormDefault) > 0.001f)
        return true;

    return false;
}

int ChannelEQComponent::getBandIndexForRow(int row) const
{
    if (row >= 0 && row < static_cast<int>(visibleBands.size()))
        return visibleBands[static_cast<size_t>(row)];
    return 0;
}

void ChannelEQComponent::updateVisibleBands()
{
    visibleBands.clear();

    for (int b = 0; b < NUM_EQ_BANDS; ++b)
    {
        if (isBandActive(b))
            visibleBands.push_back(b);
    }

    table.updateContent();
    table.repaint();
}

void ChannelEQComponent::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool)
{
    juce::ignoreUnused(width, height);
    g.fillAll(rowNumber % 2 == 0 ? juce::Colour(0xff2a2a2a) : juce::Colour(0xff323232));
}

void ChannelEQComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool)
{
    g.setColour(juce::Colours::white);

    if (columnId == 1)
    {
        int bandIndex = getBandIndexForRow(rowNumber);
        g.drawText(juce::String(bandIndex + 1), 2, 0, width - 4, height, juce::Justification::centred);
    }
}

juce::Component* ChannelEQComponent::refreshComponentForCell(int rowNumber, int columnId, bool, juce::Component* existing)
{
    // Delete existing cell if any - we always create fresh cells to avoid
    // stale parameter bindings when cells are reused
    delete existing;

    if (columnId == 1)
        return nullptr;

    int bandIndex = getBandIndexForRow(rowNumber);
    auto& apvts = processor.getAPVTS();

    switch (columnId)
    {
        case 2: // Bypass
            return new BypassButtonCell(apvts, RoomMultiEQAudioProcessor::getParamID(channelPrefix, bandIndex, "bypass"));
        case 3: // Type
            return new TypeComboCell(apvts, RoomMultiEQAudioProcessor::getParamID(channelPrefix, bandIndex, "type"));
        case 4: // Freq
            return new SliderCell(apvts, RoomMultiEQAudioProcessor::getParamID(channelPrefix, bandIndex, "freq"), 70);
        case 5: // Gain
            return new SliderCell(apvts, RoomMultiEQAudioProcessor::getParamID(channelPrefix, bandIndex, "gain"), 50);
        case 6: // Q
            return new SliderCell(apvts, RoomMultiEQAudioProcessor::getParamID(channelPrefix, bandIndex, "q"), 40);
        default:
            return nullptr;
    }
}

void ChannelEQComponent::importFilterFile()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select Filter File",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.txt");

    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync(chooserFlags, [this, chooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            processor.loadFilterFile(isLeftChannel, file);
            updateVisibleBands();
        }
    });
}

void ChannelEQComponent::clearAll()
{
    for (int b = 0; b < NUM_EQ_BANDS; ++b)
        processor.resetBandToDefaults(channelPrefix, b);

    updateVisibleBands();
}

//==============================================================================
// RoomMultiEQAudioProcessorEditor
//==============================================================================

RoomMultiEQAudioProcessorEditor::RoomMultiEQAudioProcessorEditor(RoomMultiEQAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      leftChannelComponent(p, true, p.getLeftSpectrumCollector(), sampleRate),
      rightChannelComponent(p, false, p.getRightSpectrumCollector(), sampleRate)
{
    sampleRate = p.getCurrentSampleRate();

    // Show Tables button
    showTablesButton.setButtonText("Show Tables");
    showTablesButton.onClick = [this]()
    {
        tablesVisible = !tablesVisible;
        showTablesButton.setButtonText(tablesVisible ? "Hide Tables" : "Show Tables");
        leftChannelComponent.updateLayout();
        rightChannelComponent.updateLayout();
    };
    addAndMakeVisible(showTablesButton);

    masterBypassButton.setButtonText("Master Bypass");
    masterBypassButton.setClickingTogglesState(true);
    addAndMakeVisible(masterBypassButton);

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), "master_bypass", masterBypassButton);

    addAndMakeVisible(leftChannelComponent);
    addAndMakeVisible(rightChannelComponent);

    setSize(700, 550);
    setResizable(true, true);
    setResizeLimits(600, 450, 1200, 900);
}

RoomMultiEQAudioProcessorEditor::~RoomMultiEQAudioProcessorEditor()
{
}

void RoomMultiEQAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(20.0f).withStyle("Bold"));
    g.drawText("Room Multi EQ", 10, 5, getWidth() - 150, 35, juce::Justification::centredLeft);
}

void RoomMultiEQAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Header area
    auto headerArea = bounds.removeFromTop(45);
    headerArea.removeFromLeft(10);

    // Title on left (handled in paint())

    // Buttons on right
    auto buttonArea = headerArea.removeFromRight(240);
    masterBypassButton.setBounds(buttonArea.removeFromRight(130).reduced(5));
    showTablesButton.setBounds(buttonArea.removeFromRight(100).reduced(5));

    // Split remaining for channels
    auto halfWidth = bounds.getWidth() / 2;
    leftChannelComponent.setBounds(bounds.removeFromLeft(halfWidth));
    rightChannelComponent.setBounds(bounds);
}
