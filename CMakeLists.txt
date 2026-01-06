cmake_minimum_required(VERSION 3.22)

# Allow version override from CI (must be numeric: YYYY.M.D)
if(NOT DEFINED ROOMMULTIEQ_CMAKE_VERSION)
    set(ROOMMULTIEQ_CMAKE_VERSION "1.0.0")
endif()

project(RoomMultiEQ VERSION ${ROOMMULTIEQ_CMAKE_VERSION})

# Build version for display (set by CI, e.g., 2025.01.05-abc1234)
if(NOT DEFINED ROOMMULTIEQ_BUILD_VERSION)
    set(ROOMMULTIEQ_BUILD_VERSION "dev")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS deployment target for macOS 12+
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS deployment version")

# Universal binary for Intel + Apple Silicon
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS")

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Plugin target
juce_add_plugin(RoomMultiEQ
    COMPANY_NAME "George Bashi"
    BUNDLE_ID "com.georgebashi.roommultieq"
    LV2URI "urn:georgebashi:roommultieq"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Gbsh
    PLUGIN_CODE Rmeq
    FORMATS AU VST3 LV2
    PRODUCT_NAME "Room Multi EQ"
)

# Source files
target_sources(RoomMultiEQ
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/EQBand.cpp
        Source/EQBand.h
        Source/ChannelEQ.cpp
        Source/ChannelEQ.h
        Source/FilterFileParser.cpp
        Source/FilterFileParser.h
        Source/SpectrumDataCollector.cpp
        Source/SpectrumDataCollector.h
        Source/FilterResponseCalculator.cpp
        Source/FilterResponseCalculator.h
        Source/SpectrumAnalyzer.cpp
        Source/SpectrumAnalyzer.h
)

# Preprocessor definitions
target_compile_definitions(RoomMultiEQ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# Link JUCE modules
target_link_libraries(RoomMultiEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Compiler-specific flag to treat warnings as errors
if(MSVC)
    set(WERROR_FLAG "/WX")
else()
    set(WERROR_FLAG "-Werror")
endif()

# Treat warnings as errors for our source files only
set_source_files_properties(
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/EQBand.cpp
    Source/ChannelEQ.cpp
    Source/FilterFileParser.cpp
    PROPERTIES COMPILE_FLAGS "${WERROR_FLAG}"
)

# Test executable
add_executable(RoomMultiEQ_Tests
    Source/Tests.cpp
    Source/FilterFileParser.cpp
    Source/EQBand.cpp
    Source/ChannelEQ.cpp
    Source/FilterResponseCalculator.cpp
)

target_link_libraries(RoomMultiEQ_Tests
    PRIVATE
        juce::juce_core
        juce::juce_dsp
)

target_compile_definitions(RoomMultiEQ_Tests
    PRIVATE
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

# Treat warnings as errors for test source files
set_source_files_properties(
    Source/Tests.cpp
    Source/FilterFileParser.cpp
    Source/EQBand.cpp
    Source/ChannelEQ.cpp
    PROPERTIES COMPILE_FLAGS "${WERROR_FLAG}"
)

# Screenshot capture executable (optional, for documentation only)
option(ROOMMULTIEQ_BUILD_SCREENSHOT "Build screenshot capture tool" OFF)

if(ROOMMULTIEQ_BUILD_SCREENSHOT)
    add_executable(RoomMultiEQ_Screenshot
        Source/Screenshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/EQBand.cpp
        Source/ChannelEQ.cpp
        Source/FilterFileParser.cpp
        Source/SpectrumDataCollector.cpp
        Source/FilterResponseCalculator.cpp
        Source/SpectrumAnalyzer.cpp
    )

    target_link_libraries(RoomMultiEQ_Screenshot
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
    )

    target_compile_definitions(RoomMultiEQ_Screenshot
        PRIVATE
            JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JucePlugin_Name="Room Multi EQ"
    )
endif()
