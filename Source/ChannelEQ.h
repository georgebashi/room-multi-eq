#pragma once

#include "EQBand.h"
#include <array>

constexpr int NUM_EQ_BANDS = 16;

class ChannelEQ
{
public:
    ChannelEQ();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void processSample(float& sample);

    EQBand& getBand(int index);
    const EQBand& getBand(int index) const;

private:
    std::array<EQBand, NUM_EQ_BANDS> bands;
};
