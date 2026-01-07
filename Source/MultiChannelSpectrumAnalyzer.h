// Source/MultiChannelSpectrumAnalyzer.h
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "ChannelEQ.h"
#include "ChannelColors.h"
#include "SpectrumDataCollector.h"
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

    // Motion blur accumulation buffer
    void updateAccumulationBuffer();
    void drawDifferenceSpectrum(juce::Graphics& g, int channelIndex);

    float frequencyToX(float freq) const;
    float dbToY(float db) const;

    RoomMultiEQAudioProcessor& processor;
    std::vector<bool> channelVisibility;

    // Smoothed spectrum data per channel (input and output)
    std::vector<std::vector<float>> smoothedInputs;
    std::vector<std::vector<float>> smoothedOutputs;

    // Frame accumulation buffer for motion blur effect
    juce::Image accumulationBuffer;

    // Dracula theme colors
    static constexpr juce::uint32 colBackground = 0xff282a36;
    static constexpr juce::uint32 colGridLine = 0xff44475a;
    static constexpr juce::uint32 colText = 0xfff8f8f2;

    // Display range
    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;

    // Dynamic range with peak-hold and slow decay
    float currentMaxDB = 0.0f;            // Current display ceiling (adapts over time)
    float currentMinDB = -60.0f;          // Current display floor (adapts over time)
    static constexpr float absoluteMaxDB = 12.0f;    // Absolute highest we'll show
    static constexpr float absoluteMinDB = -120.0f;  // Absolute lowest we'll show
    static constexpr float defaultMaxDB = 0.0f;      // Starting/reset value for ceiling
    static constexpr float defaultMinDB = -60.0f;    // Starting/reset value for floor
    static constexpr float rangeDecayRate = 3.0f;    // dB per second to contract range
    static constexpr float rangeMargin = 6.0f;       // Headroom above/below signal

    // Smoothing factor (0 = no smoothing, 1 = infinite smoothing)
    static constexpr float smoothingFactor = 0.7f;

    // Frame accumulation fade (0 = no trail, 1 = infinite trail)
    static constexpr float trailFade = 0.75f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiChannelSpectrumAnalyzer)
};
