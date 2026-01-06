#include "EQBand.h"
#include <cmath>

EQBand::EQBand()
{
}

void EQBand::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    filter.prepare(spec);
    updateCoefficients();
}

void EQBand::reset()
{
    filter.reset();
}

void EQBand::setFrequency(float freq)
{
    freq = juce::jlimit(20.0f, 20000.0f, freq);
    if (std::abs(params.frequency - freq) > 1e-6f)
    {
        params.frequency = freq;
        needsUpdate = true;
    }
}

void EQBand::setGain(float gainDB)
{
    gainDB = juce::jlimit(-20.0f, 20.0f, gainDB);
    if (std::abs(params.gainDB - gainDB) > 1e-6f)
    {
        params.gainDB = gainDB;
        needsUpdate = true;
    }
}

void EQBand::setQ(float q)
{
    q = juce::jlimit(0.1f, 30.0f, q);
    if (std::abs(params.q - q) > 1e-6f)
    {
        params.q = q;
        needsUpdate = true;
    }
}

void EQBand::setType(FilterType type)
{
    if (params.type != type)
    {
        params.type = type;
        needsUpdate = true;
    }
}

void EQBand::setBypassed(bool bypassed)
{
    params.bypassed = bypassed;
}

void EQBand::processSample(float& sample)
{
    if (params.bypassed)
        return;

    if (needsUpdate)
    {
        updateCoefficients();
        needsUpdate = false;
    }

    sample = filter.processSample(sample);
}

void EQBand::updateCoefficients()
{
    juce::dsp::IIR::Coefficients<float>::Ptr coeffs;
    float gain = juce::Decibels::decibelsToGain(params.gainDB);

    switch (params.type)
    {
        case FilterType::Peak:
            coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sampleRate, params.frequency, params.q, gain);
            break;

        case FilterType::LowShelf:
            coeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                sampleRate, params.frequency, params.q, gain);
            break;

        case FilterType::HighShelf:
            coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                sampleRate, params.frequency, params.q, gain);
            break;
    }

    *filter.coefficients = *coeffs;
}
