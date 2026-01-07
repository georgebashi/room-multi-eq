#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "ChannelEQ.h"
#include "SpectrumDataCollector.h"
#include <vector>
#include <memory>

class RoomMultiEQAudioProcessor : public juce::AudioProcessor,
                                  public juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr int MAX_CHANNELS = 24;

    RoomMultiEQAudioProcessor();
    ~RoomMultiEQAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // Multi-channel API
    int getNumChannels() const { return numChannels; }
    ChannelEQ& getChannel(int index) { return *channels[static_cast<size_t>(index)]; }
    const ChannelEQ& getChannel(int index) const { return *channels[static_cast<size_t>(index)]; }
    SpectrumDataCollector& getSpectrumCollector(int index) { return *spectrumCollectors[static_cast<size_t>(index)]; }
    const juce::String& getChannelName(int index) const { return channelNames[static_cast<size_t>(index)]; }
    const std::vector<juce::String>& getChannelNames() const { return channelNames; }
    double getCurrentSampleRate() const { return currentSampleRate; }

    // Legacy accessors for backward compatibility
    ChannelEQ& getLeftChannel() { return *channels[0]; }
    ChannelEQ& getRightChannel() { return *channels[static_cast<size_t>(std::min(1, numChannels - 1))]; }
    SpectrumDataCollector& getLeftSpectrumCollector() { return *spectrumCollectors[0]; }
    SpectrumDataCollector& getRightSpectrumCollector() { return *spectrumCollectors[static_cast<size_t>(std::min(1, numChannels - 1))]; }

    void loadFilterFile(int channelIndex, const juce::File& file);
    void resetBandToDefaults(int channelIndex, int band);

    // Parameter ID generation - new format: ch{0-23}_band_{1-16}_{param}
    static juce::String getParamID(int channelIndex, int band, const juce::String& param)
    {
        return "ch" + juce::String(channelIndex) + "_band_" + juce::String(band + 1) + "_" + param;
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateBandFromParameters(int channelIndex, int band);
    void initializeChannels(int count, const juce::AudioChannelSet& channelSet);

    juce::AudioProcessorValueTreeState apvts;

    std::vector<std::unique_ptr<ChannelEQ>> channels;
    std::vector<std::unique_ptr<SpectrumDataCollector>> spectrumCollectors;
    std::vector<juce::String> channelNames;
    int numChannels = 0;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoomMultiEQAudioProcessor)
};
