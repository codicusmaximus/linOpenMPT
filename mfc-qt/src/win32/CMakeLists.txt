# mfcqt_win32: the Win32 and MFC pieces that need no windowing: base
# types and headers (<windows.h>-style declarations, <afx.h> and friends,
# CString and the collections), kernel32 (files, threads, time, INI files),
# winmm's MIDI input, and MFC's message map lookup. Its include directory
# provides the Windows and MFC headers, so code that includes them compiles
# unchanged.
add_library(mfcqt_win32 STATIC win32.cpp win32_kernel.cpp win32_profile.cpp winmm_midi.cpp mfc_msgmap.cpp)
add_library(mfcqt::win32 ALIAS mfcqt_win32)
target_include_directories(mfcqt_win32 PUBLIC
  $<BUILD_INTERFACE:${MFCQT_SOURCE_DIR}/include>)
target_compile_definitions(mfcqt_win32 PUBLIC UNICODE _UNICODE)
# MFC code calls members through null pointers on purpose
# (CWnd::GetSafeHwnd() is "this ? m_hWnd : NULL"); MSVC keeps such checks,
# GCC drops them unless told not to. So for everything built on it too.
target_compile_options(mfcqt_win32 PUBLIC -fno-delete-null-pointer-checks)
target_compile_features(mfcqt_win32 PUBLIC cxx_std_20)
target_link_libraries(mfcqt_win32 PRIVATE ${CMAKE_DL_LIBS})

# MIDI input (midiIn...) through RtMidi, when there is one: the target named
# by MFCQT_RTMIDI_TARGET (an application's own copy), else the system's
# (pkg-config rtmidi). Without, there are no MIDI input devices.
if(MFCQT_RTMIDI_TARGET)
  target_link_libraries(mfcqt_win32 PRIVATE ${MFCQT_RTMIDI_TARGET})
  target_compile_definitions(mfcqt_win32 PRIVATE MFCQT_WITH_RTMIDI=1)
else()
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(MFCQT_RTMIDI IMPORTED_TARGET rtmidi)
  endif()
  if(MFCQT_RTMIDI_FOUND)
    target_link_libraries(mfcqt_win32 PRIVATE PkgConfig::MFCQT_RTMIDI)
    target_compile_definitions(mfcqt_win32 PRIVATE MFCQT_WITH_RTMIDI=1)
  else()
    message(STATUS "mfc-qt: no RtMidi, so no MIDI input devices")
  endif()
endif()
