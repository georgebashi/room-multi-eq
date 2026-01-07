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

//==============================================================================
// FilterResponseGraph - shows all channel filter curves on one graph
//==============================================================================

class FilterResponseGraph : public juce::Component,
                            private juce::Timer
{
public:
    FilterResponseGraph(RoomMultiEQAudioProcessor& p)
        : processor(p)
    {
        startTimerHz(10);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Draw background
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillAll();

        double sr = processor.getCurrentSampleRate();
        if (sr <= 0.0)
            return;

        // Draw grid
        drawGrid(g, bounds);

        // Draw filter curves for all channels
        int numChannels = processor.getNumChannels();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (processor.getLoadedFilename(ch).isEmpty())
                continue;

            const auto& channel = processor.getChannel(ch);
            auto response = FilterResponseCalculator::calculateResponse(channel, sr, static_cast<int>(bounds.getWidth()));

            juce::Path path;

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

            g.setColour(ChannelColors::getChannelColor(ch));
            g.strokePath(path, juce::PathStrokeType(2.0f));
        }
    }

private:
    void timerCallback() override { repaint(); }

    void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        g.setColour(juce::Colour(0xff3a3a3a));

        // Horizontal lines (dB)
        for (float db = minDB; db <= maxDB; db += 6.0f)
        {
            float y = bounds.getY() + (1.0f - (db - minDB) / (maxDB - minDB)) * bounds.getHeight();

            // Bold line at 0 dB
            if (std::abs(db) < 0.1f)
                g.setColour(juce::Colour(0xff5a5a5a));
            else
                g.setColour(juce::Colour(0xff3a3a3a));

            g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());

            // Label
            g.setFont(10.0f);
            g.drawText(juce::String(static_cast<int>(db)) + " dB",
                      static_cast<int>(bounds.getX()) + 4,
                      static_cast<int>(y) - 6,
                      40, 12,
                      juce::Justification::centredLeft);
        }

        // Vertical lines (frequency) - logarithmic
        const float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        const float logMin = std::log10(20.0f);
        const float logMax = std::log10(20000.0f);

        for (float freq : freqs)
        {
            float logFreq = std::log10(freq);
            float x = bounds.getX() + (logFreq - logMin) / (logMax - logMin) * bounds.getWidth();
            g.setColour(juce::Colour(0xff3a3a3a));
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());

            // Label
            juce::String label;
            if (freq >= 1000)
                label = juce::String(static_cast<int>(freq / 1000)) + "k";
            else
                label = juce::String(static_cast<int>(freq));

            g.setFont(10.0f);
            g.drawText(label,
                      static_cast<int>(x) - 15,
                      static_cast<int>(bounds.getBottom()) - 14,
                      30, 12,
                      juce::Justification::centred);
        }
    }

    RoomMultiEQAudioProcessor& processor;

    static constexpr float minDB = -24.0f;
    static constexpr float maxDB = 6.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterResponseGraph)
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
    header.addColumn("Channel", 1, 70, 50, 100);
    header.addColumn("Filter", 2, 130, 100, -1);
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

    // Filter response graph
    filterGraph = std::make_unique<FilterResponseGraph>(p);
    addAndMakeVisible(*filterGraph);

    // Initialize visibility state
    updateSpectrumVisibility();

    setSize(700, 450);
    setResizable(true, true);
    setResizeLimits(600, 350, 1200, 800);
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

    // Bottom panel: table (1/3) and filter graph (2/3)
    auto bottomPanel = bounds.reduced(10, 0);
    int tableWidth = bottomPanel.getWidth() / 3;

    channelTable->setBounds(bottomPanel.removeFromLeft(tableWidth));
    bottomPanel.removeFromLeft(10);  // Gap
    filterGraph->setBounds(bottomPanel);
}
