# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Room Multi EQ is a macOS AudioUnit plugin that applies independent parametric EQ to multiple channels (up to 24), designed for loading Room EQ Wizard (REW) filter profiles. Built with JUCE framework.

## Build Commands

```bash
# Configure (first time or after CMakeLists.txt changes)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build plugin (use all CPU cores)
cmake --build build -j

# Build and run tests
cmake --build build -j --target RoomMultiEQ_Tests
./build/RoomMultiEQ_Tests

# Validate AudioUnit
auval -a    # List all AudioUnits
auval -v aufx Rmeq Gbsh   # Validate this plugin

# Build screenshot tool (for documentation, not built by default)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DROOMMULTIEQ_BUILD_SCREENSHOT=ON
cmake --build build -j --target RoomMultiEQ_Screenshot
./build/RoomMultiEQ_Screenshot

# Build AU and restart SoundSource to test changes
cd build && cmake --build . --target RoomMultiEQ_AU && osascript -e 'quit app "SoundSource"'; killall -9 AudioComponentRegistrar 2>/dev/null; sleep 1; open -a SoundSource
```

The plugin is automatically copied to `~/Library/Audio/Plug-Ins/Components/` after build (COPY_PLUGIN_AFTER_BUILD is enabled).

**Important:** When iterating on plugin changes, always use the "Build AU and restart SoundSource" command to test. This ensures the AudioUnit cache is cleared and SoundSource picks up the new version.

## Architecture

### Core Classes

- **RoomMultiEQAudioProcessor** (`PluginProcessor.h/cpp`) - Main audio processor using AudioProcessorValueTreeState for parameter management. Routes each channel to its own ChannelEQ instance.

- **ChannelEQ** (`ChannelEQ.h/cpp`) - Contains 16 EQBand instances for a single channel. Processes samples through the filter chain.

- **EQBand** (`EQBand.h/cpp`) - Single parametric EQ band wrapping `juce::dsp::IIR::Filter<float>`. Supports Peak, LowShelf, and HighShelf filter types.

- **FilterFileParser** (`FilterFileParser.h/cpp`) - Parses REW filter export format (text files with lines like `Filter  1: ON  PK  Fc 63.0 Hz  Gain -5.2 dB  Q 4.32`).

- **RoomMultiEQAudioProcessorEditor** (`PluginEditor.h/cpp`) - UI with per-channel filter tables and import buttons.

### Parameter Naming Convention

Parameters follow the pattern: `ch{0-23}_band_{1-16}_{freq|gain|q|type|bypass}`
Plus `ch{0-23}_bypass` for per-channel bypass.

### Filter Types

```cpp
enum class FilterType { Peak, LowShelf, HighShelf };
```

Maps to REW format: `PK` → Peak, `LS` → LowShelf, `HS` → HighShelf

## Key Specifications

- 16 bands per channel (up to 24 channels)
- Frequency: 20 Hz - 20,000 Hz
- Gain: -20 dB to +20 dB
- Q: 0.1 to 30
- macOS 12+ (Universal binary: arm64 + x86_64)
- Zero latency (IIR filters)
