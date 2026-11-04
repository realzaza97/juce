include(FetchContent)

# Use a local JUCE checkout if -DJUCE_DIR=... is given, otherwise download a pinned release.
if(JUCE_DIR)
  add_subdirectory(${JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)
  FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Vocal2Instrument
  COMPANY_NAME                "Vocal2Instrument"
  PLUGIN_MANUFACTURER_CODE    V2iM
  PLUGIN_CODE                 V2i1
  PRODUCT_NAME                "Vocal2Instrument"
  FORMATS                     VST3 Standalone
  IS_SYNTH                    TRUE          # shows up as an instrument/generator (FL Studio routes its MIDI out from these)
  NEEDS_MIDI_INPUT            FALSE
  NEEDS_MIDI_OUTPUT           TRUE
  IS_MIDI_EFFECT              FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE          # Delete key in the piano roll
  COPY_PLUGIN_AFTER_BUILD     FALSE         # set TRUE to auto-copy the VST3 to C:/Program Files/Common Files/VST3
  VST3_CATEGORIES             Instrument
  BUNDLE_ID                   com.vocal2instrument.plugin)

target_sources(Vocal2Instrument PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/ui/WaveformView.cpp
  Source/ui/PianoRollView.cpp)

target_compile_definitions(Vocal2Instrument PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_USE_MP3AUDIOFORMAT=1        # MP3 import (read-only decoder bundled with JUCE)
  JUCE_DISPLAY_SPLASH_SCREEN=1)    # keep 1 unless you hold a JUCE commercial licence (see README)

target_link_libraries(Vocal2Instrument
  PRIVATE
    v2i_core
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
