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

    // Returns spectrum in dB with psychoacoustic smoothing, size = fftSize/2
    std::vector<float> getInputSpectrum(double sampleRate);
    std::vector<float> getOutputSpectrum(double sampleRate);

private:
    std::vector<float> computeSpectrum(const std::array<float, fftSize>& ringBuffer);
    void applyPsychoacousticSmoothing(std::vector<float>& spectrum, double sampleRate);

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, fftSize> inputRingBuffer{};
    std::array<float, fftSize> outputRingBuffer{};
    std::atomic<int> writeIndex{0};
};
