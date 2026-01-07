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

ChannelEQComponent::ChannelEQComponent(RoomMultiEQAudioProcessor& p)
    : processor(p), channelIndex(0)
{
    table.setModel(this);
    table.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
    table.setRowHeight(24);

    auto& header = table.getHeader();
    header.addColumn("#", 1, 30, 30, 30);
    header.addColumn("On", 2, 32, 32, 32);
    header.addColumn("Type", 3, 55, 55, 70);
    header.addColumn("Freq", 4, 85, 75, 110);
    header.addColumn("Gain", 5, 70, 60, 90);
    header.addColumn("Q", 6, 55, 45, 75);

    addAndMakeVisible(table);
    updateVisibleBands();
}

ChannelEQComponent::~ChannelEQComponent()
{
}

void ChannelEQComponent::setChannelIndex(int index)
{
    channelIndex = index;
    updateVisibleBands();
}

void ChannelEQComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff353535));
}

void ChannelEQComponent::resized()
{
    table.setBounds(getLocalBounds());
}

int ChannelEQComponent::getNumRows()
{
    return static_cast<int>(visibleBands.size());
}

bool ChannelEQComponent::isBandActive(int bandIndex) const
{
    auto& apvts = processor.getAPVTS();

    constexpr float freqDefault = 1000.0f;
    constexpr float gainDefault = 0.0f;
    constexpr float qDefault = 1.0f;

    auto* freqParam = apvts.getParameter(RoomMultiEQAudioProcessor::getParamID(channelIndex, bandIndex, "freq"));
    auto* gainParam = apvts.getParameter(RoomMultiEQAudioProcessor::getParamID(channelIndex, bandIndex, "gain"));
    auto* qParam = apvts.getParameter(RoomMultiEQAudioProcessor::getParamID(channelIndex, bandIndex, "q"));

    if (!freqParam || !gainParam || !qParam)
        return true;

    float freqNormDefault = freqParam->convertTo0to1(freqDefault);
    float gainNormDefault = gainParam->convertTo0to1(gainDefault);
    float qNormDefault = qParam->convertTo0to1(qDefault);

    float freqNorm = freqParam->getValue();
    float gainNorm = gainParam->getValue();
    float qNorm = qParam->getValue();

    if (std::abs(freqNorm - freqNormDefault) > 0.001f) return true;
    if (std::abs(gainNorm - gainNormDefault) > 0.001f) return true;
    if (std::abs(qNorm - qNormDefault) > 0.001f) return true;

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
    delete existing;

    if (columnId == 1)
        return nullptr;

    int bandIndex = getBandIndexForRow(rowNumber);
    auto& apvts = processor.getAPVTS();

    switch (columnId)
    {
        case 2:
            return new BypassButtonCell(apvts, RoomMultiEQAudioProcessor::getParamID(channelIndex, bandIndex, "bypass"));
        case 3:
            return new TypeComboCell(apvts, RoomMultiEQAudioProcessor::getParamID(channelIndex, bandIndex, "type"));
        case 4:
            return new SliderCell(apvts, RoomMultiEQAudioProcessor::getParamID(channelIndex, bandIndex, "freq"), 70);
        case 5:
            return new SliderCell(apvts, RoomMultiEQAudioProcessor::getParamID(channelIndex, bandIndex, "gain"), 50);
        case 6:
            return new SliderCell(apvts, RoomMultiEQAudioProcessor::getParamID(channelIndex, bandIndex, "q"), 40);
        default:
            return nullptr;
    }
}

//==============================================================================
// RoomMultiEQAudioProcessorEditor
//==============================================================================

RoomMultiEQAudioProcessorEditor::RoomMultiEQAudioProcessorEditor(RoomMultiEQAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      channelTable(p)
{
    // Master bypass
    masterBypassButton.setButtonText("Master Bypass");
    masterBypassButton.setClickingTogglesState(true);
    addAndMakeVisible(masterBypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), "master_bypass", masterBypassButton);

    // Multi-channel spectrum analyzer
    spectrumAnalyzer = std::make_unique<MultiChannelSpectrumAnalyzer>(p);
    addAndMakeVisible(*spectrumAnalyzer);
    spectrumAnalyzer->startAnalysis();

    // Channel selector
    channelSelector.onChange = [this]() { onChannelSelected(); };
    addAndMakeVisible(channelSelector);

    // Import/Clear buttons
    importButton.setButtonText("Import");
    importButton.onClick = [this]() { importFilterFile(); };
    addAndMakeVisible(importButton);

    clearButton.setButtonText("Clear");
    clearButton.onClick = [this]() { clearAll(); };
    addAndMakeVisible(clearButton);

    // Channel table
    addAndMakeVisible(channelTable);

    // Initialize
    updateChannelSelector();
    updateVisibilityToggles();

    setSize(700, 600);
    setResizable(true, true);
    setResizeLimits(600, 500, 1200, 900);
}

RoomMultiEQAudioProcessorEditor::~RoomMultiEQAudioProcessorEditor()
{
}

void RoomMultiEQAudioProcessorEditor::updateChannelSelector()
{
    channelSelector.clear();
    const auto& names = audioProcessor.getChannelNames();
    int numChannels = audioProcessor.getNumChannels();

    for (int i = 0; i < numChannels; ++i)
    {
        channelSelector.addItem(names[static_cast<size_t>(i)], i + 1);
    }

    if (numChannels > 0)
        channelSelector.setSelectedId(1);
}

void RoomMultiEQAudioProcessorEditor::onChannelSelected()
{
    int selectedId = channelSelector.getSelectedId();
    if (selectedId > 0)
    {
        channelTable.setChannelIndex(selectedId - 1);
    }
}

void RoomMultiEQAudioProcessorEditor::updateVisibilityToggles()
{
    // Clear existing toggles and sparklines
    for (auto& toggle : visibilityToggles)
        removeChildComponent(toggle.get());
    visibilityToggles.clear();

    for (auto& sparkline : sparklines)
        removeChildComponent(sparkline.get());
    sparklines.clear();

    int numChannels = audioProcessor.getNumChannels();
    const auto& names = audioProcessor.getChannelNames();

    for (int i = 0; i < numChannels; ++i)
    {
        auto toggle = std::make_unique<juce::ToggleButton>(names[static_cast<size_t>(i)]);
        toggle->setToggleState(true, juce::dontSendNotification);
        toggle->setColour(juce::ToggleButton::textColourId, ChannelColors::getChannelColor(i));
        toggle->setColour(juce::ToggleButton::tickColourId, ChannelColors::getChannelColor(i));

        int channelIndex = i;
        toggle->onClick = [this, channelIndex]()
        {
            bool visible = visibilityToggles[static_cast<size_t>(channelIndex)]->getToggleState();
            spectrumAnalyzer->setChannelVisible(channelIndex, visible);
        };

        addAndMakeVisible(*toggle);
        visibilityToggles.push_back(std::move(toggle));

        // Add sparkline for this channel
        auto sparkline = std::make_unique<FilterSparkline>(audioProcessor, i);
        addAndMakeVisible(*sparkline);
        sparklines.push_back(std::move(sparkline));
    }

    resized();
}

void RoomMultiEQAudioProcessorEditor::importFilterFile()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select Filter File",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.txt");

    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                audioProcessor.loadFilterFile(channelTable.getChannelIndex(), file);
                channelTable.updateVisibleBands();
            }
        });
}

void RoomMultiEQAudioProcessorEditor::clearAll()
{
    int ch = channelTable.getChannelIndex();
    for (int b = 0; b < NUM_EQ_BANDS; ++b)
        audioProcessor.resetBandToDefaults(ch, b);
    channelTable.updateVisibleBands();
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

    // Header
    auto headerArea = bounds.removeFromTop(45);
    headerArea.removeFromLeft(200); // Space for title
    masterBypassButton.setBounds(headerArea.removeFromRight(130).reduced(5));

    // Spectrum analyzer (40% of remaining height)
    int spectrumHeight = static_cast<int>(bounds.getHeight() * 0.4f);
    spectrumAnalyzer->setBounds(bounds.removeFromTop(spectrumHeight));

    // Visibility toggles row with sparklines
    auto toggleArea = bounds.removeFromTop(30);
    toggleArea.removeFromLeft(10);
    for (size_t i = 0; i < visibilityToggles.size(); ++i)
    {
        visibilityToggles[i]->setBounds(toggleArea.removeFromLeft(40));
        if (i < sparklines.size())
        {
            sparklines[i]->setBounds(toggleArea.removeFromLeft(50).reduced(2, 4));
        }
        toggleArea.removeFromLeft(5);  // Gap between channels
    }

    // Channel selector row
    auto selectorArea = bounds.removeFromTop(35);
    selectorArea.removeFromLeft(10);
    channelSelector.setBounds(selectorArea.removeFromLeft(150).reduced(2));
    importButton.setBounds(selectorArea.removeFromLeft(80).reduced(2));
    clearButton.setBounds(selectorArea.removeFromLeft(80).reduced(2));

    // Table
    bounds.removeFromTop(5);
    channelTable.setBounds(bounds.reduced(10, 0));
}
