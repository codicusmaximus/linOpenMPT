# mfc-qt: MFC and the Win32 API on Qt 6, so that an MFC application builds and
# runs on Linux with its own code unchanged. Every HWND is a QWidget and Qt
# events become window messages (src/qtwin), MFC's classes are implemented on
# that Win32 API as they are on Windows (src/mfc), and the headers
# (<afxwin.h>, <windows.h>-style declarations) are those such code includes.
# An application's .rc file compiles with mfcqt_add_resources().
#
# Use it with add_subdirectory() (or as a git submodule), then link
# mfcqt::mfcqt. See README.md and docs/PORTING.md.
cmake_minimum_required(VERSION 3.22)
project(mfc-qt VERSION 0.1.0 LANGUAGES C CXX)

set(MFCQT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
option(MFCQT_BUILD_TESTS "Build mfc-qt's tests" ${PROJECT_IS_TOP_LEVEL})
set(MFCQT_RTMIDI_TARGET "" CACHE STRING "An RtMidi target for MIDI input (default: the system's, if any)")

if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
  endif()
endif()

find_package(Threads REQUIRED)
add_subdirectory(src/win32)
add_subdirectory(src/qtwin)
add_subdirectory(src/mfc)
include(cmake/MfcQtResources.cmake)

# Everything, for applications: the three layers call each other (a static
# library cycle), so the linker sees them more than once.
set_property(TARGET mfcqt_win32 APPEND PROPERTY LINK_INTERFACE_MULTIPLICITY 3)
add_library(mfcqt INTERFACE)
add_library(mfcqt::mfcqt ALIAS mfcqt)
target_link_libraries(mfcqt INTERFACE mfcqt_mfc mfcqt_qtwin mfcqt_win32)

if(MFCQT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
