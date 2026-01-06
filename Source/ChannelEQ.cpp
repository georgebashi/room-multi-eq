#include "ChannelEQ.h"

ChannelEQ::ChannelEQ()
{
}

void ChannelEQ::prepare(const juce::dsp::ProcessSpec& spec)
{
    for (auto& band : bands)
        band.prepare(spec);
}

void ChannelEQ::reset()
{
    for (auto& band : bands)
        band.reset();
}

void ChannelEQ::processSample(float& sample)
{
    for (auto& band : bands)
        band.processSample(sample);
}

EQBand& ChannelEQ::getBand(int index)
{
    jassert(index >= 0 && index < NUM_EQ_BANDS);
    return bands[static_cast<size_t>(index)];
}

const EQBand& ChannelEQ::getBand(int index) const
{
    jassert(index >= 0 && index < NUM_EQ_BANDS);
    return bands[static_cast<size_t>(index)];
}
