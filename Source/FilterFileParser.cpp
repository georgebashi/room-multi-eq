#include "FilterFileParser.h"

std::vector<ParsedFilter> FilterFileParser::parseFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return {};

    return parseString(file.loadFileAsString());
}

std::vector<ParsedFilter> FilterFileParser::parseString(const juce::String& content)
{
    std::vector<ParsedFilter> filters;

    juce::StringArray lines;
    lines.addLines(content);

    for (const auto& line : lines)
    {
        if (line.trim().startsWithIgnoreCase("Filter"))
        {
            auto filter = parseLine(line);
            if (filter.frequency > 0)
                filters.push_back(filter);
        }
    }

    return filters;
}

ParsedFilter FilterFileParser::parseLine(const juce::String& line)
{
    ParsedFilter filter;

    // Format: "Filter  N: ON/OFF  TYPE  Fc XXX Hz  Gain XXX dB  Q XXX"
    // Example: "Filter  1: ON  PK       Fc    63.0 Hz  Gain  -5.2 dB  Q  4.32"
    // Also handles: "Filter  3: ON  LS       Fc   64.00 Hz  Gain    5.7 dB" (no Q for shelves)
    // Skip: "Filter 11: ON  None"

    // Check ON/OFF
    filter.enabled = line.containsIgnoreCase(" ON ");

    // Skip "None" filter type (empty/unused filters in REW)
    if (line.containsIgnoreCase(" None"))
    {
        filter.frequency = 0.0f;  // Signal to skip this filter
        return filter;
    }

    // Parse filter type
    if (line.containsIgnoreCase(" PK "))
        filter.type = FilterType::Peak;
    else if (line.containsIgnoreCase(" LS "))
        filter.type = FilterType::LowShelf;
    else if (line.containsIgnoreCase(" HS "))
        filter.type = FilterType::HighShelf;
    else if (line.containsIgnoreCase(" LP ") || line.containsIgnoreCase(" HP "))
    {
        filter.frequency = 0.0f;  // Signal to skip this filter
        return filter;
    }
    else
    {
        // Unknown filter type - skip
        filter.frequency = 0.0f;
        return filter;
    }

    // Parse frequency (Fc XXX Hz)
    int fcIndex = line.indexOfIgnoreCase("Fc");
    int hzIndex = line.indexOfIgnoreCase("Hz");
    if (fcIndex >= 0 && hzIndex > fcIndex)
    {
        auto freqStr = line.substring(fcIndex + 2, hzIndex).trim();
        filter.frequency = freqStr.getFloatValue();
    }

    // Parse gain (Gain XXX dB)
    int gainIndex = line.indexOfIgnoreCase("Gain");
    int dbIndex = line.indexOfIgnoreCase("dB");
    if (gainIndex >= 0 && dbIndex > gainIndex)
    {
        auto gainStr = line.substring(gainIndex + 4, dbIndex).trim();
        filter.gainDB = gainStr.getFloatValue();
    }

    // Parse Q (Q XXX) - optional for shelf filters
    int qIndex = line.lastIndexOfIgnoreCase(" Q ");
    if (qIndex >= 0)
    {
        auto qStr = line.substring(qIndex + 3).trim();
        float qVal = qStr.getFloatValue();
        if (qVal > 0.0f)
            filter.q = qVal;
        // else keep default Q of 1.0 (suitable for shelves)
    }
    // For shelf filters without Q, default of 0.71 is typical
    else if (filter.type == FilterType::LowShelf || filter.type == FilterType::HighShelf)
    {
        filter.q = 0.71f;
    }

    return filter;
}

FilterType FilterFileParser::parseFilterType(const juce::String& typeStr)
{
    if (typeStr.equalsIgnoreCase("PK"))
        return FilterType::Peak;
    if (typeStr.equalsIgnoreCase("LS"))
        return FilterType::LowShelf;
    if (typeStr.equalsIgnoreCase("HS"))
        return FilterType::HighShelf;

    return FilterType::Peak;
}
