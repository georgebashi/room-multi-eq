#include "FilterFileParser.h"
#include "EQBand.h"
#include "ChannelEQ.h"
#include "FilterResponseCalculator.h"
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <iostream>

// Simple test framework
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " #name "... "; \
    try { test_##name(); testsPassed++; std::cout << "PASSED\n"; } \
    catch (const std::exception& e) { testsFailed++; std::cout << "FAILED: " << e.what() << "\n"; } \
} while(0)

#define ASSERT(cond) if (!(cond)) throw std::runtime_error("Assertion failed: " #cond)
#define ASSERT_EQ(a, b) if ((a) != (b)) throw std::runtime_error("Expected " #a " == " #b)
#define ASSERT_NEAR(a, b, tol) if (std::abs((a) - (b)) > (tol)) throw std::runtime_error("Expected " #a " ~= " #b)

//==============================================================================
// Filter File Parser Tests
//==============================================================================

TEST(parser_basic)
{
    juce::String content = R"(
Filter  1: ON  PK       Fc    63.0 Hz  Gain  -5.2 dB  Q  4.32
Filter  2: ON  PK       Fc   125.0 Hz  Gain  -3.1 dB  Q  2.87
Filter  3: OFF PK       Fc   250.0 Hz  Gain  -2.0 dB  Q  3.50
)";

    auto filters = FilterFileParser::parseString(content);

    ASSERT(filters.size() == 3);

    // Filter 1
    ASSERT(filters[0].enabled == true);
    ASSERT(filters[0].type == FilterType::Peak);
    ASSERT_NEAR(filters[0].frequency, 63.0f, 0.1f);
    ASSERT_NEAR(filters[0].gainDB, -5.2f, 0.1f);
    ASSERT_NEAR(filters[0].q, 4.32f, 0.01f);

    // Filter 2
    ASSERT(filters[1].enabled == true);
    ASSERT_NEAR(filters[1].frequency, 125.0f, 0.1f);
    ASSERT_NEAR(filters[1].gainDB, -3.1f, 0.1f);

    // Filter 3 - OFF
    ASSERT(filters[2].enabled == false);
    ASSERT_NEAR(filters[2].frequency, 250.0f, 0.1f);
}

TEST(parser_shelf_filters)
{
    juce::String content = R"(
Filter  1: ON  LS       Fc    80.0 Hz  Gain  +2.0 dB  Q  0.71
Filter  2: ON  HS       Fc  8000.0 Hz  Gain  -1.5 dB  Q  0.71
)";

    auto filters = FilterFileParser::parseString(content);

    ASSERT(filters.size() == 2);
    ASSERT(filters[0].type == FilterType::LowShelf);
    ASSERT(filters[1].type == FilterType::HighShelf);
}

TEST(parser_skips_lp_hp)
{
    juce::String content = R"(
Filter  1: ON  LP       Fc  5000.0 Hz  Gain   0.0 dB  Q  0.71
Filter  2: ON  HP       Fc   100.0 Hz  Gain   0.0 dB  Q  0.71
Filter  3: ON  PK       Fc  1000.0 Hz  Gain  -3.0 dB  Q  2.00
)";

    auto filters = FilterFileParser::parseString(content);

    // LP and HP should be skipped, only PK should be parsed
    ASSERT(filters.size() == 1);
    ASSERT_NEAR(filters[0].frequency, 1000.0f, 0.1f);
}

TEST(parser_skips_none_filters)
{
    juce::String content = R"(
Filter  1: ON  PK       Fc   39.20 Hz  Gain   -4.1 dB  Q  4.918
Filter  2: ON  PK       Fc   54.60 Hz  Gain   -7.0 dB  Q  4.911
Filter 11: ON  None
Filter 12: ON  None
Filter  1: ON  None
Filter  2: ON  None
)";

    auto filters = FilterFileParser::parseString(content);

    // Only first two real filters should be parsed, all None should be skipped
    ASSERT(filters.size() == 2);
    ASSERT_NEAR(filters[0].frequency, 39.20f, 0.1f);
    ASSERT_NEAR(filters[1].frequency, 54.60f, 0.1f);
}

TEST(parser_shelf_without_q)
{
    juce::String content = R"(
Filter  1: ON  LS       Fc   64.00 Hz  Gain    5.7 dB
Filter  2: ON  HS       Fc 8000.00 Hz  Gain   -2.0 dB
)";

    auto filters = FilterFileParser::parseString(content);

    ASSERT(filters.size() == 2);
    ASSERT(filters[0].type == FilterType::LowShelf);
    ASSERT_NEAR(filters[0].frequency, 64.0f, 0.1f);
    ASSERT_NEAR(filters[0].gainDB, 5.7f, 0.1f);
    ASSERT_NEAR(filters[0].q, 0.71f, 0.01f);  // Default Q for shelf

    ASSERT(filters[1].type == FilterType::HighShelf);
    ASSERT_NEAR(filters[1].q, 0.71f, 0.01f);  // Default Q for shelf
}

//==============================================================================
// EQBand Tests
//==============================================================================

TEST(eqband_bypassed_passthrough)
{
    EQBand band;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1;
    band.prepare(spec);

    band.setBypassed(true);  // Filter should pass through

    float sample = 0.5f;
    float original = sample;
    band.processSample(sample);

    ASSERT_EQ(sample, original);  // Should be unchanged when bypassed
}

TEST(eqband_peak_filter_attenuates)
{
    EQBand band;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1;
    band.prepare(spec);

    band.setFrequency(1000.0f);
    band.setGain(-12.0f);  // -12 dB cut
    band.setQ(2.0f);
    band.setType(FilterType::Peak);
    band.setBypassed(false);  // Filter is active

    // Generate 1kHz sine wave and process
    const int numSamples = 4410;  // 100ms at 44.1kHz
    float inputRMS = 0.0f;
    float outputRMS = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float phase = static_cast<float>(i) / 44100.0f * 1000.0f * 2.0f * juce::MathConstants<float>::pi;
        float sample = std::sin(phase);
        inputRMS += sample * sample;

        band.processSample(sample);
        outputRMS += sample * sample;
    }

    inputRMS = std::sqrt(inputRMS / numSamples);
    outputRMS = std::sqrt(outputRMS / numSamples);

    // Output should be significantly attenuated at 1kHz with -12dB cut
    float gainDB = 20.0f * std::log10(outputRMS / inputRMS);

    // Should be around -12dB (allow some tolerance for filter settling)
    ASSERT(gainDB < -6.0f);  // At least 6dB attenuation
    ASSERT(gainDB > -18.0f); // But not more than 18dB
}

TEST(eqband_peak_filter_boosts)
{
    EQBand band;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1;
    band.prepare(spec);

    band.setFrequency(1000.0f);
    band.setGain(12.0f);  // +12 dB boost
    band.setQ(2.0f);
    band.setType(FilterType::Peak);
    band.setBypassed(false);

    // Generate 1kHz sine wave and process
    const int numSamples = 4410;
    float inputRMS = 0.0f;
    float outputRMS = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float phase = static_cast<float>(i) / 44100.0f * 1000.0f * 2.0f * juce::MathConstants<float>::pi;
        float sample = std::sin(phase) * 0.1f;  // Lower amplitude to avoid clipping
        inputRMS += sample * sample;

        band.processSample(sample);
        outputRMS += sample * sample;
    }

    inputRMS = std::sqrt(inputRMS / numSamples);
    outputRMS = std::sqrt(outputRMS / numSamples);

    float gainDB = 20.0f * std::log10(outputRMS / inputRMS);

    // Should be around +12dB
    ASSERT(gainDB > 6.0f);   // At least 6dB boost
    ASSERT(gainDB < 18.0f);  // But not more than 18dB
}

TEST(eqband_offband_passthrough)
{
    EQBand band;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1;
    band.prepare(spec);

    band.setFrequency(1000.0f);
    band.setGain(-12.0f);
    band.setQ(10.0f);  // Narrow Q
    band.setType(FilterType::Peak);
    band.setBypassed(false);

    // Generate 100Hz sine wave (far from 1kHz filter)
    const int numSamples = 4410;
    float inputRMS = 0.0f;
    float outputRMS = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float phase = static_cast<float>(i) / 44100.0f * 100.0f * 2.0f * juce::MathConstants<float>::pi;
        float sample = std::sin(phase);
        inputRMS += sample * sample;

        band.processSample(sample);
        outputRMS += sample * sample;
    }

    inputRMS = std::sqrt(inputRMS / numSamples);
    outputRMS = std::sqrt(outputRMS / numSamples);

    float gainDB = 20.0f * std::log10(outputRMS / inputRMS);

    // Off-band should be nearly unchanged (within 1dB)
    ASSERT(std::abs(gainDB) < 1.0f);
}

//==============================================================================
// ChannelEQ Tests
//==============================================================================

TEST(channeleq_processes_all_bands)
{
    ChannelEQ eq;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1;
    eq.prepare(spec);

    // Enable first band with -6dB at 1kHz
    eq.getBand(0).setFrequency(1000.0f);
    eq.getBand(0).setGain(-6.0f);
    eq.getBand(0).setQ(2.0f);
    eq.getBand(0).setBypassed(false);

    // Leave other bands bypassed (default)

    const int numSamples = 4410;
    float inputRMS = 0.0f;
    float outputRMS = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float phase = static_cast<float>(i) / 44100.0f * 1000.0f * 2.0f * juce::MathConstants<float>::pi;
        float sample = std::sin(phase);
        inputRMS += sample * sample;

        eq.processSample(sample);
        outputRMS += sample * sample;
    }

    inputRMS = std::sqrt(inputRMS / numSamples);
    outputRMS = std::sqrt(outputRMS / numSamples);

    float gainDB = 20.0f * std::log10(outputRMS / inputRMS);

    // Should see attenuation from the active band
    ASSERT(gainDB < -2.0f);
}

//==============================================================================
// FilterResponseCalculator Tests
//==============================================================================

TEST(filter_response_flat_when_bypassed)
{
    ChannelEQ eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1;
    eq.prepare(spec);

    // All bands bypassed by default
    auto response = FilterResponseCalculator::calculateResponse(eq, 44100.0, 100);

    // Response should be 0dB everywhere
    for (float db : response)
    {
        ASSERT_NEAR(db, 0.0f, 0.01f);
    }
}

TEST(filter_response_peak_at_center)
{
    ChannelEQ eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1;
    eq.prepare(spec);

    eq.getBand(0).setFrequency(1000.0f);
    eq.getBand(0).setGain(6.0f);
    eq.getBand(0).setQ(2.0f);
    eq.getBand(0).setType(FilterType::Peak);
    eq.getBand(0).setBypassed(false);

    auto response = FilterResponseCalculator::calculateResponse(eq, 44100.0, 200);

    // Find the point closest to 1kHz (log scale)
    // 1kHz is at t = (log10(1000) - log10(20)) / (log10(20000) - log10(20))
    // = (3 - 1.301) / (4.301 - 1.301) = 1.699 / 3 = 0.566
    int idx1k = static_cast<int>(0.566f * 199);

    // Response at 1kHz should be close to +6dB
    ASSERT(response[idx1k] > 5.0f);
    ASSERT(response[idx1k] < 7.0f);

    // Response far from center should be near 0dB
    ASSERT(std::abs(response[0]) < 1.0f);   // 20Hz
    ASSERT(std::abs(response[199]) < 1.0f); // 20kHz
}

//==============================================================================
// Main
//==============================================================================

int main()
{
    std::cout << "=== Room Multi EQ Tests ===\n\n";

    // Parser tests
    RUN_TEST(parser_basic);
    RUN_TEST(parser_shelf_filters);
    RUN_TEST(parser_skips_lp_hp);
    RUN_TEST(parser_skips_none_filters);
    RUN_TEST(parser_shelf_without_q);

    // EQBand tests
    RUN_TEST(eqband_bypassed_passthrough);
    RUN_TEST(eqband_peak_filter_attenuates);
    RUN_TEST(eqband_peak_filter_boosts);
    RUN_TEST(eqband_offband_passthrough);

    // ChannelEQ tests
    RUN_TEST(channeleq_processes_all_bands);

    // FilterResponseCalculator tests
    RUN_TEST(filter_response_flat_when_bypassed);
    RUN_TEST(filter_response_peak_at_center);

    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << testsPassed << "\n";
    std::cout << "Failed: " << testsFailed << "\n";

    return testsFailed > 0 ? 1 : 0;
}
