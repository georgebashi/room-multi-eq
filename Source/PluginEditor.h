#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MultiChannelSpectrumAnalyzer.h"
#include "ChannelColors.h"

class RoomMultiEQAudioProcessorEditor;  // Forward declaration

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

    juce::ToggleButton masterBypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    std::unique_ptr<MultiChannelSpectrumAnalyzer> spectrumAnalyzer;

    // Visibility toggles
    std::vector<std::unique_ptr<juce::ToggleButton>> visibilityToggles;

    // Channel selector
    juce::ComboBox channelSelector;
    juce::TextButton importButton;
    juce::TextButton clearButton;

    // Single channel table
    ChannelEQComponent channelTable;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoomMultiEQAudioProcessorEditor)
};
