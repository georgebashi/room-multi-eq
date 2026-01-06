#pragma once

#include <juce_core/juce_core.h>
#include "EQBand.h"
#include <vector>

struct ParsedFilter
{
    bool enabled = false;
    FilterType type = FilterType::Peak;
    float frequency = 1000.0f;
    float gainDB = 0.0f;
    float q = 1.0f;
};

class FilterFileParser
{
public:
    static std::vector<ParsedFilter> parseFile(const juce::File& file);
    static std::vector<ParsedFilter> parseString(const juce::String& content);

private:
    static ParsedFilter parseLine(const juce::String& line);
    static FilterType parseFilterType(const juce::String& typeStr);
};
