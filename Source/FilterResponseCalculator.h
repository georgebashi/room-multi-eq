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
