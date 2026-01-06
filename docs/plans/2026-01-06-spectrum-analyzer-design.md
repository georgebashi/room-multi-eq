# Spectrum Analyzer Design

## Overview

Add real-time spectrum visualization to the Room Multi EQ plugin, showing input/output frequency content and filter response curves per channel.

## Requirements

- One spectrum graph per channel (left/right)
- Three visual elements per graph:
  1. **Input spectrum** - filled area showing pre-EQ frequency content
  2. **Output spectrum** - semi-transparent filled overlay showing post-EQ content
  3. **Filter response curve** - mathematical calculation of combined EQ effect
- Toggle between graph-only view (default) and graph+table view
- Dracula color theme
- 4096-point FFT, 20Hz-20kHz range, -24dB to +12dB vertical scale
- 30fps update rate with exponential smoothing
- CPU optimization: FFT/rendering disabled when plugin window is closed

## Architecture

### New Components

**SpectrumDataCollector** (lives in processor, one per channel)
- Ring buffers for input and output samples (4096 samples each)
- Lock-free write from audio thread (atomic index)
- FFT computation on GUI thread when requested
- Provides spectrum data as dB values

**FilterResponseCalculator** (stateless utility)
- Computes combined frequency response from all 16 bands
- Uses standard biquad magnitude formulas
- Returns dB values at logarithmically-spaced frequencies

**SpectrumAnalyzer** (UI component)
- Renders spectrum fills and filter curve
- Runs 30fps timer for updates
- Applies exponential smoothing to spectrum data
- Caches filter response (recalculates only on parameter change)

### Data Flow

```
Audio Thread (Processor)
  → Writes samples to ring buffers (pre and post EQ)
  → No FFT, no heavy computation

GUI Thread (Editor, 30fps timer)
  → Reads ring buffers
  → Performs windowed FFT
  → Applies smoothing
  → Calculates filter curve from parameters
  → Renders to screen
```

### CPU Optimization

| State | Ring Buffer | FFT | Rendering |
|-------|-------------|-----|-----------|
| Window closed | Active (cheap) | Disabled | Disabled |
| Window open | Active | Active | Active |

Ring buffers stay active when window is closed - they're cheap (just memory copies) and provide immediate data when window reopens.

## File Structure

```
Source/
├── SpectrumAnalyzer.h/cpp       # UI component for rendering
├── SpectrumDataCollector.h/cpp  # Ring buffers + FFT processing
├── FilterResponseCalculator.h/cpp # Biquad magnitude math
├── PluginProcessor.h/cpp        # Modified: add collectors, feed samples
└── PluginEditor.h/cpp           # Modified: add analyzers, toggle button
```

## Class Interfaces

```cpp
class SpectrumDataCollector {
public:
    void pushInputSample(float sample);
    void pushOutputSample(float sample);
    std::vector<float> getInputSpectrum();   // Returns dB values
    std::vector<float> getOutputSpectrum();  // Returns dB values

private:
    static constexpr int fftSize = 4096;
    juce::dsp::FFT fft{12};  // 2^12 = 4096
    std::array<float, fftSize> inputRingBuffer;
    std::array<float, fftSize> outputRingBuffer;
    std::atomic<int> writeIndex{0};
};

class FilterResponseCalculator {
public:
    static std::vector<float> calculateResponse(
        const ChannelEQ& channel,
        double sampleRate,
        int numPoints = 200
    );
};

class SpectrumAnalyzer : public juce::Component,
                          private juce::Timer {
public:
    SpectrumAnalyzer(SpectrumDataCollector& collector,
                     ChannelEQ& channel,
                     juce::AudioProcessorValueTreeState& apvts,
                     const juce::String& channelPrefix);

    void paint(juce::Graphics& g) override;
    void timerCallback() override;

private:
    std::vector<float> smoothedInput, smoothedOutput;
    std::vector<float> filterResponse;
};
```

## UI Layout

### Graph Mode (default)

```
┌─────────────────────────────────────────────────────┐
│  Room Multi EQ            [Show Tables] [⊘ Bypass]  │
├────────────────────────┬────────────────────────────┤
│      LEFT CHANNEL      │      RIGHT CHANNEL         │
│  ┌──────────────────┐  │  ┌──────────────────────┐  │
│  │   Spectrum +     │  │  │    Spectrum +        │  │
│  │   Filter Curve   │  │  │    Filter Curve      │  │
│  │  -24────────+12  │  │  │   -24────────+12     │  │
│  │  20Hz      20kHz │  │  │   20Hz       20kHz   │  │
│  └──────────────────┘  │  └──────────────────────┘  │
│     [Import] [Clear]   │     [Import] [Clear]       │
└────────────────────────┴────────────────────────────┘
```

### Table Mode

```
┌─────────────────────────────────────────────────────┐
│  Room Multi EQ            [Hide Tables] [⊘ Bypass]  │
├────────────────────────┬────────────────────────────┤
│      LEFT CHANNEL      │      RIGHT CHANNEL         │
│  ┌──────────────────┐  │  ┌──────────────────────┐  │
│  │ Spectrum (40%)   │  │  │ Spectrum (40%)       │  │
│  └──────────────────┘  │  └──────────────────────┘  │
│  ┌──────────────────┐  │  ┌──────────────────────┐  │
│  │ Filter Table     │  │  │ Filter Table         │  │
│  │ (60%, scroll)    │  │  │ (60%, scroll)        │  │
│  └──────────────────┘  │  └──────────────────────┘  │
│     [Import] [Clear]   │     [Import] [Clear]       │
└────────────────────────┴────────────────────────────┘
```

## Visual Design (Dracula Theme)

| Element | Color | Hex |
|---------|-------|-----|
| Background | Dracula Background | `#282a36` |
| Grid lines | Dracula Current Line | `#44475a` |
| Input spectrum | Dracula Comment | `#6272a4` |
| Output spectrum | Dracula Green (40% opacity) | `#50fa7b66` |
| Filter curve | Dracula Orange | `#ffb86c` |
| Axis labels | Dracula Foreground | `#f8f8f2` |
| Channel titles | Dracula Purple | `#bd93f9` |

### Grid

- Vertical lines at: 100Hz, 1kHz, 10kHz (logarithmic spacing)
- Horizontal lines at: -24, -18, -12, -6, 0, +6, +12 dB
- Frequency labels: "100", "1k", "10k"
- dB labels: "-24", "-12", "0", "+12"

### Spectrum Rendering

- FFT output converted to dB: `20 * log10(magnitude)`
- Exponential smoothing: `smoothed = 0.7 * new + 0.3 * old`
- Input: solid filled path from bottom
- Output: semi-transparent filled path, overlaid on input
- Visual difference shows EQ effect

### Filter Curve

- ~200 logarithmically-spaced frequency points
- Smooth path using `Path::lineTo()`
- 2px line weight, Dracula orange
- Drawn last (on top of spectrums)

## Deferred Features

**Measured filter response** (low priority)
- Derive filter response from actual input/output spectrum difference
- Shows "real" effect including non-linearities
- Requires audio playing to display
- Could overlay as subtle line alongside mathematical curve
