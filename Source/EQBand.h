#pragma once

#include <juce_dsp/juce_dsp.h>

enum class FilterType
{
    Peak,
    LowShelf,
    HighShelf
};

struct EQBandParams
{
    float frequency = 1000.0f;  // 20 Hz - 20000 Hz
    float gainDB = 0.0f;        // -20 dB to +20 dB
    float q = 1.0f;             // 0.1 to 30
    FilterType type = FilterType::Peak;
    bool bypassed = true;
};

class EQBand
{
public:
    EQBand();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void setFrequency(float freq);
    void setGain(float gainDB);
    void setQ(float q);
    void setType(FilterType type);
    void setBypassed(bool bypassed);

    float getFrequency() const { return params.frequency; }
    float getGain() const { return params.gainDB; }
    float getQ() const { return params.q; }
    FilterType getType() const { return params.type; }
    bool isBypassed() const { return params.bypassed; }

    void processSample(float& sample);

private:
    void updateCoefficients();

    EQBandParams params;
    juce::dsp::IIR::Filter<float> filter;
    double sampleRate = 44100.0;
    bool needsUpdate = true;
};
