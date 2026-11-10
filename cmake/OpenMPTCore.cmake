# Core libraries built from upstream OpenMPT sources.
#
#   mpt_common     common/, misc/, src/openmpt/*  -- tracker configuration
#                  (MODPLUG_TRACKER, MPT_WITH_MFC, UNICODE), exactly as in
#                  OpenMPT.exe, with MFC/Win32 supplied by compat/.
#   mpt_soundlib   soundlib/, sounddsp/, plugins  -- tracker configuration.
#   mpt_tracker_logic  the non-UI files of mptrack/ and tracklib/.
#   mpt_libopenmpt the same soundlib in upstream's *player* configuration
#                  (LIBOPENMPT_BUILD). Builds on Linux today and is used to
#                  prove the toolchain and file loading/rendering work. It is
#                  a stepping stone only; the app will use mpt_soundlib.
#
# The two configurations live in different C++ namespaces upstream
# (global/mptx vs OpenMPT/mpt_libopenmpt), so they never clash.

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(MPT_DEPS REQUIRED IMPORTED_TARGET
  zlib libmpg123 ogg vorbis vorbisfile)
pkg_check_modules(MPT_FLAC IMPORTED_TARGET flac)

option(LINOPENMPT_TRACKER_SOUNDLIB "Build the engine and app logic in tracker configuration" ON)

set(MPT_WARN_OPTS -fpermissive -w)   # upstream's Linux build uses -fpermissive too

# ---- mpt_common (tracker configuration) -----------------------------------
add_library(mpt_common STATIC ${MPT_COMMON_SOURCES} ${LINOPENMPT_COMPAT_MPT_SOURCES})
add_library(linopenmpt::common ALIAS mpt_common)
target_include_directories(mpt_common PUBLIC ${MPT_UPSTREAM_INCLUDE_DIRS})
target_compile_definitions(mpt_common PUBLIC
  MODPLUG_TRACKER
  MPT_BUILD_ENABLE_PCH
  MPT_WITH_NLOHMANNJSON
  $<$<CONFIG:Debug>:MPT_BUILD_DEBUG>)
target_compile_options(mpt_common PRIVATE ${MPT_WARN_OPTS})
target_compile_options(mpt_common PUBLIC ${LINOPENMPT_FORCE_INCLUDES})
target_link_libraries(mpt_common PUBLIC linopenmpt_compat Threads::Threads ${CMAKE_DL_LIBS})

# ---- mpt_soundlib (tracker configuration, step 2) -------------------------
if(LINOPENMPT_TRACKER_SOUNDLIB)
  add_library(mpt_soundlib STATIC ${MPT_SOUNDLIB_SOURCES})
  add_library(linopenmpt::soundlib ALIAS mpt_soundlib)
  target_compile_definitions(mpt_soundlib PUBLIC
    MPT_WITH_ZLIB MPT_WITH_MPG123 MPT_WITH_OGG MPT_WITH_VORBIS MPT_WITH_VORBISFILE)
  target_compile_options(mpt_soundlib PRIVATE ${MPT_WARN_OPTS})
  target_link_libraries(mpt_soundlib PUBLIC mpt_common PkgConfig::MPT_DEPS)

  # The non-UI half of upstream's tracker application (mptrack/), compiled
  # unchanged against compat/. Files are added here as they are ported; the
  # list is the running record of plan step 2.
  set(MPT_TRACKER_LOGIC_SOURCES
    ${MPT_UPSTREAM_DIR}/mptrack/MIDIMapping.cpp
    ${MPT_UPSTREAM_DIR}/mptrack/Settings.cpp
    ${MPT_UPSTREAM_DIR}/mptrack/StreamEncoderSettings.cpp
    ${MPT_UPSTREAM_DIR}/mptrack/TrackerSettings.cpp
    ${MPT_UPSTREAM_DIR}/mptrack/Undo.cpp
    ${MPT_UPSTREAM_DIR}/mptrack/VstPresets.cpp
    ${MPT_UPSTREAM_DIR}/src/openmpt/streamencoder/StreamEncoder.cpp
    ${MPT_UPSTREAM_DIR}/src/openmpt/streamencoder/StreamEncoderAU.cpp
    ${MPT_UPSTREAM_DIR}/src/openmpt/streamencoder/StreamEncoderFLAC.cpp
    ${MPT_UPSTREAM_DIR}/src/openmpt/streamencoder/StreamEncoderMP3.cpp
    ${MPT_UPSTREAM_DIR}/src/openmpt/streamencoder/StreamEncoderOpus.cpp
    ${MPT_UPSTREAM_DIR}/src/openmpt/streamencoder/StreamEncoderRAW.cpp
    ${MPT_UPSTREAM_DIR}/src/openmpt/streamencoder/StreamEncoderVorbis.cpp
    ${MPT_UPSTREAM_DIR}/src/openmpt/streamencoder/StreamEncoderWAV.cpp
    ${MPT_UPSTREAM_DIR}/tracklib/SampleEdit.cpp
    ${MPT_UPSTREAM_DIR}/tracklib/TimeStretchPitchShift.cpp
    ${MPT_UPSTREAM_DIR}/include/r8brain/r8bbase.cpp)
  add_library(mpt_tracker_logic STATIC ${MPT_TRACKER_LOGIC_SOURCES})
  add_library(linopenmpt::tracker_logic ALIAS mpt_tracker_logic)
  target_include_directories(mpt_tracker_logic PUBLIC ${MPT_UPSTREAM_DIR}/mptrack PRIVATE ${MPT_UPSTREAM_DIR}/include/SignalsmithStretch ${MPT_UPSTREAM_DIR}/include/r8brain)
  target_compile_options(mpt_tracker_logic PRIVATE ${MPT_WARN_OPTS})
  target_link_libraries(mpt_tracker_logic PUBLIC mpt_soundlib mpt_sounddevice)

  # The rest of upstream's tracker (app, frames, documents, views, dialogs),
  # compiled unchanged against compat/'s MFC. It is compiled but not linked
  # into anything yet: its MFC and Win32 calls get implementations on top of
  # Qt in plan step 3. Left out are the files that only make sense on
  # Windows; docs/PORTING-STATUS.md lists them and what replaces each.
  file(GLOB MPT_TRACKER_UI_SOURCES CONFIGURE_DEPENDS
    ${MPT_UPSTREAM_DIR}/mptrack/*.cpp
    ${MPT_UPSTREAM_DIR}/mptrack/plugins/*.cpp)
  list(REMOVE_ITEM MPT_TRACKER_UI_SOURCES ${MPT_TRACKER_LOGIC_SOURCES})
  foreach(windows_only
      ExceptionHandler   # crash dumps (dbghelp)
      FileDialog         # Win32 common file dialogs
      HTTP               # WinInet
      MPTrackLink        # MSVC linker directives
      MPTrackUtilWine    # Wine integration
      MPTrackWine        # Wine integration
      UpdateCheck        # WinInet and Windows CNG signature checks
      )
    list(REMOVE_ITEM MPT_TRACKER_UI_SOURCES ${MPT_UPSTREAM_DIR}/mptrack/${windows_only}.cpp)
  endforeach()
  add_library(mpt_tracker_ui OBJECT ${MPT_TRACKER_UI_SOURCES})
  target_compile_options(mpt_tracker_ui PRIVATE ${MPT_WARN_OPTS})
  target_link_libraries(mpt_tracker_ui PUBLIC mpt_tracker_logic)
endif()

# ---- mpt_libopenmpt (player configuration, stepping stone) ----------------
file(GLOB MPT_LIBOPENMPT_SOURCES CONFIGURE_DEPENDS
  ${MPT_UPSTREAM_DIR}/src/openmpt/all/*.cpp
  ${MPT_UPSTREAM_DIR}/src/openmpt/base/*.cpp
  ${MPT_UPSTREAM_DIR}/src/openmpt/logging/*.cpp
  ${MPT_UPSTREAM_DIR}/src/openmpt/random/*.cpp
  ${MPT_UPSTREAM_DIR}/common/*.cpp
  ${MPT_UPSTREAM_DIR}/src/openmpt/fileformat_base/*.cpp
  ${MPT_UPSTREAM_DIR}/src/openmpt/soundbase/*.cpp
  ${MPT_UPSTREAM_DIR}/src/openmpt/soundfile_data/*.cpp
  ${MPT_UPSTREAM_DIR}/soundlib/*.cpp
  ${MPT_UPSTREAM_DIR}/soundlib/plugins/*.cpp
  ${MPT_UPSTREAM_DIR}/soundlib/plugins/dmo/*.cpp
  ${MPT_UPSTREAM_DIR}/sounddsp/*.cpp
  ${MPT_UPSTREAM_DIR}/libopenmpt/*.cpp)
add_library(mpt_libopenmpt STATIC ${MPT_LIBOPENMPT_SOURCES})
add_library(linopenmpt::libopenmpt ALIAS mpt_libopenmpt)
target_include_directories(mpt_libopenmpt
  PRIVATE ${MPT_UPSTREAM_INCLUDE_DIRS}
  PUBLIC  ${MPT_UPSTREAM_DIR})   # <libopenmpt/libopenmpt.hpp>
target_compile_definitions(mpt_libopenmpt PRIVATE
  LIBOPENMPT_BUILD MPT_WITH_ZLIB MPT_WITH_MPG123 MPT_WITH_OGG MPT_WITH_VORBIS MPT_WITH_VORBISFILE)
target_compile_definitions(mpt_libopenmpt PUBLIC LIBOPENMPT_USE_STATIC)
target_compile_options(mpt_libopenmpt PRIVATE ${MPT_WARN_OPTS})
target_link_libraries(mpt_libopenmpt PUBLIC PkgConfig::MPT_DEPS Threads::Threads)
