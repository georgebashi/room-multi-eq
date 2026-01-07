#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FilterFileParser.h"

RoomMultiEQAudioProcessor::RoomMultiEQAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Pre-create channels for stereo default
    initializeChannels(2, juce::AudioChannelSet::stereo());

    // Add parameter listeners for all possible channels
    for (int ch = 0; ch < MAX_CHANNELS; ++ch)
    {
        for (int b = 0; b < NUM_EQ_BANDS; ++b)
        {
            apvts.addParameterListener(getParamID(ch, b, "freq"), this);
            apvts.addParameterListener(getParamID(ch, b, "gain"), this);
            apvts.addParameterListener(getParamID(ch, b, "q"), this);
            apvts.addParameterListener(getParamID(ch, b, "type"), this);
            apvts.addParameterListener(getParamID(ch, b, "bypass"), this);
        }
    }
}

RoomMultiEQAudioProcessor::~RoomMultiEQAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout RoomMultiEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Per-channel, per-band parameters (pre-allocate for MAX_CHANNELS)
    for (int ch = 0; ch < MAX_CHANNELS; ++ch)
    {
        juce::String channelName = "Ch " + juce::String(ch + 1);

        for (int b = 0; b < NUM_EQ_BANDS; ++b)
        {
            juce::String bandNum = juce::String(b + 1);

            // Frequency
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(getParamID(ch, b, "freq"), 1),
                channelName + " Band " + bandNum + " Freq",
                juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f),
                1000.0f,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction([](float value, int) { return juce::String(value, 1) + " Hz"; })));

            // Gain
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(getParamID(ch, b, "gain"), 1),
                channelName + " Band " + bandNum + " Gain",
                juce::NormalisableRange<float>(-20.0f, 20.0f, 0.1f),
                0.0f,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction([](float value, int) { return juce::String(value, 1) + " dB"; })));

            // Q
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(getParamID(ch, b, "q"), 1),
                channelName + " Band " + bandNum + " Q",
                juce::NormalisableRange<float>(0.1f, 30.0f, 0.01f, 0.5f),
                1.0f));

            // Type
            layout.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID(getParamID(ch, b, "type"), 1),
                channelName + " Band " + bandNum + " Type",
                juce::StringArray{"Peak", "Low Shelf", "High Shelf"},
                0));

            // Bypass (default true = bypassed)
            layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(getParamID(ch, b, "bypass"), 1),
                channelName + " Band " + bandNum + " Bypass",
                true));
        }
    }

    return layout;
}

void RoomMultiEQAudioProcessor::initializeChannels(int count, const juce::AudioChannelSet& channelSet)
{
    // Resize vectors if needed
    while (static_cast<int>(channels.size()) < count)
    {
        channels.push_back(std::make_unique<ChannelEQ>());
        spectrumCollectors.push_back(std::make_unique<SpectrumDataCollector>());
    }

    // Get channel names from JUCE
    channelNames.clear();
    for (int i = 0; i < count; ++i)
    {
        auto channelType = channelSet.getTypeOfChannel(i);
        juce::String name = juce::AudioChannelSet::getAbbreviatedChannelTypeName(channelType);
        if (name.isEmpty())
            name = "Ch" + juce::String(i + 1);
        channelNames.push_back(name);
    }

    numChannels = count;
}

void RoomMultiEQAudioProcessor::parameterChanged(const juce::String& parameterID, float)
{
    if (parameterID == "master_bypass")
        return;

    // Parse channel index from "ch{N}_band_{M}_{param}" format
    if (!parameterID.startsWith("ch"))
        return;

    int underscorePos = parameterID.indexOf("_");
    if (underscorePos < 0)
        return;

    int channelIndex = parameterID.substring(2, underscorePos).getIntValue();

    // Parse band index
    int bandIndex = -1;
    for (int b = 0; b < NUM_EQ_BANDS; ++b)
    {
        if (parameterID.contains("_band_" + juce::String(b + 1) + "_"))
        {
            bandIndex = b;
            break;
        }
    }

    if (bandIndex >= 0 && channelIndex >= 0 && channelIndex < numChannels)
    {
        updateBandFromParameters(channelIndex, bandIndex);
    }
}

void RoomMultiEQAudioProcessor::updateBandFromParameters(int channelIndex, int band)
{
    if (channelIndex < 0 || channelIndex >= static_cast<int>(channels.size()))
        return;

    auto& eq = *channels[static_cast<size_t>(channelIndex)];
    auto& b = eq.getBand(band);

    b.setFrequency(*apvts.getRawParameterValue(getParamID(channelIndex, band, "freq")));
    b.setGain(*apvts.getRawParameterValue(getParamID(channelIndex, band, "gain")));
    b.setQ(*apvts.getRawParameterValue(getParamID(channelIndex, band, "q")));

    int typeIndex = static_cast<int>(*apvts.getRawParameterValue(getParamID(channelIndex, band, "type")));
    b.setType(static_cast<FilterType>(typeIndex));

    b.setBypassed(*apvts.getRawParameterValue(getParamID(channelIndex, band, "bypass")) > 0.5f);
}

const juce::String RoomMultiEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool RoomMultiEQAudioProcessor::acceptsMidi() const
{
    return false;
}

bool RoomMultiEQAudioProcessor::producesMidi() const
{
    return false;
}

bool RoomMultiEQAudioProcessor::isMidiEffect() const
{
    return false;
}

double RoomMultiEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int RoomMultiEQAudioProcessor::getNumPrograms()
{
    return 1;
}

int RoomMultiEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void RoomMultiEQAudioProcessor::setCurrentProgram(int)
{
}

const juce::String RoomMultiEQAudioProcessor::getProgramName(int)
{
    return {};
}

void RoomMultiEQAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void RoomMultiEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Initialize channels based on current bus layout
    auto channelSet = getBusesLayout().getMainInputChannelSet();
    int channelCount = channelSet.size();
    initializeChannels(channelCount, channelSet);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        channels[static_cast<size_t>(ch)]->prepare(spec);

        // Initialize all bands from current parameter values
        for (int b = 0; b < NUM_EQ_BANDS; ++b)
        {
            updateBandFromParameters(ch, b);
        }
    }
}

void RoomMultiEQAudioProcessor::releaseResources()
{
    for (auto& channel : channels)
        channel->reset();
}

bool RoomMultiEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Accept any layout where:
    // 1. Input and output channel sets match
    // 2. Channel set is not disabled
    // 3. Channel count is within our maximum
    auto inputSet = layouts.getMainInputChannelSet();
    auto outputSet = layouts.getMainOutputChannelSet();

    if (inputSet.isDisabled() || outputSet.isDisabled())
        return false;

    if (inputSet != outputSet)
        return false;

    if (inputSet.size() > MAX_CHANNELS)
        return false;

    return true;
}

void RoomMultiEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Update timestamp for host bypass detection
    lastProcessTime.store(juce::Time::getMillisecondCounterHiRes());

    int numSamples = buffer.getNumSamples();
    int channelsToProcess = std::min(buffer.getNumChannels(), numChannels);

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < channelsToProcess; ++ch)
        {
            float* channelData = buffer.getWritePointer(ch);

            spectrumCollectors[static_cast<size_t>(ch)]->pushInputSample(channelData[i]);
            channels[static_cast<size_t>(ch)]->processSample(channelData[i]);
            spectrumCollectors[static_cast<size_t>(ch)]->pushOutputSample(channelData[i]);
        }
    }
}

bool RoomMultiEQAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* RoomMultiEQAudioProcessor::createEditor()
{
    return new RoomMultiEQAudioProcessorEditor(*this);
}

void RoomMultiEQAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void RoomMultiEQAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

        // Update all bands from restored state
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int b = 0; b < NUM_EQ_BANDS; ++b)
            {
                updateBandFromParameters(ch, b);
            }
        }
    }
}

void RoomMultiEQAudioProcessor::resetBandToDefaults(int channelIndex, int band)
{
    if (auto* param = apvts.getParameter(getParamID(channelIndex, band, "freq")))
        param->setValueNotifyingHost(param->convertTo0to1(1000.0f));
    if (auto* param = apvts.getParameter(getParamID(channelIndex, band, "gain")))
        param->setValueNotifyingHost(param->convertTo0to1(0.0f));
    if (auto* param = apvts.getParameter(getParamID(channelIndex, band, "q")))
        param->setValueNotifyingHost(param->convertTo0to1(1.0f));
    if (auto* param = apvts.getParameter(getParamID(channelIndex, band, "type")))
        param->setValueNotifyingHost(0.0f);
    if (auto* param = apvts.getParameter(getParamID(channelIndex, band, "bypass")))
        param->setValueNotifyingHost(1.0f);
}

void RoomMultiEQAudioProcessor::loadFilterFile(int channelIndex, const juce::File& file)
{
    auto filters = FilterFileParser::parseFile(file);

    for (int b = 0; b < NUM_EQ_BANDS; ++b)
        resetBandToDefaults(channelIndex, b);

    // Apply parsed filters
    for (size_t i = 0; i < filters.size() && i < NUM_EQ_BANDS; ++i)
    {
        const auto& f = filters[i];
        int b = static_cast<int>(i);

        if (auto* param = apvts.getParameter(getParamID(channelIndex, b, "freq")))
            param->setValueNotifyingHost(param->convertTo0to1(f.frequency));

        if (auto* param = apvts.getParameter(getParamID(channelIndex, b, "gain")))
            param->setValueNotifyingHost(param->convertTo0to1(f.gainDB));

        if (auto* param = apvts.getParameter(getParamID(channelIndex, b, "q")))
            param->setValueNotifyingHost(param->convertTo0to1(f.q));

        if (auto* param = apvts.getParameter(getParamID(channelIndex, b, "type")))
            param->setValueNotifyingHost(static_cast<float>(f.type) / 2.0f);

        if (auto* param = apvts.getParameter(getParamID(channelIndex, b, "bypass")))
            param->setValueNotifyingHost(f.enabled ? 0.0f : 1.0f);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RoomMultiEQAudioProcessor();
}
