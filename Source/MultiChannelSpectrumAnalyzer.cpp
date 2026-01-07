// Source/MultiChannelSpectrumAnalyzer.cpp
#include "MultiChannelSpectrumAnalyzer.h"
#include "PluginProcessor.h"
#include "FilterResponseCalculator.h"
#include <cmath>

MultiChannelSpectrumAnalyzer::MultiChannelSpectrumAnalyzer(RoomMultiEQAudioProcessor& p)
    : processor(p)
{
    channelVisibility.resize(static_cast<size_t>(RoomMultiEQAudioProcessor::MAX_CHANNELS), true);
}

MultiChannelSpectrumAnalyzer::~MultiChannelSpectrumAnalyzer()
{
    stopTimer();
}

void MultiChannelSpectrumAnalyzer::startAnalysis()
{
    startTimerHz(30);
}

void MultiChannelSpectrumAnalyzer::stopAnalysis()
{
    stopTimer();
}

void MultiChannelSpectrumAnalyzer::setChannelVisible(int channelIndex, bool visible)
{
    if (channelIndex >= 0 && channelIndex < static_cast<int>(channelVisibility.size()))
    {
        channelVisibility[static_cast<size_t>(channelIndex)] = visible;
        repaint();
    }
}

bool MultiChannelSpectrumAnalyzer::isChannelVisible(int channelIndex) const
{
    if (channelIndex >= 0 && channelIndex < static_cast<int>(channelVisibility.size()))
        return channelVisibility[static_cast<size_t>(channelIndex)];
    return false;
}

void MultiChannelSpectrumAnalyzer::timerCallback()
{
    repaint();
}

void MultiChannelSpectrumAnalyzer::paint(juce::Graphics& g)
{
    drawBackground(g);
    drawGrid(g);

    // Draw filter curves for all visible channels
    int numChannels = processor.getNumChannels();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (channelVisibility[static_cast<size_t>(ch)])
        {
            drawFilterCurve(g, ch);
        }
    }
}

void MultiChannelSpectrumAnalyzer::resized()
{
}

void MultiChannelSpectrumAnalyzer::drawBackground(juce::Graphics& g)
{
    g.fillAll(juce::Colour(colBackground));
}

void MultiChannelSpectrumAnalyzer::drawGrid(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(30.0f, 20.0f);
    bounds.removeFromBottom(20.0f);
    bounds.removeFromLeft(25.0f);

    g.setColour(juce::Colour(colGridLine));

    // Vertical lines at frequency decades
    const float freqs[] = {100.0f, 1000.0f, 10000.0f};
    for (float freq : freqs)
    {
        float x = bounds.getX() + frequencyToX(freq) * bounds.getWidth();
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
    }

    // Horizontal lines at dB intervals
    const float dbs[] = {-60.0f, -48.0f, -36.0f, -24.0f, -12.0f, 0.0f, 12.0f};
    for (float db : dbs)
    {
        float y = bounds.getY() + dbToY(db) * bounds.getHeight();
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }

    // Labels
    g.setColour(juce::Colour(colText));
    g.setFont(10.0f);

    const char* freqLabels[] = {"100", "1k", "10k"};
    for (int i = 0; i < 3; ++i)
    {
        float x = bounds.getX() + frequencyToX(freqs[i]) * bounds.getWidth();
        g.drawText(freqLabels[i], static_cast<int>(x) - 15, static_cast<int>(bounds.getBottom()) + 2, 30, 16, juce::Justification::centred);
    }

    const char* dbLabels[] = {"-60", "-48", "-36", "-24", "-12", "0", "+12"};
    const float dbValues[] = {-60.0f, -48.0f, -36.0f, -24.0f, -12.0f, 0.0f, 12.0f};
    for (int i = 0; i < 7; ++i)
    {
        float y = bounds.getY() + dbToY(dbValues[i]) * bounds.getHeight();
        g.drawText(dbLabels[i], 2, static_cast<int>(y) - 8, 25, 16, juce::Justification::right);
    }
}

void MultiChannelSpectrumAnalyzer::drawFilterCurve(juce::Graphics& g, int channelIndex)
{
    double sr = processor.getCurrentSampleRate();
    if (sr <= 0.0)
        return;

    auto bounds = getLocalBounds().toFloat().reduced(30.0f, 20.0f);
    bounds.removeFromBottom(20.0f);
    bounds.removeFromLeft(25.0f);

    const auto& channel = processor.getChannel(channelIndex);
    auto response = FilterResponseCalculator::calculateResponse(channel, sr, 200);

    juce::Path path;

    for (int i = 0; i < 200; ++i)
    {
        float t = static_cast<float>(i) / 199.0f;
        float x = bounds.getX() + t * bounds.getWidth();
        float db = std::clamp(response[static_cast<size_t>(i)], minDB, maxDB);
        float y = bounds.getY() + dbToY(db) * bounds.getHeight();

        if (i == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    g.setColour(ChannelColors::getChannelColor(channelIndex));
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

float MultiChannelSpectrumAnalyzer::frequencyToX(float freq) const
{
    float logMin = std::log10(minFreq);
    float logMax = std::log10(maxFreq);
    float logFreq = std::log10(std::clamp(freq, minFreq, maxFreq));
    return (logFreq - logMin) / (logMax - logMin);
}

float MultiChannelSpectrumAnalyzer::dbToY(float db) const
{
    return 1.0f - (db - minDB) / (maxDB - minDB);
}
