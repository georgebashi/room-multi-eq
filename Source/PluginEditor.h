#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MultiChannelSpectrumAnalyzer.h"
#include "FilterResponseCalculator.h"
#include "ChannelColors.h"

class RoomMultiEQAudioProcessorEditor;  // Forward declaration

class ChannelSummaryTable : public juce::Component,
                            public juce::TableListBoxModel
{
public:
    explicit ChannelSummaryTable(RoomMultiEQAudioProcessor& processor,
                                  MultiChannelSpectrumAnalyzer& analyzer);
    ~ChannelSummaryTable() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // TableListBoxModel
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

    void refresh();

private:
    RoomMultiEQAudioProcessor& processor;
    MultiChannelSpectrumAnalyzer& spectrumAnalyzer;
    juce::TableListBox table;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelSummaryTable)
};

class FilterResponseGraph;

class RoomMultiEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit RoomMultiEQAudioProcessorEditor(RoomMultiEQAudioProcessor&);
    ~RoomMultiEQAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void updateSpectrumVisibility();

    RoomMultiEQAudioProcessor& audioProcessor;

    std::unique_ptr<MultiChannelSpectrumAnalyzer> spectrumAnalyzer;
    std::unique_ptr<ChannelSummaryTable> channelTable;
    std::unique_ptr<FilterResponseGraph> filterGraph;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoomMultiEQAudioProcessorEditor)
};
