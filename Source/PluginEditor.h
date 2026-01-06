#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class ChannelEQComponent : public juce::Component,
                           public juce::TableListBoxModel
{
public:
    ChannelEQComponent(RoomMultiEQAudioProcessor& processor, bool isLeft);
    ~ChannelEQComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // TableListBoxModel
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

    void updateVisibleBands();

private:
    void importFilterFile();
    void clearAll();
    bool isBandActive(int bandIndex) const;
    int getBandIndexForRow(int row) const;

    RoomMultiEQAudioProcessor& processor;
    bool isLeftChannel;
    juce::String channelPrefix;

    juce::TextButton importButton;
    juce::TextButton clearButton;
    juce::TableListBox table;

    std::vector<int> visibleBands;  // Maps row index to band index

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

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
    RoomMultiEQAudioProcessor& audioProcessor;

    juce::ToggleButton masterBypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    ChannelEQComponent leftChannelComponent;
    ChannelEQComponent rightChannelComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoomMultiEQAudioProcessorEditor)
};
