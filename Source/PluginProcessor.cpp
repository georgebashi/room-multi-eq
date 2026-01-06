#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FilterFileParser.h"

RoomMultiEQAudioProcessor::RoomMultiEQAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Add parameter listeners
    apvts.addParameterListener("master_bypass", this);

    for (int ch = 0; ch < 2; ++ch)
    {
        juce::String channel = (ch == 0) ? "left" : "right";
        for (int b = 0; b < NUM_EQ_BANDS; ++b)
        {
            apvts.addParameterListener(getParamID(channel, b, "freq"), this);
            apvts.addParameterListener(getParamID(channel, b, "gain"), this);
            apvts.addParameterListener(getParamID(channel, b, "q"), this);
            apvts.addParameterListener(getParamID(channel, b, "type"), this);
            apvts.addParameterListener(getParamID(channel, b, "bypass"), this);
        }
    }
}

RoomMultiEQAudioProcessor::~RoomMultiEQAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout RoomMultiEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Master bypass
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("master_bypass", 1),
        "Master Bypass",
        false));

    // Per-channel, per-band parameters
    for (int ch = 0; ch < 2; ++ch)
    {
        juce::String channel = (ch == 0) ? "left" : "right";
        juce::String channelName = (ch == 0) ? "Left" : "Right";

        for (int b = 0; b < NUM_EQ_BANDS; ++b)
        {
            juce::String bandNum = juce::String(b + 1);

            // Frequency
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(getParamID(channel, b, "freq"), 1),
                channelName + " Band " + bandNum + " Freq",
                juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f),
                1000.0f,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction([](float value, int) { return juce::String(value, 1) + " Hz"; })));

            // Gain
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(getParamID(channel, b, "gain"), 1),
                channelName + " Band " + bandNum + " Gain",
                juce::NormalisableRange<float>(-20.0f, 20.0f, 0.1f),
                0.0f,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction([](float value, int) { return juce::String(value, 1) + " dB"; })));

            // Q
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(getParamID(channel, b, "q"), 1),
                channelName + " Band " + bandNum + " Q",
                juce::NormalisableRange<float>(0.1f, 30.0f, 0.01f, 0.5f),
                1.0f));

            // Type
            layout.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID(getParamID(channel, b, "type"), 1),
                channelName + " Band " + bandNum + " Type",
                juce::StringArray{"Peak", "Low Shelf", "High Shelf"},
                0));

            // Bypass
            layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(getParamID(channel, b, "bypass"), 1),
                channelName + " Band " + bandNum + " Bypass",
                true));
        }
    }

    return layout;
}

void RoomMultiEQAudioProcessor::parameterChanged(const juce::String& parameterID, float)
{
    if (parameterID == "master_bypass")
        return;

    // Parse channel and band from parameter ID
    bool isLeft = parameterID.startsWith("left_");
    int bandIndex = -1;

    for (int b = 0; b < NUM_EQ_BANDS; ++b)
    {
        if (parameterID.contains("_band_" + juce::String(b + 1) + "_"))
        {
            bandIndex = b;
            break;
        }
    }

    if (bandIndex >= 0)
    {
        updateBandFromParameters(isLeft ? 0 : 1, bandIndex);
    }
}

void RoomMultiEQAudioProcessor::updateBandFromParameters(int channel, int band)
{
    juce::String ch = (channel == 0) ? "left" : "right";
    ChannelEQ& eq = (channel == 0) ? leftChannel : rightChannel;

    auto& b = eq.getBand(band);
    b.setFrequency(*apvts.getRawParameterValue(getParamID(ch, band, "freq")));
    b.setGain(*apvts.getRawParameterValue(getParamID(ch, band, "gain")));
    b.setQ(*apvts.getRawParameterValue(getParamID(ch, band, "q")));

    int typeIndex = static_cast<int>(*apvts.getRawParameterValue(getParamID(ch, band, "type")));
    b.setType(static_cast<FilterType>(typeIndex));

    b.setBypassed(*apvts.getRawParameterValue(getParamID(ch, band, "bypass")) > 0.5f);
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
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;

    leftChannel.prepare(spec);
    rightChannel.prepare(spec);

    // Initialize all bands from current parameter values
    for (int b = 0; b < NUM_EQ_BANDS; ++b)
    {
        updateBandFromParameters(0, b);
        updateBandFromParameters(1, b);
    }
}

void RoomMultiEQAudioProcessor::releaseResources()
{
    leftChannel.reset();
    rightChannel.reset();
}

bool RoomMultiEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void RoomMultiEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Read bypass directly from parameter for reliability
    if (*apvts.getRawParameterValue("master_bypass") > 0.5f)
        return;

    auto* leftData = buffer.getWritePointer(0);
    auto* rightData = buffer.getWritePointer(1);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        leftChannel.processSample(leftData[i]);
        rightChannel.processSample(rightData[i]);
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
        for (int b = 0; b < NUM_EQ_BANDS; ++b)
        {
            updateBandFromParameters(0, b);
            updateBandFromParameters(1, b);
        }
    }
}

void RoomMultiEQAudioProcessor::resetBandToDefaults(const juce::String& channel, int band)
{
    if (auto* param = apvts.getParameter(getParamID(channel, band, "freq")))
        param->setValueNotifyingHost(param->convertTo0to1(1000.0f));
    if (auto* param = apvts.getParameter(getParamID(channel, band, "gain")))
        param->setValueNotifyingHost(param->convertTo0to1(0.0f));
    if (auto* param = apvts.getParameter(getParamID(channel, band, "q")))
        param->setValueNotifyingHost(param->convertTo0to1(1.0f));
    if (auto* param = apvts.getParameter(getParamID(channel, band, "type")))
        param->setValueNotifyingHost(0.0f);
    if (auto* param = apvts.getParameter(getParamID(channel, band, "bypass")))
        param->setValueNotifyingHost(1.0f);
}

void RoomMultiEQAudioProcessor::loadFilterFile(bool isLeftChannel, const juce::File& file)
{
    auto filters = FilterFileParser::parseFile(file);
    juce::String channel = isLeftChannel ? "left" : "right";

    for (int b = 0; b < NUM_EQ_BANDS; ++b)
        resetBandToDefaults(channel, b);

    // Apply parsed filters
    for (size_t i = 0; i < filters.size() && i < NUM_EQ_BANDS; ++i)
    {
        const auto& f = filters[i];
        int b = static_cast<int>(i);

        if (auto* param = apvts.getParameter(getParamID(channel, b, "freq")))
            param->setValueNotifyingHost(param->convertTo0to1(f.frequency));

        if (auto* param = apvts.getParameter(getParamID(channel, b, "gain")))
            param->setValueNotifyingHost(param->convertTo0to1(f.gainDB));

        if (auto* param = apvts.getParameter(getParamID(channel, b, "q")))
            param->setValueNotifyingHost(param->convertTo0to1(f.q));

        if (auto* param = apvts.getParameter(getParamID(channel, b, "type")))
            param->setValueNotifyingHost(static_cast<float>(f.type) / 2.0f);

        if (auto* param = apvts.getParameter(getParamID(channel, b, "bypass")))
            param->setValueNotifyingHost(f.enabled ? 0.0f : 1.0f);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RoomMultiEQAudioProcessor();
}
