// Source/SpectrumAnalyzer.h
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "SpectrumDataCollector.h"
#include "ChannelEQ.h"
#include <vector>

class SpectrumAnalyzer : public juce::Component,
                          private juce::Timer
{
public:
    SpectrumAnalyzer(SpectrumDataCollector& collector,
                     const ChannelEQ& channel,
                     double& sampleRateRef);
    ~SpectrumAnalyzer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void startAnalysis();
    void stopAnalysis();

private:
    void timerCallback() override;

    void drawBackground(juce::Graphics& g);
    void drawGrid(juce::Graphics& g);
    void drawSpectrum(juce::Graphics& g, const std::vector<float>& spectrum, juce::Colour colour, bool filled);
    void drawFilterCurve(juce::Graphics& g);

    float frequencyToX(float freq) const;
    float dbToY(float db) const;

    SpectrumDataCollector& collector;
    const ChannelEQ& channel;
    double& sampleRate;

    std::vector<float> smoothedInput;
    std::vector<float> smoothedOutput;

    // Dracula theme colors
    static constexpr juce::uint32 colBackground = 0xff282a36;
    static constexpr juce::uint32 colGridLine = 0xff44475a;
    static constexpr juce::uint32 colInputSpectrum = 0xff6272a4;
    static constexpr juce::uint32 colOutputSpectrum = 0x6650fa7b;  // 40% opacity green
    static constexpr juce::uint32 colFilterCurve = 0xffffb86c;
    static constexpr juce::uint32 colText = 0xfff8f8f2;
    static constexpr juce::uint32 colTitle = 0xffbd93f9;

    // Display range
    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr float minDB = -60.0f;
    static constexpr float maxDB = 12.0f;

    // Smoothing factor (0 = no smoothing, 1 = infinite smoothing)
    static constexpr float smoothingFactor = 0.7f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
