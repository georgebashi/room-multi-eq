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
    void drawFilterCurve(juce::Graphics& g, int channelIndex);

    // Motion blur accumulation buffer
    juce::Image createAdditiveComposite(const juce::Rectangle<int>& bounds);
    void updateAccumulationBuffer(const juce::Image& currentFrame);
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

    // Display range - use shared constants for frequency
    static constexpr float minFreq = VisualizationConstants::minFreq;
    static constexpr float maxFreq = VisualizationConstants::maxFreq;
    static constexpr float maxDB = 0.0f;
    static constexpr float minDB = -120.0f;
    static constexpr float silenceThresholdDB = -90.0f;  // Don't draw below this

    // Smoothing factor (0 = no smoothing, 1 = infinite smoothing)
    static constexpr float smoothingFactor = 0.7f;

    // Frame accumulation fade (0 = no trail, 1 = infinite trail)
    static constexpr float trailFade = 0.90f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiChannelSpectrumAnalyzer)
};
