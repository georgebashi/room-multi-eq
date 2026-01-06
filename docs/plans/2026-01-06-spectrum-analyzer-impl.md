# Spectrum Analyzer Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add real-time spectrum visualization showing input/output frequency content and filter response curves per channel.

**Architecture:** Ring buffers capture audio in the processor. GUI thread performs FFT and renders spectrum fills (input/output) plus a mathematically-calculated filter response curve. Timer-based updates at 30fps, disabled when UI not visible.

**Tech Stack:** JUCE 8.0 (juce::dsp::FFT, juce::Component, juce::Timer), C++17

---

## Task 1: Add SpectrumDataCollector Class

**Files:**
- Create: `Source/SpectrumDataCollector.h`
- Create: `Source/SpectrumDataCollector.cpp`

**Step 1: Create header file**

```cpp
// Source/SpectrumDataCollector.h
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

class SpectrumDataCollector
{
public:
    static constexpr int fftOrder = 12;  // 2^12 = 4096
    static constexpr int fftSize = 1 << fftOrder;

    SpectrumDataCollector();

    void pushInputSample(float sample);
    void pushOutputSample(float sample);

    // Returns spectrum in dB, size = fftSize/2
    std::vector<float> getInputSpectrum();
    std::vector<float> getOutputSpectrum();

private:
    std::vector<float> computeSpectrum(const std::array<float, fftSize>& ringBuffer);

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, fftSize> inputRingBuffer{};
    std::array<float, fftSize> outputRingBuffer{};
    std::atomic<int> writeIndex{0};
};
```

**Step 2: Create implementation file**

```cpp
// Source/SpectrumDataCollector.cpp
#include "SpectrumDataCollector.h"
#include <cmath>

SpectrumDataCollector::SpectrumDataCollector()
    : fft(fftOrder),
      window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
}

void SpectrumDataCollector::pushInputSample(float sample)
{
    int idx = writeIndex.load(std::memory_order_relaxed);
    inputRingBuffer[idx] = sample;
}

void SpectrumDataCollector::pushOutputSample(float sample)
{
    int idx = writeIndex.load(std::memory_order_relaxed);
    outputRingBuffer[idx] = sample;
    writeIndex.store((idx + 1) % fftSize, std::memory_order_relaxed);
}

std::vector<float> SpectrumDataCollector::getInputSpectrum()
{
    return computeSpectrum(inputRingBuffer);
}

std::vector<float> SpectrumDataCollector::getOutputSpectrum()
{
    return computeSpectrum(outputRingBuffer);
}

std::vector<float> SpectrumDataCollector::computeSpectrum(const std::array<float, fftSize>& ringBuffer)
{
    // Copy ring buffer starting from current write position (oldest sample)
    std::array<float, fftSize * 2> fftData{};
    int readIdx = writeIndex.load(std::memory_order_relaxed);
    for (int i = 0; i < fftSize; ++i)
    {
        fftData[i] = ringBuffer[(readIdx + i) % fftSize];
    }

    // Apply window
    window.multiplyWithWindowingTable(fftData.data(), fftSize);

    // Perform FFT
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    // Convert to dB
    std::vector<float> spectrum(fftSize / 2);
    for (int i = 0; i < fftSize / 2; ++i)
    {
        float magnitude = fftData[i];
        // Normalize and convert to dB, with floor at -100dB
        float db = magnitude > 0.0f ? 20.0f * std::log10(magnitude / static_cast<float>(fftSize)) : -100.0f;
        spectrum[i] = db;
    }

    return spectrum;
}
```

**Step 3: Commit**

```bash
git add Source/SpectrumDataCollector.h Source/SpectrumDataCollector.cpp
git commit -m "feat: add SpectrumDataCollector for FFT analysis"
```

---

## Task 2: Add FilterResponseCalculator Class

**Files:**
- Create: `Source/FilterResponseCalculator.h`
- Create: `Source/FilterResponseCalculator.cpp`
- Modify: `Source/Tests.cpp` (add tests)

**Step 1: Create header file**

```cpp
// Source/FilterResponseCalculator.h
#pragma once

#include "ChannelEQ.h"
#include <vector>

class FilterResponseCalculator
{
public:
    // Calculate combined frequency response for all bands in a channel
    // Returns dB values at logarithmically-spaced frequencies from 20Hz to 20kHz
    static std::vector<float> calculateResponse(
        const ChannelEQ& channel,
        double sampleRate,
        int numPoints = 200
    );

    // Calculate magnitude response of a single biquad filter at given frequency
    static float calculateBiquadMagnitude(
        float frequency,
        float centerFreq,
        float gainDB,
        float q,
        FilterType type,
        double sampleRate
    );
};
```

**Step 2: Create implementation file**

```cpp
// Source/FilterResponseCalculator.cpp
#include "FilterResponseCalculator.h"
#include <cmath>

std::vector<float> FilterResponseCalculator::calculateResponse(
    const ChannelEQ& channel,
    double sampleRate,
    int numPoints)
{
    std::vector<float> response(numPoints);

    // Logarithmic frequency spacing from 20Hz to 20kHz
    const float minFreq = 20.0f;
    const float maxFreq = 20000.0f;
    const float logMin = std::log10(minFreq);
    const float logMax = std::log10(maxFreq);

    for (int i = 0; i < numPoints; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(numPoints - 1);
        float freq = std::pow(10.0f, logMin + t * (logMax - logMin));

        // Sum contributions from all active bands (in dB)
        float totalDB = 0.0f;
        for (int b = 0; b < NUM_EQ_BANDS; ++b)
        {
            const auto& band = channel.getBand(b);
            if (!band.isBypassed())
            {
                totalDB += calculateBiquadMagnitude(
                    freq,
                    band.getFrequency(),
                    band.getGain(),
                    band.getQ(),
                    band.getType(),
                    sampleRate
                );
            }
        }
        response[i] = totalDB;
    }

    return response;
}

float FilterResponseCalculator::calculateBiquadMagnitude(
    float frequency,
    float centerFreq,
    float gainDB,
    float q,
    FilterType type,
    double sampleRate)
{
    // Digital filter frequency response calculation
    // Using bilinear transform analysis
    const double pi = juce::MathConstants<double>::pi;
    const double w0 = 2.0 * pi * centerFreq / sampleRate;
    const double w = 2.0 * pi * frequency / sampleRate;

    const double A = std::pow(10.0, gainDB / 40.0);  // sqrt of linear gain
    const double alpha = std::sin(w0) / (2.0 * q);

    double b0, b1, b2, a0, a1, a2;

    switch (type)
    {
        case FilterType::Peak:
        {
            b0 = 1.0 + alpha * A;
            b1 = -2.0 * std::cos(w0);
            b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A;
            a1 = -2.0 * std::cos(w0);
            a2 = 1.0 - alpha / A;
            break;
        }
        case FilterType::LowShelf:
        {
            const double sqrtA = std::sqrt(A);
            const double sqrtA2alpha = 2.0 * sqrtA * alpha;
            b0 = A * ((A + 1.0) - (A - 1.0) * std::cos(w0) + sqrtA2alpha);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * std::cos(w0));
            b2 = A * ((A + 1.0) - (A - 1.0) * std::cos(w0) - sqrtA2alpha);
            a0 = (A + 1.0) + (A - 1.0) * std::cos(w0) + sqrtA2alpha;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * std::cos(w0));
            a2 = (A + 1.0) + (A - 1.0) * std::cos(w0) - sqrtA2alpha;
            break;
        }
        case FilterType::HighShelf:
        {
            const double sqrtA = std::sqrt(A);
            const double sqrtA2alpha = 2.0 * sqrtA * alpha;
            b0 = A * ((A + 1.0) + (A - 1.0) * std::cos(w0) + sqrtA2alpha);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * std::cos(w0));
            b2 = A * ((A + 1.0) + (A - 1.0) * std::cos(w0) - sqrtA2alpha);
            a0 = (A + 1.0) - (A - 1.0) * std::cos(w0) + sqrtA2alpha;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * std::cos(w0));
            a2 = (A + 1.0) - (A - 1.0) * std::cos(w0) - sqrtA2alpha;
            break;
        }
        default:
            return 0.0f;
    }

    // Normalize coefficients
    b0 /= a0; b1 /= a0; b2 /= a0;
    a1 /= a0; a2 /= a0;

    // Calculate frequency response H(e^jw)
    // H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
    // At z = e^jw
    const double cosW = std::cos(w);
    const double sinW = std::sin(w);
    const double cos2W = std::cos(2.0 * w);
    const double sin2W = std::sin(2.0 * w);

    // Numerator: b0 + b1*e^-jw + b2*e^-2jw
    const double numReal = b0 + b1 * cosW + b2 * cos2W;
    const double numImag = -b1 * sinW - b2 * sin2W;

    // Denominator: 1 + a1*e^-jw + a2*e^-2jw
    const double denReal = 1.0 + a1 * cosW + a2 * cos2W;
    const double denImag = -a1 * sinW - a2 * sin2W;

    // |H| = |num| / |den|
    const double numMag = std::sqrt(numReal * numReal + numImag * numImag);
    const double denMag = std::sqrt(denReal * denReal + denImag * denImag);

    if (denMag < 1e-10)
        return 0.0f;

    const double magnitude = numMag / denMag;
    return static_cast<float>(20.0 * std::log10(magnitude));
}
```

**Step 3: Add test to Tests.cpp**

Add after the ChannelEQ tests section (around line 313):

```cpp
//==============================================================================
// FilterResponseCalculator Tests
//==============================================================================

TEST(filter_response_flat_when_bypassed)
{
    ChannelEQ eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1;
    eq.prepare(spec);

    // All bands bypassed by default
    auto response = FilterResponseCalculator::calculateResponse(eq, 44100.0, 100);

    // Response should be 0dB everywhere
    for (float db : response)
    {
        ASSERT_NEAR(db, 0.0f, 0.01f);
    }
}

TEST(filter_response_peak_at_center)
{
    ChannelEQ eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1;
    eq.prepare(spec);

    eq.getBand(0).setFrequency(1000.0f);
    eq.getBand(0).setGain(6.0f);
    eq.getBand(0).setQ(2.0f);
    eq.getBand(0).setType(FilterType::Peak);
    eq.getBand(0).setBypassed(false);

    auto response = FilterResponseCalculator::calculateResponse(eq, 44100.0, 200);

    // Find the point closest to 1kHz (log scale)
    // 1kHz is at t = (log10(1000) - log10(20)) / (log10(20000) - log10(20))
    // = (3 - 1.301) / (4.301 - 1.301) = 1.699 / 3 = 0.566
    int idx1k = static_cast<int>(0.566f * 199);

    // Response at 1kHz should be close to +6dB
    ASSERT(response[idx1k] > 5.0f);
    ASSERT(response[idx1k] < 7.0f);

    // Response far from center should be near 0dB
    ASSERT(std::abs(response[0]) < 1.0f);   // 20Hz
    ASSERT(std::abs(response[199]) < 1.0f); // 20kHz
}
```

Also add include at top:

```cpp
#include "FilterResponseCalculator.h"
```

And add RUN_TEST calls in main():

```cpp
    // FilterResponseCalculator tests
    RUN_TEST(filter_response_flat_when_bypassed);
    RUN_TEST(filter_response_peak_at_center);
```

**Step 4: Update CMakeLists.txt to include new files in test target**

Add `Source/FilterResponseCalculator.cpp` to the test executable sources (around line 103-108).

**Step 5: Build and run tests**

```bash
cmake --build build -j --target RoomMultiEQ_Tests
./build/RoomMultiEQ_Tests
```

Expected: All tests pass including new FilterResponseCalculator tests.

**Step 6: Commit**

```bash
git add Source/FilterResponseCalculator.h Source/FilterResponseCalculator.cpp Source/Tests.cpp CMakeLists.txt
git commit -m "feat: add FilterResponseCalculator with biquad magnitude calculation"
```

---

## Task 3: Add SpectrumAnalyzer UI Component

**Files:**
- Create: `Source/SpectrumAnalyzer.h`
- Create: `Source/SpectrumAnalyzer.cpp`

**Step 1: Create header file**

```cpp
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
    static constexpr float minDB = -24.0f;
    static constexpr float maxDB = 12.0f;

    // Smoothing factor (0 = no smoothing, 1 = infinite smoothing)
    static constexpr float smoothingFactor = 0.7f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
```

**Step 2: Create implementation file**

```cpp
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
```

**Step 3: Commit**

```bash
git add Source/SpectrumAnalyzer.h Source/SpectrumAnalyzer.cpp
git commit -m "feat: add SpectrumAnalyzer UI component with Dracula theme"
```

---

## Task 4: Integrate Into Processor

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Update PluginProcessor.h**

Add include after existing includes (line 5):

```cpp
#include "SpectrumDataCollector.h"
```

Add public methods after `getRightChannel()` (around line 46):

```cpp
    SpectrumDataCollector& getLeftSpectrumCollector() { return leftSpectrumCollector; }
    SpectrumDataCollector& getRightSpectrumCollector() { return rightSpectrumCollector; }
    double getCurrentSampleRate() const { return currentSampleRate; }
```

Add private members after `rightChannel` (around line 63):

```cpp
    SpectrumDataCollector leftSpectrumCollector;
    SpectrumDataCollector rightSpectrumCollector;
    double currentSampleRate = 44100.0;
```

**Step 2: Update PluginProcessor.cpp**

In `prepareToPlay()` (around line 183), add after the existing spec setup:

```cpp
    currentSampleRate = sampleRate;
```

In `processBlock()` (around line 218), modify the processing loop to capture samples:

Replace the existing loop (lines 230-234):
```cpp
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        leftChannel.processSample(leftData[i]);
        rightChannel.processSample(rightData[i]);
    }
```

With:
```cpp
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        // Capture input
        leftSpectrumCollector.pushInputSample(leftData[i]);
        rightSpectrumCollector.pushInputSample(rightData[i]);

        // Process EQ
        leftChannel.processSample(leftData[i]);
        rightChannel.processSample(rightData[i]);

        // Capture output
        leftSpectrumCollector.pushOutputSample(leftData[i]);
        rightSpectrumCollector.pushOutputSample(rightData[i]);
    }
```

**Step 3: Update CMakeLists.txt**

Add new source files to plugin target (around line 63, after FilterFileParser.h):

```cmake
        Source/SpectrumDataCollector.cpp
        Source/SpectrumDataCollector.h
        Source/FilterResponseCalculator.cpp
        Source/FilterResponseCalculator.h
        Source/SpectrumAnalyzer.cpp
        Source/SpectrumAnalyzer.h
```

**Step 4: Build and verify**

```bash
cmake --build build -j
```

Expected: Build succeeds.

**Step 5: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp CMakeLists.txt
git commit -m "feat: integrate spectrum collectors into audio processor"
```

---

## Task 5: Integrate Into Editor

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

**Step 1: Update PluginEditor.h**

Add include after existing includes (line 4):

```cpp
#include "SpectrumAnalyzer.h"
```

Add to `ChannelEQComponent` private members (around line 42):

```cpp
    std::unique_ptr<SpectrumAnalyzer> spectrumAnalyzer;
    double& sampleRateRef;
```

Update `ChannelEQComponent` constructor signature (line 10):

```cpp
    ChannelEQComponent(RoomMultiEQAudioProcessor& processor, bool isLeft, SpectrumDataCollector& collector, double& sampleRate);
```

Add to `RoomMultiEQAudioProcessorEditor` private members (around line 63):

```cpp
    juce::TextButton showTablesButton;
    bool tablesVisible = false;
    double sampleRate = 44100.0;
```

**Step 2: Update PluginEditor.cpp**

This is a substantial modification. Update the `ChannelEQComponent` constructor (around line 118):

```cpp
ChannelEQComponent::ChannelEQComponent(RoomMultiEQAudioProcessor& p, bool isLeft, SpectrumDataCollector& collector, double& sr)
    : processor(p),
      isLeftChannel(isLeft),
      channelPrefix(isLeft ? "left" : "right"),
      sampleRateRef(sr)
{
    // Create spectrum analyzer
    spectrumAnalyzer = std::make_unique<SpectrumAnalyzer>(
        collector,
        isLeft ? p.getLeftChannel() : p.getRightChannel(),
        sampleRateRef
    );
    addAndMakeVisible(*spectrumAnalyzer);
    spectrumAnalyzer->startAnalysis();

    // ... rest of existing constructor code
```

Update `ChannelEQComponent::resized()` (around line 166) to conditionally show table:

```cpp
void ChannelEQComponent::resized()
{
    auto bounds = getLocalBounds().reduced(5);

    // Title area
    bounds.removeFromTop(30);
    bounds.removeFromTop(5);

    // Buttons at bottom
    auto buttonArea = bounds.removeFromBottom(30);
    importButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth() / 2).reduced(2));
    clearButton.setBounds(buttonArea.reduced(2));

    bounds.removeFromBottom(5);

    // Check if table should be visible
    auto* editor = findParentComponentOfClass<RoomMultiEQAudioProcessorEditor>();
    bool showTable = editor != nullptr && editor->areTablesVisible();

    if (showTable)
    {
        // Split: 40% spectrum, 60% table
        int spectrumHeight = static_cast<int>(bounds.getHeight() * 0.4f);
        spectrumAnalyzer->setBounds(bounds.removeFromTop(spectrumHeight));
        bounds.removeFromTop(5);
        table.setBounds(bounds);
        table.setVisible(true);
    }
    else
    {
        // Full spectrum
        spectrumAnalyzer->setBounds(bounds);
        table.setVisible(false);
    }
}
```

Add a public method to `ChannelEQComponent`:

```cpp
void ChannelEQComponent::updateLayout()
{
    resized();
}
```

Update `RoomMultiEQAudioProcessorEditor` constructor (around line 325):

```cpp
RoomMultiEQAudioProcessorEditor::RoomMultiEQAudioProcessorEditor(RoomMultiEQAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      leftChannelComponent(p, true, p.getLeftSpectrumCollector(), sampleRate),
      rightChannelComponent(p, false, p.getRightSpectrumCollector(), sampleRate)
{
    sampleRate = p.getCurrentSampleRate();

    // Show Tables button
    showTablesButton.setButtonText("Show Tables");
    showTablesButton.onClick = [this]()
    {
        tablesVisible = !tablesVisible;
        showTablesButton.setButtonText(tablesVisible ? "Hide Tables" : "Show Tables");
        leftChannelComponent.updateLayout();
        rightChannelComponent.updateLayout();
    };
    addAndMakeVisible(showTablesButton);

    // ... rest of existing constructor
```

Add public method to `RoomMultiEQAudioProcessorEditor`:

```cpp
bool RoomMultiEQAudioProcessorEditor::areTablesVisible() const
{
    return tablesVisible;
}
```

Update `RoomMultiEQAudioProcessorEditor::resized()` (around line 359):

```cpp
void RoomMultiEQAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Header area
    auto headerArea = bounds.removeFromTop(40);
    headerArea.removeFromLeft(10);

    // Title on left
    // (existing title drawing in paint())

    // Buttons on right
    auto buttonArea = headerArea.removeFromRight(200);
    masterBypassButton.setBounds(buttonArea.removeFromRight(90).reduced(5));
    showTablesButton.setBounds(buttonArea.removeFromRight(100).reduced(5));

    // Split remaining for channels
    auto channelWidth = bounds.getWidth() / 2;
    leftChannelComponent.setBounds(bounds.removeFromLeft(channelWidth));
    rightChannelComponent.setBounds(bounds);
}
```

Also add the method declarations to the header:

In `ChannelEQComponent` public section:
```cpp
    void updateLayout();
```

In `RoomMultiEQAudioProcessorEditor` public section:
```cpp
    bool areTablesVisible() const;
```

**Step 3: Build and test manually**

```bash
cmake --build build -j
```

Open the plugin in a DAW to verify:
1. Spectrum analyzer shows for both channels
2. Toggle button switches between graph-only and graph+table views
3. Filter curve responds to parameter changes

**Step 4: Commit**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "feat: integrate spectrum analyzer into editor with toggle"
```

---

## Task 6: Update Screenshot Tool (Optional)

**Files:**
- Modify: `CMakeLists.txt`

**Step 1: Add new files to screenshot target**

In CMakeLists.txt, update the RoomMultiEQ_Screenshot sources (around line 136-143):

```cmake
    add_executable(RoomMultiEQ_Screenshot
        Source/Screenshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/EQBand.cpp
        Source/ChannelEQ.cpp
        Source/FilterFileParser.cpp
        Source/SpectrumDataCollector.cpp
        Source/FilterResponseCalculator.cpp
        Source/SpectrumAnalyzer.cpp
    )
```

**Step 2: Build and verify**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DROOMMULTIEQ_BUILD_SCREENSHOT=ON
cmake --build build -j --target RoomMultiEQ_Screenshot
./build/RoomMultiEQ_Screenshot
```

**Step 3: Commit**

```bash
git add CMakeLists.txt
git commit -m "chore: add spectrum analyzer files to screenshot tool"
```

---

## Task 7: Final Testing and Cleanup

**Step 1: Run all tests**

```bash
cmake --build build -j --target RoomMultiEQ_Tests
./build/RoomMultiEQ_Tests
```

Expected: All tests pass.

**Step 2: Build plugin**

```bash
cmake --build build -j
```

**Step 3: Test in DAW**

1. Load plugin in Logic Pro, Reaper, or another DAW
2. Verify spectrum displays real-time audio
3. Verify filter curve updates when changing parameters
4. Verify toggle button shows/hides tables
5. Verify CPU usage is reasonable

**Step 4: Final commit**

```bash
git add -A
git commit -m "feat: complete spectrum analyzer implementation"
```

---

## Summary

| Task | Description | Files |
|------|-------------|-------|
| 1 | SpectrumDataCollector | New: SpectrumDataCollector.h/cpp |
| 2 | FilterResponseCalculator + tests | New: FilterResponseCalculator.h/cpp, Modify: Tests.cpp, CMakeLists.txt |
| 3 | SpectrumAnalyzer UI | New: SpectrumAnalyzer.h/cpp |
| 4 | Processor integration | Modify: PluginProcessor.h/cpp, CMakeLists.txt |
| 5 | Editor integration | Modify: PluginEditor.h/cpp |
| 6 | Screenshot tool update | Modify: CMakeLists.txt |
| 7 | Testing and cleanup | - |

Total new files: 6
Total modified files: 5
