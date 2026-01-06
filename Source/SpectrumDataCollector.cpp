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

std::vector<float> SpectrumDataCollector::getInputSpectrum(double sampleRate)
{
    auto spectrum = computeSpectrum(inputRingBuffer);
    applyPsychoacousticSmoothing(spectrum, sampleRate);
    return spectrum;
}

std::vector<float> SpectrumDataCollector::getOutputSpectrum(double sampleRate)
{
    auto spectrum = computeSpectrum(outputRingBuffer);
    applyPsychoacousticSmoothing(spectrum, sampleRate);
    return spectrum;
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
    // performFrequencyOnlyForwardTransform returns magnitudes scaled by fftSize/2
    // We want 0dB to represent a full-scale sine wave, which has magnitude fftSize/2 at its bin
    std::vector<float> spectrum(fftSize / 2);
    const float referenceLevel = static_cast<float>(fftSize) / 2.0f;
    for (int i = 0; i < fftSize / 2; ++i)
    {
        float magnitude = fftData[i];
        // Convert to dB relative to full-scale, with floor at -100dB
        float db = magnitude > 0.0f ? 20.0f * std::log10(magnitude / referenceLevel) : -100.0f;
        spectrum[i] = db;
    }

    return spectrum;
}

void SpectrumDataCollector::applyPsychoacousticSmoothing(std::vector<float>& spectrum, double sampleRate)
{
    if (spectrum.empty() || sampleRate <= 0.0)
        return;

    const float binWidth = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
    std::vector<float> smoothed(spectrum.size());

    for (size_t i = 1; i < spectrum.size(); ++i)
    {
        float centerFreq = static_cast<float>(i) * binWidth;
        if (centerFreq < 20.0f)
        {
            smoothed[i] = spectrum[i];
            continue;
        }

        // Octave fraction varies from 1/48 at low freq to 1/6 at high freq
        // Linear interpolation in log frequency space from 20Hz to 20kHz
        float logFreq = std::log10(centerFreq);
        float t = (logFreq - std::log10(20.0f)) / (std::log10(20000.0f) - std::log10(20.0f));
        t = std::clamp(t, 0.0f, 1.0f);

        // 1/48 octave = 0.0208, 1/6 octave = 0.167
        float octaveFraction = 0.0208f + t * (0.167f - 0.0208f);

        // Calculate smoothing bandwidth in Hz
        // bandwidth = freq * (2^(oct/2) - 2^(-oct/2))
        float halfOct = octaveFraction / 2.0f;
        float bandwidth = centerFreq * (std::pow(2.0f, halfOct) - std::pow(2.0f, -halfOct));

        // Find bins within bandwidth and average them
        int halfBins = static_cast<int>(std::ceil(bandwidth / (2.0f * binWidth)));
        halfBins = std::max(halfBins, 1);

        float sum = 0.0f;
        float weightSum = 0.0f;
        int startBin = std::max(1, static_cast<int>(i) - halfBins);
        int endBin = std::min(static_cast<int>(spectrum.size()) - 1, static_cast<int>(i) + halfBins);

        for (int j = startBin; j <= endBin; ++j)
        {
            // Triangular weighting
            float dist = std::abs(static_cast<float>(j - static_cast<int>(i)));
            float weight = 1.0f - (dist / static_cast<float>(halfBins + 1));

            // Convert from dB to linear, average, then back to dB
            float linear = std::pow(10.0f, spectrum[j] / 20.0f);
            sum += linear * weight;
            weightSum += weight;
        }

        if (weightSum > 0.0f)
        {
            float avgLinear = sum / weightSum;
            smoothed[i] = 20.0f * std::log10(std::max(avgLinear, 1e-10f));
        }
        else
        {
            smoothed[i] = spectrum[i];
        }
    }

    smoothed[0] = spectrum[0];
    spectrum = std::move(smoothed);
}
