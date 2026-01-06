// Source/FilterResponseCalculator.cpp
#include "FilterResponseCalculator.h"
#include <juce_core/juce_core.h>
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
