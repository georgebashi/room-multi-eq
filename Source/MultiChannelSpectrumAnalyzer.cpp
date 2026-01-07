// Source/MultiChannelSpectrumAnalyzer.cpp
#include "MultiChannelSpectrumAnalyzer.h"
#include "PluginProcessor.h"
#include "FilterResponseCalculator.h"
#include <cmath>

MultiChannelSpectrumAnalyzer::MultiChannelSpectrumAnalyzer(RoomMultiEQAudioProcessor& p)
    : processor(p)
{
    channelVisibility.resize(static_cast<size_t>(RoomMultiEQAudioProcessor::MAX_CHANNELS), true);

    // Initialize smoothed spectrum storage for each channel
    smoothedInputs.resize(static_cast<size_t>(RoomMultiEQAudioProcessor::MAX_CHANNELS));
    smoothedOutputs.resize(static_cast<size_t>(RoomMultiEQAudioProcessor::MAX_CHANNELS));
    for (int i = 0; i < RoomMultiEQAudioProcessor::MAX_CHANNELS; ++i)
    {
        smoothedInputs[static_cast<size_t>(i)].resize(SpectrumDataCollector::fftSize / 2, -100.0f);
        smoothedOutputs[static_cast<size_t>(i)].resize(SpectrumDataCollector::fftSize / 2, -100.0f);
    }
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
    double sr = processor.getCurrentSampleRate();
    if (sr <= 0.0)
    {
        repaint();
        return;
    }

    // Skip updating spectrum data when audio stopped - accumulation buffer will fade it out
    if (processor.isAudioStopped())
    {
        repaint();
        return;
    }

    int numChannels = processor.getNumChannels();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (!channelVisibility[static_cast<size_t>(ch)])
            continue;

        auto& collector = processor.getSpectrumCollector(ch);
        auto [inputSpectrum, outputSpectrum] = collector.getBothSpectrums(sr);

        auto& smoothedIn = smoothedInputs[static_cast<size_t>(ch)];
        auto& smoothedOut = smoothedOutputs[static_cast<size_t>(ch)];

        for (size_t i = 0; i < smoothedIn.size() && i < inputSpectrum.size(); ++i)
        {
            smoothedIn[i] = smoothingFactor * smoothedIn[i] + (1.0f - smoothingFactor) * inputSpectrum[i];
            smoothedOut[i] = smoothingFactor * smoothedOut[i] + (1.0f - smoothingFactor) * outputSpectrum[i];
        }
    }

    repaint();
}

void MultiChannelSpectrumAnalyzer::paint(juce::Graphics& g)
{
    drawBackground(g);

    // Draw accumulated spectrum with motion blur
    updateAccumulationBuffer();
    if (accumulationBuffer.isValid())
        g.drawImageAt(accumulationBuffer, 0, 0);
}

void MultiChannelSpectrumAnalyzer::resized()
{
    // Invalidate buffer on resize
    accumulationBuffer = juce::Image();
}

void MultiChannelSpectrumAnalyzer::updateAccumulationBuffer()
{
    auto bounds = getLocalBounds();
    if (bounds.isEmpty())
        return;

    // Create or resize buffer if needed
    if (!accumulationBuffer.isValid() ||
        accumulationBuffer.getWidth() != bounds.getWidth() ||
        accumulationBuffer.getHeight() != bounds.getHeight())
    {
        accumulationBuffer = juce::Image(juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    }

    // Apply blur and fade to accumulation buffer
    // Older data gets progressively more blurry as it accumulates blur each frame
    {
        juce::Image blurred(juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
        juce::Graphics blurG(blurred);

        // Gaussian-ish blur kernel: center + two rings of offset copies
        // Total weight should be ~trailFade to control fade rate
        const float centerWeight = trailFade * 0.4f;
        const float innerWeight = trailFade * 0.1f;   // 4 copies
        const float outerWeight = trailFade * 0.025f; // 4 copies

        // Center
        blurG.setOpacity(centerWeight);
        blurG.drawImageAt(accumulationBuffer, 0, 0);

        // Inner ring (1px)
        blurG.setOpacity(innerWeight);
        blurG.drawImageAt(accumulationBuffer, -1, 0);
        blurG.drawImageAt(accumulationBuffer, 1, 0);
        blurG.drawImageAt(accumulationBuffer, 0, -1);
        blurG.drawImageAt(accumulationBuffer, 0, 1);

        // Outer ring (2px)
        blurG.setOpacity(outerWeight);
        blurG.drawImageAt(accumulationBuffer, -2, 0);
        blurG.drawImageAt(accumulationBuffer, 2, 0);
        blurG.drawImageAt(accumulationBuffer, 0, -2);
        blurG.drawImageAt(accumulationBuffer, 0, 2);

        accumulationBuffer = std::move(blurred);
    }

    // Draw new spectrum frame onto buffer with some transparency
    // This helps it blend with the trail rather than sitting sharply on top
    // Skip drawing when audio stopped - the blur/fade above will naturally fade out the display
    if (!processor.isAudioStopped())
    {
        juce::Graphics bufferG(accumulationBuffer);
        bufferG.setOpacity(0.7f);

        int numChannels = processor.getNumChannels();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (channelVisibility[static_cast<size_t>(ch)])
            {
                drawDifferenceSpectrum(bufferG, ch);
            }
        }
    }
}

void MultiChannelSpectrumAnalyzer::drawBackground(juce::Graphics& g)
{
    g.fillAll(juce::Colour(colBackground));
}

void MultiChannelSpectrumAnalyzer::drawDifferenceSpectrum(juce::Graphics& g, int channelIndex)
{
    double sr = processor.getCurrentSampleRate();
    const auto& smoothedInput = smoothedInputs[static_cast<size_t>(channelIndex)];
    const auto& smoothedOutput = smoothedOutputs[static_cast<size_t>(channelIndex)];

    if (smoothedInput.empty() || smoothedOutput.empty() || sr <= 0.0)
        return;

    // Check if there's any meaningful signal - find peak level
    float peakLevel = -200.0f;
    for (size_t i = 1; i < smoothedInput.size(); ++i)
    {
        peakLevel = std::max(peakLevel, smoothedInput[i]);
        peakLevel = std::max(peakLevel, smoothedOutput[i]);
    }

    // Don't draw if signal is below silence threshold
    if (peakLevel < silenceThresholdDB)
        return;

    auto bounds = getLocalBounds().toFloat();

    // Clip to bounds so off-screen portions aren't visible
    g.reduceClipRegion(bounds.toNearestInt());

    const float binWidth = static_cast<float>(sr) / static_cast<float>(SpectrumDataCollector::fftSize);
    const float minLineThickness = 1.0f;

    // Interpolate spectrum at a given frequency using cubic interpolation between bins
    auto interpolateSpectrum = [&](const std::vector<float>& spectrum, float freq) -> float {
        float bin = freq / binWidth;
        int binInt = static_cast<int>(bin);
        float frac = bin - static_cast<float>(binInt);

        // Get 4 neighboring bins for cubic interpolation (clamped at edges)
        auto getBin = [&](int idx) -> float {
            if (idx < 1) idx = 1;
            if (idx >= static_cast<int>(spectrum.size())) idx = static_cast<int>(spectrum.size()) - 1;
            return spectrum[static_cast<size_t>(idx)];
        };

        float p0 = getBin(binInt - 1);
        float p1 = getBin(binInt);
        float p2 = getBin(binInt + 1);
        float p3 = getBin(binInt + 2);

        // Catmull-Rom cubic interpolation
        float t = frac;
        float t2 = t * t;
        float t3 = t2 * t;

        return 0.5f * ((2.0f * p1) +
                       (-p0 + p2) * t +
                       (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                       (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    };

    // Sample at regular intervals in log-frequency space (screen space)
    // Use ~2 pixels per sample for smooth curves
    const int numSamples = static_cast<int>(bounds.getWidth() / 2.0f);
    const float logMin = std::log10(minFreq);
    const float logMax = std::log10(maxFreq);

    struct Point {
        float x, yMin, yMax;
    };
    std::vector<Point> points;
    points.reserve(static_cast<size_t>(numSamples));

    bool seenData = false;
    size_t firstDataIdx = 0;
    size_t lastDataIdx = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(numSamples - 1);
        float freq = std::pow(10.0f, logMin + t * (logMax - logMin));
        float x = bounds.getX() + t * bounds.getWidth();

        float inputDB = std::clamp(interpolateSpectrum(smoothedInput, freq), minDB - 20.0f, maxDB);
        float outputDB = std::clamp(interpolateSpectrum(smoothedOutput, freq), minDB - 20.0f, maxDB);

        // Track data range (where we have signal above the floor)
        bool hasSignal = (inputDB > minDB || outputDB > minDB);
        if (hasSignal)
        {
            if (!seenData)
            {
                seenData = true;
                firstDataIdx = points.size();
            }
            lastDataIdx = points.size();
        }

        float yInput = bounds.getY() + dbToY(inputDB) * bounds.getHeight();
        float yOutput = bounds.getY() + dbToY(outputDB) * bounds.getHeight();

        float yMin = std::min(yInput, yOutput);
        float yMax = std::max(yInput, yOutput);

        // Ensure minimum thickness
        if (yMax - yMin < minLineThickness)
        {
            float center = (yMin + yMax) / 2.0f;
            yMin = center - minLineThickness / 2.0f;
            yMax = center + minLineThickness / 2.0f;
        }

        points.push_back({x, yMin, yMax});
    }

    // Only draw if we have some data above the floor
    if (!seenData || points.empty())
        return;

    // Trim to only the range where we have data
    if (lastDataIdx + 1 < points.size())
        points.resize(lastDataIdx + 1);
    if (firstDataIdx > 0)
        points.erase(points.begin(), points.begin() + static_cast<std::ptrdiff_t>(firstDataIdx));

    if (points.empty())
        return;

    // Build path - points are already smoothly sampled, use straight lines
    // (the density of sampling makes curves unnecessary)
    juce::Path path;

    path.startNewSubPath(points[0].x, points[0].yMin);
    for (size_t i = 1; i < points.size(); ++i)
        path.lineTo(points[i].x, points[i].yMin);

    for (size_t i = points.size(); i > 0; --i)
        path.lineTo(points[i - 1].x, points[i - 1].yMax);

    path.closeSubPath();

    // Use channel color for the difference spectrum
    auto color = ChannelColors::getChannelColor(channelIndex);
    g.setColour(color);
    g.fillPath(path);
    g.strokePath(path, juce::PathStrokeType(1.0f));
}

void MultiChannelSpectrumAnalyzer::drawFilterCurve(juce::Graphics& g, int channelIndex)
{
    double sr = processor.getCurrentSampleRate();
    if (sr <= 0.0)
        return;

    auto bounds = getLocalBounds().toFloat();

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
