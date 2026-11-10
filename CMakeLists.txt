cmake_minimum_required(VERSION 3.22)
# Set before project() so CMake applies these values during compiler detection.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum supported macOS")
set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Legacy Intel target architecture")

project(SmartArranger VERSION 0.1.0 LANGUAGES C CXX)

# Point JUCE_DIR at a JUCE checkout containing CMakeLists.txt. We intentionally
# avoid fetching dependencies during configure so the build is reproducible.
set(JUCE_DIR "" CACHE PATH "Path to a JUCE source checkout")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Set JUCE_DIR to a JUCE source checkout (CMakeLists.txt not found).")
endif()
add_subdirectory("${JUCE_DIR}" JUCE)

juce_add_plugin(SmartArranger
    COMPANY_NAME "Smart Arranger"
    BUNDLE_ID "com.smartarranger.plugin"
    PLUGIN_MANUFACTURER_CODE SmAr
    PLUGIN_CODE SmA1
    PRODUCT_NAME "Smart Arranger"
    FORMATS AU
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_SYNTH FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(SmartArranger)

target_sources(SmartArranger PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/StyleFile.cpp
    Source/StyleFile.h
)

target_compile_features(SmartArranger PRIVATE cxx_std_17)
target_compile_definitions(SmartArranger PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(SmartArranger PRIVATE
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Standalone diagnostic host shares the exact processor, style parser and UI
# sources with the AU target. It creates a CoreMIDI virtual output at runtime.
juce_add_gui_app(SmartArranger_Standalone
    PRODUCT_NAME "Smart Arranger Standalone"
    COMPANY_NAME "Smart Arranger"
    BUNDLE_ID "com.smartarranger.standalone"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/SmartArrangerIcon.png"
)
juce_generate_juce_header(SmartArranger_Standalone)
target_sources(SmartArranger_Standalone PRIVATE
    Source/Standalone/Main.cpp
    Source/Standalone/MainComponent.cpp
    Source/Standalone/MainComponent.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/StyleFile.cpp
    Source/StyleFile.h
)
target_compile_features(SmartArranger_Standalone PRIVATE cxx_std_17)
target_compile_definitions(SmartArranger_Standalone PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JucePlugin_Name="Smart Arranger"
)
target_link_libraries(SmartArranger_Standalone PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# The legacy target above is a build setting, not proof of runtime compatibility.
