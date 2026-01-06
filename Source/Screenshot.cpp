/**
 * Screenshot capture utility for Room Multi EQ plugin.
 * Creates a headless snapshot of the plugin UI for documentation.
 *
 * Usage: ./build/RoomMultiEQ_Screenshot
 * Output: docs/screenshot.png
 */

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_graphics/juce_graphics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main()
{
    // Initialize JUCE GUI subsystem (required for component rendering)
    juce::ScopedJuceInitialiser_GUI juceInit;

    // Create processor instance
    RoomMultiEQAudioProcessor processor;

    // Get parameter tree for setting sample values
    auto& apvts = processor.getAPVTS();

    // Helper to set band parameters
    auto setBand = [&apvts](const juce::String& channel, int band,
                            float freq, float gain, float q, bool bypass = false) {
        auto prefix = channel + "_band_" + juce::String(band) + "_";
        if (auto* param = apvts.getParameter(prefix + "freq"))
            param->setValueNotifyingHost(param->convertTo0to1(freq));
        if (auto* param = apvts.getParameter(prefix + "gain"))
            param->setValueNotifyingHost(param->convertTo0to1(gain));
        if (auto* param = apvts.getParameter(prefix + "q"))
            param->setValueNotifyingHost(param->convertTo0to1(q));
        if (auto* param = apvts.getParameter(prefix + "bypass"))
            param->setValueNotifyingHost(bypass ? 0.0f : 1.0f);
    };

    // Set sample filter data - Left Channel
    setBand("left", 1, 63.0f, -4.5f, 4.0f);
    setBand("left", 2, 125.0f, -3.2f, 3.5f);
    setBand("left", 3, 250.0f, 2.0f, 2.8f);

    // Set sample filter data - Right Channel
    setBand("right", 1, 80.0f, -5.0f, 4.5f);
    setBand("right", 2, 200.0f, -2.8f, 3.0f);

    // Create editor (this creates the full UI)
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());

    if (editor == nullptr)
    {
        std::cerr << "Failed to create plugin editor\n";
        return 1;
    }

    // Render the component to an image
    auto image = editor->createComponentSnapshot(editor->getLocalBounds());

    // Determine output path (relative to working directory)
    juce::File outputFile = juce::File::getCurrentWorkingDirectory()
                                .getChildFile("docs")
                                .getChildFile("screenshot.png");

    // Ensure docs directory exists
    outputFile.getParentDirectory().createDirectory();

    // Save as PNG
    juce::FileOutputStream stream(outputFile);
    if (stream.openedOk())
    {
        juce::PNGImageFormat pngFormat;
        if (pngFormat.writeImageToStream(image, stream))
        {
            std::cout << "Screenshot saved to: " << outputFile.getFullPathName() << "\n";
            return 0;
        }
        else
        {
            std::cerr << "Failed to write PNG image\n";
            return 1;
        }
    }
    else
    {
        std::cerr << "Failed to open output file: " << outputFile.getFullPathName() << "\n";
        return 1;
    }
}
