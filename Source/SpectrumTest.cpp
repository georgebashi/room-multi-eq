/**
 * Spectrum Analyzer Test Harness
 *
 * Creates a processor, feeds synthetic audio, and captures a screenshot
 * to verify spectrum visualization is working.
 */

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_graphics/juce_graphics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>
#include <cmath>

int main()
{
    std::cout << "=== Spectrum Analyzer Test Harness ===\n\n";

    // Initialize JUCE GUI subsystem
    juce::ScopedJuceInitialiser_GUI juceInit;

    // Create processor instance
    RoomMultiEQAudioProcessor processor;
    std::cout << "1. Created processor\n";

    // Prepare for playback - THIS IS CRITICAL for sample rate
    const double sampleRate = 44100.0;
    const int blockSize = 512;
    processor.prepareToPlay(sampleRate, blockSize);
    std::cout << "2. Called prepareToPlay(sr=" << sampleRate << ", blockSize=" << blockSize << ")\n";
    std::cout << "   Processor reports sampleRate: " << processor.getCurrentSampleRate() << "\n";
    std::cout << "   Processor reports numChannels: " << processor.getNumChannels() << "\n";

    // Set up some filter bands so we see a curve
    // New API uses channel index instead of "left"/"right"
    auto& apvts = processor.getAPVTS();
    auto setBand = [&apvts](int channelIndex, int band,
                            float freq, float gain, float q) {
        auto prefix = "ch" + juce::String(channelIndex) + "_band_" + juce::String(band) + "_";
        if (auto* param = apvts.getParameter(prefix + "freq"))
            param->setValueNotifyingHost(param->convertTo0to1(freq));
        if (auto* param = apvts.getParameter(prefix + "gain"))
            param->setValueNotifyingHost(param->convertTo0to1(gain));
        if (auto* param = apvts.getParameter(prefix + "q"))
            param->setValueNotifyingHost(param->convertTo0to1(q));
        if (auto* param = apvts.getParameter(prefix + "bypass"))
            param->setValueNotifyingHost(0.0f);  // 0.0 = not bypassed (filter active)
    };

    // Channel 0 (L), Channel 1 (R)
    setBand(0, 1, 100.0f, 6.0f, 2.0f);
    setBand(0, 2, 1000.0f, -6.0f, 2.0f);
    setBand(1, 1, 500.0f, 3.0f, 1.5f);
    std::cout << "3. Set filter bands\n";

    // Generate synthetic audio and process it
    // Use a mix of frequencies to make spectrum visible
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;

    std::cout << "4. Processing synthetic audio...\n";

    // Process multiple blocks to fill the ring buffer (4096 samples needed)
    const int numBlocksToProcess = 20;  // 20 * 512 = 10240 samples
    for (int block = 0; block < numBlocksToProcess; ++block)
    {
        // Generate test signal: sum of sine waves at different frequencies
        for (int i = 0; i < blockSize; ++i)
        {
            int sampleIndex = block * blockSize + i;
            float t = static_cast<float>(sampleIndex) / static_cast<float>(sampleRate);

            // Mix of frequencies: 100Hz, 440Hz, 1kHz, 5kHz
            float sample = 0.0f;
            sample += 0.3f * std::sin(2.0f * juce::MathConstants<float>::pi * 100.0f * t);
            sample += 0.3f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * t);
            sample += 0.2f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * t);
            sample += 0.1f * std::sin(2.0f * juce::MathConstants<float>::pi * 5000.0f * t);

            buffer.setSample(0, i, sample);  // Left
            buffer.setSample(1, i, sample);  // Right
        }

        processor.processBlock(buffer, midi);
    }

    std::cout << "   Processed " << (numBlocksToProcess * blockSize) << " samples\n";

    // Verify data in collectors (use new multi-channel API)
    auto& collector = processor.getSpectrumCollector(0);  // Channel 0
    auto spectrum = collector.getInputSpectrum(sampleRate);

    float maxDb = -200.0f;
    int maxBin = 0;
    for (size_t i = 0; i < spectrum.size(); ++i)
    {
        if (spectrum[i] > maxDb)
        {
            maxDb = spectrum[i];
            maxBin = static_cast<int>(i);
        }
    }
    float maxFreq = static_cast<float>(maxBin) * static_cast<float>(sampleRate) / 4096.0f;

    std::cout << "5. Spectrum analysis:\n";
    std::cout << "   Spectrum size: " << spectrum.size() << " bins\n";
    std::cout << "   Max level: " << maxDb << " dB at bin " << maxBin << " (~" << maxFreq << " Hz)\n";

    // Print first few bins
    std::cout << "   First 20 bins (dB): ";
    for (int i = 0; i < 20 && i < static_cast<int>(spectrum.size()); ++i)
    {
        std::cout << spectrum[static_cast<size_t>(i)] << " ";
    }
    std::cout << "\n";

    // Create editor
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    if (!editor)
    {
        std::cerr << "Failed to create editor\n";
        return 1;
    }
    std::cout << "6. Created editor\n";

    // With new UI, the spectrum analyzer is updated via timer, which doesn't work in test context.
    // Just take a snapshot - the filter curves should be visible even without spectrum data.
    std::cout << "7. New UI uses unified multi-channel spectrum (timer-based updates).\n";

    // Capture screenshot
    auto image = editor->createComponentSnapshot(editor->getLocalBounds());

    juce::File outputFile = juce::File::getCurrentWorkingDirectory()
                                .getChildFile("docs")
                                .getChildFile("spectrum_test.png");
    outputFile.getParentDirectory().createDirectory();

    juce::FileOutputStream stream(outputFile);
    if (stream.openedOk())
    {
        juce::PNGImageFormat pngFormat;
        if (pngFormat.writeImageToStream(image, stream))
        {
            std::cout << "8. Screenshot saved to: " << outputFile.getFullPathName() << "\n";
        }
        else
        {
            std::cerr << "Failed to write PNG\n";
            return 1;
        }
    }
    else
    {
        std::cerr << "Failed to open output file\n";
        return 1;
    }

    std::cout << "\n=== Test Complete ===\n";
    return 0;
}
