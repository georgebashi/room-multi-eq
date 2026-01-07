#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MultiChannelSpectrumAnalyzer.h"
#include "FilterResponseCalculator.h"
#include "ChannelColors.h"

class RoomMultiEQAudioProcessorEditor;  // Forward declaration

// Small filter curve preview for channel toggles
class FilterSparkline : public juce::Component,
                        private juce::Timer
{
public:
    FilterSparkline(RoomMultiEQAudioProcessor& p, int channel)
        : processor(p), channelIndex(channel)
    {
        startTimerHz(10);  // Update 10fps
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        double sr = processor.getCurrentSampleRate();
        if (sr <= 0.0)
            return;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterSparkline)
};

class ChannelEQComponent : public juce::Component,
                           public juce::TableListBoxModel
{
public:
    explicit ChannelEQComponent(RoomMultiEQAudioProcessor& processor);
    ~ChannelEQComponent() override;

    void setChannelIndex(int index);
    int getChannelIndex() const { return channelIndex; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    // TableListBoxModel
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

    void updateVisibleBands();

private:
    bool isBandActive(int bandIndex) const;
    int getBandIndexForRow(int row) const;

    RoomMultiEQAudioProcessor& processor;
    int channelIndex = 0;

    juce::TableListBox table;
    std::vector<int> visibleBands;  // Maps row index to band index

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelEQComponent)
};

class RoomMultiEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit RoomMultiEQAudioProcessorEditor(RoomMultiEQAudioProcessor&);
    ~RoomMultiEQAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void updateChannelSelector();
    void onChannelSelected();
    void updateVisibilityToggles();
    void importFilterFile();
    void clearAll();

    RoomMultiEQAudioProcessor& audioProcessor;

    std::unique_ptr<MultiChannelSpectrumAnalyzer> spectrumAnalyzer;

    // Visibility toggles with sparklines
    std::vector<std::unique_ptr<juce::ToggleButton>> visibilityToggles;
    std::vector<std::unique_ptr<FilterSparkline>> sparklines;

    // Channel selector
    juce::ComboBox channelSelector;
    juce::TextButton importButton;
    juce::TextButton clearButton;

    // Single channel table
    ChannelEQComponent channelTable;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoomMultiEQAudioProcessorEditor)
};
