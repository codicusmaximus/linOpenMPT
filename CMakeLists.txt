cmake_minimum_required(VERSION 3.22)
project(linOpenMPT VERSION 0.1.0 LANGUAGES C CXX)

# linOpenMPT: a native Linux / Qt6 port of the OpenMPT tracker.
# Upstream OpenMPT lives untouched in ./upstream (git submodule, pinned to a release).
# Everything in this repo outside ./upstream is port code.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)          # upstream builds with gnu++20 on Linux
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# MFC code calls members through null pointers on purpose
# (CWnd::GetSafeHwnd() is "this ? m_hWnd : NULL"); MSVC keeps such checks,
# GCC drops them unless told not to.
add_compile_options(-fno-delete-null-pointer-checks)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

set(MPT_UPSTREAM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/upstream)
if(NOT EXISTS ${MPT_UPSTREAM_DIR}/soundlib/Sndfile.h)
  message(FATAL_ERROR "upstream/ is empty. Run: git submodule update --init --depth 1")
endif()

option(LINOPENMPT_WITH_JACK  "JACK audio (lowest latency; also works on PipeWire via pipewire-jack)" ON)
option(LINOPENMPT_WITH_PULSE "PulseAudio audio (also works on PipeWire via pipewire-pulse)" ON)
option(LINOPENMPT_WITH_ALSA  "ALSA audio" ON)
option(LINOPENMPT_BUILD_QT   "Build the Qt6 application (stub until step 3)" ON)
option(LINOPENMPT_BUILD_TESTS "Build smoke tests" ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(UpstreamPatches)
include(OpenMPTSources)
add_subdirectory(compat)
include(OpenMPTCore)
include(OpenMPTSoundDevice)

if(TARGET mpt_soundlib)
  add_subdirectory(platform)
endif()

if(LINOPENMPT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tools/smoke)
endif()

if(LINOPENMPT_BUILD_QT)
  add_subdirectory(app)
endif()
