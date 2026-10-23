cmake_minimum_required(VERSION 3.22)
project(immensio_versio VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")

option(IV_BUILD_PLUGIN "Build the JUCE AU/VST3 Plugin (needs libs/JUCE)" ON)
# Off by default: development builds must not replace the plugin the owner is
# using in Ableton. Install a milestone build with tools/install_plugin.sh.
option(IV_INSTALL_PLUGIN "Copy AU/VST3 to ~/Library/Audio/Plug-Ins after building" OFF)

# Core: platform-independent DSP + ParamSpec. No libDaisy, no JUCE.
# Header-only until the first core/dsp/*.cpp lands (M1).
file(GLOB IV_CORE_SOURCES CONFIGURE_DEPENDS core/dsp/*.cpp)
if(IV_CORE_SOURCES)
    add_library(iv_core STATIC ${IV_CORE_SOURCES})
    target_include_directories(iv_core PUBLIC core)
    set_target_properties(iv_core PROPERTIES LINKER_LANGUAGE CXX POSITION_INDEPENDENT_CODE ON)
    if(NOT MSVC)
        target_compile_options(iv_core PRIVATE -Wall -Wextra)
    endif()
else()
    add_library(iv_core INTERFACE)
    target_include_directories(iv_core INTERFACE core)
endif()

# Desktop host helpers (WAV, JSON, FFT, metrics). Header-only or host/common/*.cpp.
file(GLOB IV_HOST_COMMON_SOURCES CONFIGURE_DEPENDS host/common/*.cpp)
add_library(iv_host_common STATIC ${IV_HOST_COMMON_SOURCES} host/common/Wav.h)
target_include_directories(iv_host_common PUBLIC host/common)
target_link_libraries(iv_host_common PUBLIC iv_core)
set_target_properties(iv_host_common PROPERTIES LINKER_LANGUAGE CXX)

file(GLOB IV_RENDER_SOURCES CONFIGURE_DEPENDS host/render/*.cpp)
add_executable(iv_render ${IV_RENDER_SOURCES})
target_link_libraries(iv_render PRIVATE iv_host_common)

# Every host/tests/test_*.cpp is its own test executable with its own main().
enable_testing()
file(GLOB IV_TEST_SOURCES CONFIGURE_DEPENDS host/tests/test_*.cpp)
foreach(src ${IV_TEST_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    add_executable(iv_${name} ${src})
    target_link_libraries(iv_${name} PRIVATE iv_host_common)
    add_test(NAME ${name} COMMAND iv_${name} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach()
add_dependencies(iv_test_set_args iv_render) # runs the real binary

if(IV_BUILD_PLUGIN)
    if(EXISTS ${CMAKE_SOURCE_DIR}/libs/JUCE/CMakeLists.txt)
        add_subdirectory(libs/JUCE EXCLUDE_FROM_ALL)
        add_subdirectory(plugin)
    else()
        message(WARNING "libs/JUCE not found: skipping Plugin. Run: git submodule update --init libs/JUCE")
    endif()
endif()

# The soak harness (docs/research/soak-audit.md): hours of randomised automation,
# gestures and stimuli through the Engine. Not a ctest suite; run it by hand.
add_executable(iv_soak tools/soak/soak.cpp)
target_link_libraries(iv_soak PRIVATE iv_host_common)
