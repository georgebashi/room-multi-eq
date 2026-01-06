// Source/SpectrumAnalyzer.cpp
#include "SpectrumAnalyzer.h"
#include "FilterResponseCalculator.h"
#include <cmath>

SpectrumAnalyzer::SpectrumAnalyzer(SpectrumDataCollector& c,
                                   const ChannelEQ& ch,
                                   double& sr)
    : collector(c), channel(ch), sampleRate(sr)
{
    smoothedInput.resize(SpectrumDataCollector::fftSize / 2, minDB);
    smoothedOutput.resize(SpectrumDataCollector::fftSize / 2, minDB);
}

SpectrumAnalyzer::~SpectrumAnalyzer()
{
    stopTimer();
}

void SpectrumAnalyzer::startAnalysis()
{
    startTimerHz(30);  // 30fps
}

void SpectrumAnalyzer::stopAnalysis()
{
    stopTimer();
}

void SpectrumAnalyzer::timerCallback()
{
    // Get new spectrum data
    auto inputSpectrum = collector.getInputSpectrum();
    auto outputSpectrum = collector.getOutputSpectrum();

    // Apply smoothing
    for (size_t i = 0; i < smoothedInput.size() && i < inputSpectrum.size(); ++i)
    {
        smoothedInput[i] = smoothingFactor * smoothedInput[i] + (1.0f - smoothingFactor) * inputSpectrum[i];
        smoothedOutput[i] = smoothingFactor * smoothedOutput[i] + (1.0f - smoothingFactor) * outputSpectrum[i];
    }

    repaint();
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    drawBackground(g);
    drawGrid(g);
    drawSpectrum(g, smoothedInput, juce::Colour(colInputSpectrum), true);
    drawSpectrum(g, smoothedOutput, juce::Colour(colOutputSpectrum), true);
    drawFilterCurve(g);
}

void SpectrumAnalyzer::resized()
{
}

void SpectrumAnalyzer::drawBackground(juce::Graphics& g)
{
    g.fillAll(juce::Colour(colBackground));
}

void SpectrumAnalyzer::drawGrid(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(30.0f, 20.0f);
    bounds.removeFromBottom(20.0f);  // Space for frequency labels
    bounds.removeFromLeft(25.0f);    // Space for dB labels

    g.setColour(juce::Colour(colGridLine));

    // Vertical lines at 100Hz, 1kHz, 10kHz
    const float freqs[] = {100.0f, 1000.0f, 10000.0f};
    for (float freq : freqs)
    {
        float x = bounds.getX() + frequencyToX(freq) * bounds.getWidth();
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
    }

    // Horizontal lines at dB intervals
    const float dbs[] = {-24.0f, -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f};
    for (float db : dbs)
    {
        float y = bounds.getY() + dbToY(db) * bounds.getHeight();
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }

    // Draw labels
    g.setColour(juce::Colour(colText));
    g.setFont(10.0f);

    // Frequency labels
    const char* freqLabels[] = {"100", "1k", "10k"};
    for (int i = 0; i < 3; ++i)
    {
        float x = bounds.getX() + frequencyToX(freqs[i]) * bounds.getWidth();
        g.drawText(freqLabels[i], static_cast<int>(x) - 15, static_cast<int>(bounds.getBottom()) + 2, 30, 16, juce::Justification::centred);
    }

    // dB labels
    const char* dbLabels[] = {"-24", "-12", "0", "+12"};
    const float dbValues[] = {-24.0f, -12.0f, 0.0f, 12.0f};
    for (int i = 0; i < 4; ++i)
    {
        float y = bounds.getY() + dbToY(dbValues[i]) * bounds.getHeight();
        g.drawText(dbLabels[i], 2, static_cast<int>(y) - 8, 25, 16, juce::Justification::right);
    }
}

void SpectrumAnalyzer::drawSpectrum(juce::Graphics& g, const std::vector<float>& spectrum, juce::Colour colour, bool filled)
{
    if (spectrum.empty() || sampleRate <= 0.0)
        return;

    auto bounds = getLocalBounds().toFloat().reduced(30.0f, 20.0f);
    bounds.removeFromBottom(20.0f);
    bounds.removeFromLeft(25.0f);

    juce::Path path;
    bool pathStarted = false;

    const float binWidth = static_cast<float>(sampleRate) / static_cast<float>(SpectrumDataCollector::fftSize);

    for (size_t i = 1; i < spectrum.size(); ++i)
    {
        float freq = static_cast<float>(i) * binWidth;
        if (freq < minFreq || freq > maxFreq)
            continue;

        float x = bounds.getX() + frequencyToX(freq) * bounds.getWidth();
        float db = std::clamp(spectrum[i], minDB, maxDB);
        float y = bounds.getY() + dbToY(db) * bounds.getHeight();

        if (!pathStarted)
        {
            if (filled)
                path.startNewSubPath(x, bounds.getBottom());
            path.lineTo(x, y);
            pathStarted = true;
        }
        else
        {
            path.lineTo(x, y);
        }
    }

    if (pathStarted && filled)
    {
        path.lineTo(path.getCurrentPosition().x, bounds.getBottom());
        path.closeSubPath();
        g.setColour(colour);
        g.fillPath(path);
    }
    else if (pathStarted)
    {
        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }
}

void SpectrumAnalyzer::drawFilterCurve(juce::Graphics& g)
{
    if (sampleRate <= 0.0)
        return;

    auto bounds = getLocalBounds().toFloat().reduced(30.0f, 20.0f);
    bounds.removeFromBottom(20.0f);
    bounds.removeFromLeft(25.0f);

    auto response = FilterResponseCalculator::calculateResponse(channel, sampleRate, 200);

    juce::Path path;
    const float logMin = std::log10(minFreq);
    const float logMax = std::log10(maxFreq);

    for (int i = 0; i < 200; ++i)
    {
        float t = static_cast<float>(i) / 199.0f;
        float freq = std::pow(10.0f, logMin + t * (logMax - logMin));

        float x = bounds.getX() + t * bounds.getWidth();
        float db = std::clamp(response[i], minDB, maxDB);
        float y = bounds.getY() + dbToY(db) * bounds.getHeight();

        if (i == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    g.setColour(juce::Colour(colFilterCurve));
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

float SpectrumAnalyzer::frequencyToX(float freq) const
{
    // Logarithmic mapping
    float logMin = std::log10(minFreq);
    float logMax = std::log10(maxFreq);
    float logFreq = std::log10(std::clamp(freq, minFreq, maxFreq));
    return (logFreq - logMin) / (logMax - logMin);
}

float SpectrumAnalyzer::dbToY(float db) const
{
    // Linear mapping, inverted (top = max, bottom = min)
    return 1.0f - (db - minDB) / (maxDB - minDB);
}
