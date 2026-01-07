#include "PluginEditor.h"

//==============================================================================
// Custom cell components for ChannelSummaryTable
//==============================================================================

// Import button OR filename with clear button
class ImportCell : public juce::Component
{
public:
    ImportCell(RoomMultiEQAudioProcessor& p, int channel, ChannelSummaryTable& table)
        : processor(p), channelIndex(channel), parentTable(table)
    {
        importButton.setButtonText("Import");
        importButton.onClick = [this]() { importFile(); };
        addAndMakeVisible(importButton);

        clearButton.setButtonText(juce::CharPointer_UTF8("\xc3\x97"));  // × symbol
        clearButton.onClick = [this]() { clearChannel(); };
        addChildComponent(clearButton);

        filenameLabel.setJustificationType(juce::Justification::centredLeft);
        filenameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addChildComponent(filenameLabel);

        updateDisplay();
    }

    void updateDisplay()
    {
        auto filename = processor.getLoadedFilename(channelIndex);
        bool hasFilter = filename.isNotEmpty();

        importButton.setVisible(!hasFilter);
        filenameLabel.setVisible(hasFilter);
        clearButton.setVisible(hasFilter);

        if (hasFilter)
            filenameLabel.setText(filename, juce::dontSendNotification);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(2);

        if (importButton.isVisible())
        {
            importButton.setBounds(bounds);
        }
        else
        {
            clearButton.setBounds(bounds.removeFromRight(24));
            bounds.removeFromRight(4);
            filenameLabel.setBounds(bounds);
        }
    }

private:
    void importFile()
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
                    processor.loadFilterFile(channelIndex, file);
                    updateDisplay();
                    parentTable.refresh();
                }
            });
    }

    void clearChannel()
    {
        processor.clearChannel(channelIndex);
        updateDisplay();
        parentTable.refresh();
    }

    RoomMultiEQAudioProcessor& processor;
    int channelIndex;
    ChannelSummaryTable& parentTable;

    juce::TextButton importButton;
    juce::TextButton clearButton;
    juce::Label filenameLabel;
};

// Sparkline showing filter response curve
class TableSparkline : public juce::Component,
                       private juce::Timer
{
public:
    TableSparkline(RoomMultiEQAudioProcessor& p, int channel)
        : processor(p), channelIndex(channel)
    {
        startTimerHz(10);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2);

        // Draw background
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(bounds, 3.0f);

        double sr = processor.getCurrentSampleRate();
        if (sr <= 0.0)
            return;

        // Check if channel has a filter loaded
        if (processor.getLoadedFilename(channelIndex).isEmpty())
        {
            // Draw flat line for empty channel
            g.setColour(juce::Colours::grey.withAlpha(0.3f));
            float y = bounds.getCentreY();
            g.drawHorizontalLine(static_cast<int>(y), bounds.getX() + 2, bounds.getRight() - 2);
            return;
        }

        const auto& ch = processor.getChannel(channelIndex);
        auto response = FilterResponseCalculator::calculateResponse(ch, sr, static_cast<int>(bounds.getWidth()));

        juce::Path path;
        const float minDB = -24.0f;
        const float maxDB = 24.0f;

        for (int i = 0; i < static_cast<int>(response.size()); ++i)
        {
            float x = bounds.getX() + static_cast<float>(i);
            float db = std::clamp(response[static_cast<size_t>(i)], minDB, maxDB);
            float y = bounds.getY() + (1.0f - (db - minDB) / (maxDB - minDB)) * bounds.getHeight();

            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }

        g.setColour(ChannelColors::getChannelColor(channelIndex));
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }

private:
    void timerCallback() override { repaint(); }

    RoomMultiEQAudioProcessor& processor;
    int channelIndex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TableSparkline)
};

//==============================================================================
// ChannelSummaryTable
//==============================================================================

ChannelSummaryTable::ChannelSummaryTable(RoomMultiEQAudioProcessor& p,
                                          MultiChannelSpectrumAnalyzer& analyzer)
    : processor(p), spectrumAnalyzer(analyzer)
{
    table.setModel(this);
    table.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff2a2a2a));
    table.setRowHeight(36);

    auto& header = table.getHeader();
    header.addColumn("Channel", 1, 80, 60, 100);
    header.addColumn("Filter", 2, 200, 150, 300);
    header.addColumn("Response", 3, 150, 100, -1);  // -1 max = no limit
    header.setStretchToFitActive(true);

    addAndMakeVisible(table);
}

ChannelSummaryTable::~ChannelSummaryTable()
{
}

void ChannelSummaryTable::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff353535));
}

void ChannelSummaryTable::resized()
{
    table.setBounds(getLocalBounds());
}

int ChannelSummaryTable::getNumRows()
{
    return processor.getNumChannels();
}

void ChannelSummaryTable::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool)
{
    juce::ignoreUnused(width, height);
    g.fillAll(rowNumber % 2 == 0 ? juce::Colour(0xff2a2a2a) : juce::Colour(0xff323232));
}

void ChannelSummaryTable::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool)
{
    if (columnId == 1)
    {
        // Channel name with color indicator
        g.setColour(ChannelColors::getChannelColor(rowNumber));
        g.fillEllipse(4.0f, (height - 8.0f) / 2.0f, 8.0f, 8.0f);

        g.setColour(juce::Colours::white);
        g.drawText(processor.getChannelName(rowNumber), 16, 0, width - 20, height, juce::Justification::centredLeft);
    }
}

juce::Component* ChannelSummaryTable::refreshComponentForCell(int rowNumber, int columnId, bool, juce::Component* existing)
{
    delete existing;

    if (columnId == 1)
        return nullptr;

    if (columnId == 2)
        return new ImportCell(processor, rowNumber, *this);

    if (columnId == 3)
        return new TableSparkline(processor, rowNumber);

    return nullptr;
}

void ChannelSummaryTable::refresh()
{
    table.updateContent();
    table.repaint();

    // Update spectrum analyzer visibility based on bypass state
    for (int i = 0; i < processor.getNumChannels(); ++i)
    {
        bool visible = !processor.isChannelBypassed(i);
        spectrumAnalyzer.setChannelVisible(i, visible);
    }
}

//==============================================================================
// RoomMultiEQAudioProcessorEditor
//==============================================================================

RoomMultiEQAudioProcessorEditor::RoomMultiEQAudioProcessorEditor(RoomMultiEQAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p)
{
    // Multi-channel spectrum analyzer
    spectrumAnalyzer = std::make_unique<MultiChannelSpectrumAnalyzer>(p);
    addAndMakeVisible(*spectrumAnalyzer);
    spectrumAnalyzer->startAnalysis();

    // Channel summary table
    channelTable = std::make_unique<ChannelSummaryTable>(p, *spectrumAnalyzer);
    addAndMakeVisible(*channelTable);

    // Initialize visibility state
    updateSpectrumVisibility();

    setSize(600, 450);
    setResizable(true, true);
    setResizeLimits(500, 350, 1000, 700);
}

RoomMultiEQAudioProcessorEditor::~RoomMultiEQAudioProcessorEditor()
{
}

void RoomMultiEQAudioProcessorEditor::updateSpectrumVisibility()
{
    for (int i = 0; i < audioProcessor.getNumChannels(); ++i)
    {
        bool visible = !audioProcessor.isChannelBypassed(i);
        spectrumAnalyzer->setChannelVisible(i, visible);
    }
}

void RoomMultiEQAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(20.0f).withStyle("Bold"));
    g.drawText("Room Multi EQ", 10, 5, getWidth() - 20, 35, juce::Justification::centredLeft);
}

void RoomMultiEQAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Header
    bounds.removeFromTop(45);

    // Spectrum analyzer (60% of remaining height)
    int spectrumHeight = static_cast<int>(bounds.getHeight() * 0.6f);
    spectrumAnalyzer->setBounds(bounds.removeFromTop(spectrumHeight));

    // Gap
    bounds.removeFromTop(10);

    // Channel table (remaining space)
    channelTable->setBounds(bounds.reduced(10, 0));
}
