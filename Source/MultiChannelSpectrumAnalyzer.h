// Source/MultiChannelSpectrumAnalyzer.h
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "ChannelEQ.h"
#include "ChannelColors.h"
#include <vector>

class RoomMultiEQAudioProcessor;

class MultiChannelSpectrumAnalyzer : public juce::Component,
                                      private juce::Timer
{
public:
    explicit MultiChannelSpectrumAnalyzer(RoomMultiEQAudioProcessor& processor);
    ~MultiChannelSpectrumAnalyzer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void startAnalysis();
    void stopAnalysis();

    // Channel visibility control
    void setChannelVisible(int channelIndex, bool visible);
    bool isChannelVisible(int channelIndex) const;

private:
    void timerCallback() override;

    void drawBackground(juce::Graphics& g);
    void drawGrid(juce::Graphics& g);
    void drawFilterCurve(juce::Graphics& g, int channelIndex);

    float frequencyToX(float freq) const;
    float dbToY(float db) const;

    RoomMultiEQAudioProcessor& processor;
    std::vector<bool> channelVisibility;

    // Dracula theme colors
    static constexpr juce::uint32 colBackground = 0xff282a36;
    static constexpr juce::uint32 colGridLine = 0xff44475a;
    static constexpr juce::uint32 colText = 0xfff8f8f2;

    // Display range
    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr float minDB = -24.0f;
    static constexpr float maxDB = 12.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiChannelSpectrumAnalyzer)
};
