# Source lists for upstream OpenMPT, mirroring upstream's own Makefile and
# build/wine/native_support.mk. Globs are used deliberately so new upstream
# files are picked up automatically after a sync; scripts/update-upstream.sh
# re-runs CMake for that reason.

set(U ${MPT_UPSTREAM_DIR})

file(GLOB MPT_COMMON_SOURCES CONFIGURE_DEPENDS
  ${U}/src/openmpt/all/*.cpp
  ${U}/src/openmpt/base/*.cpp
  ${U}/src/openmpt/logging/*.cpp
  ${U}/src/openmpt/random/*.cpp
  ${U}/common/*.cpp
  ${U}/misc/*.cpp)

file(GLOB MPT_SOUNDLIB_SOURCES CONFIGURE_DEPENDS
  ${U}/src/openmpt/fileformat_base/*.cpp
  ${U}/src/openmpt/soundbase/*.cpp
  ${U}/src/openmpt/soundfile_data/*.cpp
  ${U}/src/openmpt/soundfile_write/*.cpp
  ${U}/soundlib/*.cpp
  ${U}/soundlib/plugins/*.cpp
  ${U}/soundlib/plugins/dmo/*.cpp
  ${U}/sounddsp/*.cpp
  ${U}/unarchiver/*.cpp)

file(GLOB MPT_SOUNDDEVICE_SOURCES CONFIGURE_DEPENDS
  ${U}/src/openmpt/sounddevice/*.cpp)

set(MPT_UPSTREAM_INCLUDE_DIRS
  ${U}
  ${U}/common
  ${U}/soundlib
  ${U}/src
  ${U}/include
  ${U}/include/nlohmann-json/include)
