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
