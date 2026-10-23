# Plugin Host: JUCE AU + VST3 test bench (ADR 0004).
# Each installed build gets its own version (tools/install_plugin.sh and
# make_release.sh pass 1.<commits / 100>.<commits % 100>): Ableton re-reads a
# plugin's parameter list only when its version changes. With every build at
# 0.0.1 it kept showing a renamed parameter as a dead one (Resilio, 29 Sep 2026).
# Keep parameter keys (ParamSpec) stable: hosts save automation by them.
set(IV_PLUGIN_VERSION "${PROJECT_VERSION}" CACHE STRING "Plugin version shown to hosts")
# A candidate build for friends (make_release.sh --candidate) gets its own name
# and plugin code, so it installs next to the released plugin for A/B.
set(IV_PLUGIN_NAME "Immensio Versio" CACHE STRING "Plugin name (and bundle name)")
set(IV_PLUGIN_CODE "ImVs" CACHE STRING "4-character plugin code") # Resilio Versio is RsVs
set(IV_PLUGIN_BUNDLE_ID "com.OmbraAudio.ImmensioVersio" CACHE STRING "macOS bundle identifier")
juce_add_plugin(ImmensioVersio
    VERSION ${IV_PLUGIN_VERSION}
    COMPANY_NAME "Ombra Audio"  # the maker both plugins sit under (owner, 23 Oct 2026)
    PLUGIN_MANUFACTURER_CODE Ombr
    PLUGIN_CODE ${IV_PLUGIN_CODE}
    BUNDLE_ID ${IV_PLUGIN_BUNDLE_ID}
    PRODUCT_NAME "${IV_PLUGIN_NAME}"
    FORMATS AU VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE   # MIDI notes are the gate (SPEC §6, ADR 0011)
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    COPY_PLUGIN_AFTER_BUILD ${IV_INSTALL_PLUGIN})

target_sources(ImmensioVersio PRIVATE PluginProcessor.cpp PluginEditor.cpp)

target_compile_definitions(ImmensioVersio PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    # Without this, JUCE's VST3 wrapper exposes ~2080 synthetic "MIDI CC nn"
    # parameters (one per channel/controller, for hosts too old to route raw
    # MIDI) alongside the real 9. That breaks "All ParamSpec params visible
    # and automatable. Names/ranges generated from ParamSpec" (SPEC §7 M2):
    # a host's automation list would be swamped with parameters that aren't
    # in iv::kParams. We accept MIDI natively (NEEDS_MIDI_INPUT TRUE), so the
    # legacy CC-as-parameter bridge is not needed.
    JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0)

target_link_libraries(ImmensioVersio
    PRIVATE iv_core juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Console test bench (SPEC §7 M2): hosts the built AU/VST3 bundles the way a
# DAW does (juce::AudioPluginFormatManager, headless — no GUI needed to load
# and process a plugin). Verifies parameters, MIDI-gate timing, parity with a
# direct Engine render, latency/null-test at MIX 0, and state save/restore.
# Its stimulus is generated in the test, so it needs only the Core.
add_executable(plugin_host_test tests/PluginHostTest.cpp)

target_compile_definitions(plugin_host_test PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

add_dependencies(plugin_host_test ImmensioVersio_VST3)

# The VST3 is a CMake bundle only on macOS; elsewhere (a Linux cloud build)
# ask JUCE for the .vst3 folder it made.
if(APPLE)
    target_compile_definitions(plugin_host_test PRIVATE
        IV_VST3_PATH="$<TARGET_BUNDLE_DIR:ImmensioVersio_VST3>")
else()
    target_compile_definitions(plugin_host_test PRIVATE
        IV_VST3_PATH="$<GENEX_EVAL:$<TARGET_PROPERTY:ImmensioVersio_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")
endif()

if(APPLE)
    target_compile_definitions(plugin_host_test PRIVATE
        IV_AU_PATH="$<TARGET_BUNDLE_DIR:ImmensioVersio_AU>")
    add_dependencies(plugin_host_test ImmensioVersio_AU)
endif()

target_link_libraries(plugin_host_test PRIVATE
    iv_core
    juce::juce_audio_processors_headless
    juce::juce_recommended_warning_flags)

add_test(NAME plugin_host_test COMMAND plugin_host_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(plugin_host_test PROPERTIES TIMEOUT 300)
