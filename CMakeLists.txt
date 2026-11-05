cmake_minimum_required(VERSION 3.22)

project(ademusicVocalRack VERSION 1.2.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (APPLE)
    if (NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
    endif()
    if (NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architecture")
    endif()
endif()

# Reproducible JUCE pin.  The local path is only an offline-build convenience.
set(VR_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (default: fetch pin)")
set(VR_JUCE_TAG "8.0.15" CACHE STRING "JUCE tag fetched when VR_JUCE_PATH is empty")
option(VR_BUILD_TESTS "Build the headless DSP/validation suite" ON)

if (VR_JUCE_PATH)
    message(STATUS "Vocal Rack: using local JUCE at ${VR_JUCE_PATH}")
    add_subdirectory("${VR_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    message(STATUS "Vocal Rack: fetching JUCE ${VR_JUCE_TAG} (pin 91ad83ae34a81e0833b1a2b0866f54846370ae53)")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${VR_JUCE_TAG}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(VR_CORE_SOURCES
    Source/PluginProcessor.cpp
    Source/Engine.cpp
    Source/Parameters.cpp
    Source/Presets.cpp
    Source/dsp/PitchDetector.cpp
    Source/dsp/PitchShifter.cpp
    Source/dsp/PitchSelector.cpp
    Source/dsp/SpectralDenoiser.cpp
    Source/dsp/Reverb.cpp
)

set(VR_UI_SOURCES
    Source/PluginEditor.cpp
    Source/ui/VocalRackLookAndFeel.cpp
    Source/ui/RackComponents.cpp
)

juce_add_plugin(VocalRack
    PRODUCT_NAME              "ademusic Vocal Rack"
    COMPANY_NAME              "ademusic"
    BUNDLE_ID                 "com.ademusic.vocalrack"
    PLUGIN_MANUFACTURER_CODE  Adms
    PLUGIN_CODE               Vrck
    FORMATS                   VST3 AU
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           "Fx"
    AU_MAIN_TYPE              "kAudioUnitType_Effect"
)

target_sources(VocalRack PRIVATE ${VR_CORE_SOURCES} ${VR_UI_SOURCES})

target_include_directories(VocalRack PRIVATE Source)

target_compile_definitions(VocalRack PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_APPLICATION_NAME_STRING="ademusic Vocal Rack"
    JUCE_APPLICATION_VERSION_STRING="1.2.0"
    VR_VERSION_STRING="1.2.0"
)

target_link_libraries(VocalRack
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
)

if (VR_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(VocalRackTests PRODUCT_NAME "VocalRackTests")

    target_sources(VocalRackTests PRIVATE
        Tests/Tests.cpp
        ${VR_CORE_SOURCES}
        ${VR_UI_SOURCES}
    )

    target_include_directories(VocalRackTests PRIVATE Source)

    target_compile_definitions(VocalRackTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_STANDALONE_APPLICATION=1
        JUCE_APPLICATION_NAME_STRING="VocalRackTests"
        JUCE_APPLICATION_VERSION_STRING="1.2.0"
        VR_VERSION_STRING="1.2.0"
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=1
    )

    target_link_libraries(VocalRackTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_audio_basics
            juce::juce_dsp
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
    )

    add_test(NAME VocalRackTests COMMAND VocalRackTests)
endif()
