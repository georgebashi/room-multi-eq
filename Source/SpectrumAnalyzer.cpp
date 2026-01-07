// Source/SpectrumAnalyzer.cpp
#include "SpectrumAnalyzer.h"
#include "PluginProcessor.h"
#include "FilterResponseCalculator.h"
#include <cmath>

SpectrumAnalyzer::SpectrumAnalyzer(SpectrumDataCollector& c,
                                   const ChannelEQ& ch,
                                   RoomMultiEQAudioProcessor& p)
    : collector(c), channel(ch), processorRef(p)
{
    // Initialize to -100dB (silence) - actual values will smooth in when audio plays
    smoothedInput.resize(SpectrumDataCollector::fftSize / 2, -100.0f);
    smoothedOutput.resize(SpectrumDataCollector::fftSize / 2, -100.0f);
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

void SpectrumAnalyzer::forceUpdate()
{
    double sr = processorRef.getCurrentSampleRate();

    // Directly copy spectrum data without time smoothing (for testing)
    auto inputSpectrum = collector.getInputSpectrum(sr);
    auto outputSpectrum = collector.getOutputSpectrum(sr);

    for (size_t i = 0; i < smoothedInput.size() && i < inputSpectrum.size(); ++i)
    {
        smoothedInput[i] = inputSpectrum[i];
        smoothedOutput[i] = outputSpectrum[i];
    }
}

void SpectrumAnalyzer::timerCallback()
{
    double sr = processorRef.getCurrentSampleRate();

    // Get new spectrum data with psychoacoustic smoothing
    auto inputSpectrum = collector.getInputSpectrum(sr);
    auto outputSpectrum = collector.getOutputSpectrum(sr);

    // Apply time smoothing
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

    // Draw accumulated spectrum with motion blur
    updateAccumulationBuffer();
    if (accumulationBuffer.isValid())
        g.drawImageAt(accumulationBuffer, 0, 0);

    drawFilterCurve(g);
}

void SpectrumAnalyzer::resized()
{
    // Invalidate buffer on resize
    accumulationBuffer = juce::Image();
}

void SpectrumAnalyzer::updateAccumulationBuffer()
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
    {
        juce::Graphics bufferG(accumulationBuffer);
        bufferG.setOpacity(0.7f);
        drawDifferenceSpectrum(bufferG);
    }
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
    const float dbs[] = {-60.0f, -48.0f, -36.0f, -24.0f, -12.0f, 0.0f, 12.0f};
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
    const char* dbLabels[] = {"-60", "-36", "-12", "+12"};
    const float dbValues[] = {-60.0f, -36.0f, -12.0f, 12.0f};
    for (int i = 0; i < 4; ++i)
    {
        float y = bounds.getY() + dbToY(dbValues[i]) * bounds.getHeight();
        g.drawText(dbLabels[i], 2, static_cast<int>(y) - 8, 25, 16, juce::Justification::right);
    }
}

void SpectrumAnalyzer::drawSpectrum(juce::Graphics& g, const std::vector<float>& spectrum, juce::Colour colour, bool filled)
{
    double sr = processorRef.getCurrentSampleRate();
    if (spectrum.empty() || sr <= 0.0)
        return;

    auto bounds = getLocalBounds().toFloat().reduced(30.0f, 20.0f);
    bounds.removeFromBottom(20.0f);
    bounds.removeFromLeft(25.0f);

    juce::Path path;
    bool pathStarted = false;

    const float binWidth = static_cast<float>(sr) / static_cast<float>(SpectrumDataCollector::fftSize);

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
    double sr = processorRef.getCurrentSampleRate();
    if (sr <= 0.0)
        return;

    auto bounds = getLocalBounds().toFloat().reduced(30.0f, 20.0f);
    bounds.removeFromBottom(20.0f);
    bounds.removeFromLeft(25.0f);

    auto response = FilterResponseCalculator::calculateResponse(channel, sr, 200);

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

void SpectrumAnalyzer::drawDifferenceSpectrum(juce::Graphics& g)
{
    double sr = processorRef.getCurrentSampleRate();
    if (smoothedInput.empty() || smoothedOutput.empty() || sr <= 0.0)
        return;

    auto bounds = getLocalBounds().toFloat().reduced(30.0f, 20.0f);
    bounds.removeFromBottom(20.0f);
    bounds.removeFromLeft(25.0f);

    // Clip to bounds so off-screen portions aren't visible
    g.reduceClipRegion(bounds.toNearestInt());

    const float binWidth = static_cast<float>(sr) / static_cast<float>(SpectrumDataCollector::fftSize);
    const float minLineThickness = 1.0f;  // Minimum 1px line thickness
    const float boostThresholdDB = 0.5f;  // Hysteresis to prevent flickering

    // We'll draw segments, each colored based on boost vs cut
    struct Point {
        float x, yMin, yMax;
        bool isBoost;  // true = output > input (white), false = output < input (purple)
    };
    std::vector<Point> points;

    // Track if we've seen any data above the floor
    bool seenData = false;
    size_t firstDataIdx = 0;
    size_t lastDataIdx = 0;

    for (size_t i = 1; i < smoothedInput.size() && i < smoothedOutput.size(); ++i)
    {
        float freq = static_cast<float>(i) * binWidth;
        if (freq < minFreq || freq > maxFreq)
            continue;

        float x = bounds.getX() + frequencyToX(freq) * bounds.getWidth();

        // Don't clamp - let values go off-screen for continuous appearance
        float inputDB = std::clamp(smoothedInput[i], minDB - 20.0f, maxDB);  // Allow 20dB below floor
        float outputDB = std::clamp(smoothedOutput[i], minDB - 20.0f, maxDB);

        // Track data range (where we have signal above the floor)
        bool hasSignal = (smoothedInput[i] > minDB || smoothedOutput[i] > minDB);
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

        // Determine min/max y (remember: lower y = higher dB on screen)
        float yMin = std::min(yInput, yOutput);
        float yMax = std::max(yInput, yOutput);

        // Ensure minimum thickness
        if (yMax - yMin < minLineThickness)
        {
            float center = (yMin + yMax) / 2.0f;
            yMin = center - minLineThickness / 2.0f;
            yMax = center + minLineThickness / 2.0f;
        }

        // isBoost with hysteresis: only switch color when difference exceeds threshold
        // This prevents flickering when input and output are nearly equal
        bool isBoost = (outputDB - inputDB) > boostThresholdDB;

        points.push_back({x, yMin, yMax, isBoost});
    }

    // Only draw if we have some data above the floor
    if (!seenData || points.empty())
        return;

    // Trim to only the range where we have data (avoid drawing flat lines at edges)
    if (lastDataIdx + 1 < points.size())
        points.resize(lastDataIdx + 1);
    if (firstDataIdx > 0)
        points.erase(points.begin(), points.begin() + firstDataIdx);

    if (points.empty())
        return;

    // Draw filled regions by color
    // At color transitions, include the boundary point in the current region
    // to avoid gaps between adjacent regions
    size_t i = 0;
    while (i < points.size())
    {
        bool currentBoost = points[i].isBoost;
        size_t start = i;

        // Find end of contiguous same-color region
        while (i < points.size() && points[i].isBoost == currentBoost)
            ++i;

        if (i == start)
            continue;

        // Determine the end index for drawing - include next point if it's a color transition
        // so adjacent regions share their boundary
        size_t drawEnd = i;
        if (i < points.size())
            drawEnd = i + 1;  // Include the transition point

        // Build path for this region
        juce::Path path;

        // Top edge (left to right)
        path.startNewSubPath(points[start].x, points[start].yMin);
        for (size_t j = start + 1; j < drawEnd; ++j)
            path.lineTo(points[j].x, points[j].yMin);

        // Bottom edge (right to left)
        for (size_t j = drawEnd; j > start; --j)
            path.lineTo(points[j - 1].x, points[j - 1].yMax);

        path.closeSubPath();

        auto colour = juce::Colour(currentBoost ? colBoost : colCut);
        g.setColour(colour);
        g.fillPath(path);

        // Stroke the path to ensure visibility when shape is thin horizontally
        g.strokePath(path, juce::PathStrokeType(1.0f));
    }
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
