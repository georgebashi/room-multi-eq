// Source/ChannelColors.h
#pragma once

#include <juce_graphics/juce_graphics.h>
#include <array>

// Shared visualization constants
namespace VisualizationConstants
{
    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr juce::uint32 colBackground = 0xff282a36;  // Dracula background
    static constexpr juce::uint32 colGridLine = 0xff44475a;
    static constexpr juce::uint32 colText = 0xfff8f8f2;
}

namespace ChannelColors
{
    // Dracula theme colors for channels
    static constexpr std::array<juce::uint32, 8> palette = {{
        0xff8be9fd,  // 0: Cyan - Front Left
        0xff50fa7b,  // 1: Green - Front Right
        0xffbd93f9,  // 2: Purple - Center
        0xffffb86c,  // 3: Orange - LFE
        0xffff79c6,  // 4: Pink - Left Surround
        0xfff1fa8c,  // 5: Yellow - Right Surround
        0xffff5555,  // 6: Red - Additional
        0xfff8f8f2   // 7: Foreground - Additional
    }};

    inline juce::Colour getChannelColor(int channelIndex)
    {
        // Cycle through palette, with brightness variation for channels > 8
        int paletteIndex = channelIndex % 8;
        juce::Colour base(palette[static_cast<size_t>(paletteIndex)]);

        // For channels 8-15, slightly darker; 16-23, slightly lighter
        int cycle = channelIndex / 8;
        if (cycle == 1)
            return base.darker(0.2f);
        else if (cycle >= 2)
            return base.brighter(0.2f);

        return base;
    }
}
