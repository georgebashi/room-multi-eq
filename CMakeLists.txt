cmake_minimum_required(VERSION 3.22)

# Allow version override from CI (e.g., -DROOMMULTIEQ_VERSION=2025.01.05-abc1234)
if(NOT DEFINED ROOMMULTIEQ_VERSION)
    set(ROOMMULTIEQ_VERSION "1.0.0")
endif()

project(RoomMultiEQ VERSION ${ROOMMULTIEQ_VERSION})

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

# Treat warnings as errors for our source files only
set_source_files_properties(
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/EQBand.cpp
    Source/ChannelEQ.cpp
    Source/FilterFileParser.cpp
    PROPERTIES COMPILE_FLAGS "-Werror"
)

# Test executable
add_executable(RoomMultiEQ_Tests
    Source/Tests.cpp
    Source/FilterFileParser.cpp
    Source/EQBand.cpp
    Source/ChannelEQ.cpp
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
    PROPERTIES COMPILE_FLAGS "-Werror"
)
