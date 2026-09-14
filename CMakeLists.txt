cmake_minimum_required(VERSION 3.22)

project(K2M VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add local JUCE 8 submodule
add_subdirectory(external/JUCE)

juce_add_gui_app(K2M
    PRODUCT_NAME "K2M"
    VERSION 0.1.0
    COMPANY_NAME "K2M Project"
    BUNDLE_ID "com.k2m.autosampler"
)

target_sources(K2M
    PRIVATE
        Source/App/Main.cpp
        Source/UI/MainComponent.cpp
        Source/Plugin/PluginHost.cpp
        Source/Audio/AudioEngine.cpp
        Source/Audio/CaptureSink.cpp
        Source/Mapping/Mapper.cpp
        Source/Export/SfzExporter.cpp
        Source/DSP/SampleProcessor.cpp
        Source/Project/ProjectStore.cpp
)

target_include_directories(K2M
    PRIVATE
        Source
        Source/App
        Source/UI
        Source/Plugin
        Source/Audio
        Source/Sampling
        Source/Mapping
        Source/Export
        Source/DSP
        Source/Project
)

target_compile_features(K2M
    PRIVATE
        cxx_std_20
)

target_compile_definitions(K2M
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_VST=0
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_LADSPA=0
        JUCE_PLUGINHOST_LV2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=0
        JUCE_ASIO=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:K2M,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:K2M,JUCE_VERSION>"
)

target_link_libraries(K2M
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Optional helper: Also make AudioPluginHost available if building extras
option(K2M_BUILD_AUDIO_PLUGIN_HOST "Build JUCE AudioPluginHost for diagnostic reference" OFF)
if(K2M_BUILD_AUDIO_PLUGIN_HOST)
    add_subdirectory(external/JUCE/extras/AudioPluginHost AudioPluginHost)
endif()

# Controlled Test Synthesizer VST3 (para aprovação e validação autônoma do Gate A)
juce_add_plugin(K2M_TestSynth
    PLUGIN_MANUFACTURER_CODE K2MP
    PLUGIN_CODE K2MS
    FORMATS VST3
    PRODUCT_NAME "K2M Test Synth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

target_sources(K2M_TestSynth
    PRIVATE
        Source/TestPlugin/TestSynthProcessor.cpp
)

target_include_directories(K2M_TestSynth
    PRIVATE
        Source/TestPlugin
)

target_compile_features(K2M_TestSynth PRIVATE cxx_std_20)

target_compile_definitions(K2M_TestSynth
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
)

target_link_libraries(K2M_TestSynth
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_core
        juce::juce_graphics
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Gate A automated test runner
add_executable(GateATest
    Tests/GateATest.cpp
    Source/Plugin/PluginHost.cpp
    Source/Audio/CaptureSink.cpp
)

target_include_directories(GateATest
    PRIVATE
        Source
        Source/Plugin
        Source/Audio
        Source/Sampling
)

target_compile_features(GateATest PRIVATE cxx_std_20)

target_compile_definitions(GateATest
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_VST=0
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_LADSPA=0
        JUCE_PLUGINHOST_LV2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(GateATest
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Fase 3 - Gate B fixture: captura real de 2 notas x 2 velocities + exportação SFZ portável
add_executable(GateBFixtureTest
    Tests/GateBFixtureTest.cpp
    Source/Plugin/PluginHost.cpp
    Source/Audio/CaptureSink.cpp
    Source/Mapping/Mapper.cpp
    Source/Export/SfzExporter.cpp
    Source/Project/ProjectStore.cpp
)

target_include_directories(GateBFixtureTest
    PRIVATE
        Source
        Source/Plugin
        Source/Audio
        Source/Sampling
        Source/Mapping
        Source/Export
        Source/Project
)

target_compile_features(GateBFixtureTest PRIVATE cxx_std_20)

target_compile_definitions(GateBFixtureTest
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_VST=0
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_LADSPA=0
        JUCE_PLUGINHOST_LV2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(GateBFixtureTest
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Batch, Mapping, SFZ, DSP and ProjectStore test runner
add_executable(BatchAndSfzTest
    Tests/BatchAndSfzTest.cpp
    Source/Mapping/Mapper.cpp
    Source/Export/SfzExporter.cpp
    Source/DSP/SampleProcessor.cpp
    Source/Project/ProjectStore.cpp
)

target_include_directories(BatchAndSfzTest
    PRIVATE
        Source
        Source/Sampling
        Source/Mapping
        Source/Export
        Source/DSP
        Source/Project
)

target_compile_features(BatchAndSfzTest PRIVATE cxx_std_20)

target_compile_definitions(BatchAndSfzTest
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(BatchAndSfzTest
    PRIVATE
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
