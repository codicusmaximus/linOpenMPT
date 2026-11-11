# The optional libraries upstream's Windows build has (build/premake/
# mpt-OpenMPT.lua): sample formats, stream export encoders and the
# containers and archives modules come in.
#
# Libraries every Linux distribution packages come from the system, as a
# distribution's own build would use them: FLAC, Vorbis encoding, Opus
# (decoding and encoding), LAME and lhasa. The ones no distribution
# packages in the form OpenMPT needs are built from upstream's copies in
# upstream/include, as upstream does on Windows:
#   ancient  Amiga decompressors (PowerPacker, XPK, MMCMP)
#   UnRAR    RAR archives (upstream's trimmed copy)
#   minizip  ZIP archives, against the system zlib (Debian's libminizip
#            lives in a different include path than upstream expects)
# r8brain and Signalsmith Stretch, also from upstream/include, are part of
# mpt_tracker_logic (OpenMPTCore.cmake).
#
# Everything here is used by mpt_soundlib (sample formats, unarchiver/)
# and mpt_tracker_logic (stream encoders), so the defines are public on
# linopenmpt_external.

find_package(PkgConfig REQUIRED)
pkg_check_modules(MPT_EXT_SYSTEM REQUIRED IMPORTED_TARGET
  flac vorbisenc opus opusfile libopusenc liblhasa)
find_path(MPT_LAME_INCLUDE_DIR lame/lame.h REQUIRED)
find_library(MPT_LAME_LIBRARY mp3lame REQUIRED)

set(X ${MPT_UPSTREAM_DIR}/include)

# ancient: C++17, its API headers as <ancient/ancient.hpp>.
file(GLOB MPT_ANCIENT_SOURCES CONFIGURE_DEPENDS ${X}/ancient/src/*.cpp ${X}/ancient/src/*/*.cpp)
add_library(mpt_ext_ancient STATIC ${MPT_ANCIENT_SOURCES})
target_include_directories(mpt_ext_ancient
  PUBLIC ${X}/ancient/api
  PRIVATE ${X}/ancient/api/ancient ${X}/ancient/src)
target_compile_options(mpt_ext_ancient PRIVATE -w)
set_target_properties(mpt_ext_ancient PROPERTIES POSITION_INDEPENDENT_CODE ON)

# UnRAR: the files and defines of upstream's ext-UnRAR.lua, built as
# Unix code (without isnt.cpp, which is Windows-only as in UnRAR's own
# makefile). unarchiver/unrar.cpp includes "unrar/dll.hpp" from
# upstream/include, which is already on the include path.
foreach(f archive arcread blake2s cmddata consio crc crypt dll encname errhnd
    extinfo extract filcreat file filefn filestr find getbits global hash
    headers largepage list match options pathfn qopen rarvm rawread
    rdwrfn recvol rijndael rs rs16 scantree secpassword sha1 sha256 smallfn
    strfn strlist system threadpool timefn ui unicode unpack volume)
  list(APPEND MPT_UNRAR_SOURCES ${X}/unrar/${f}.cpp)
endforeach()
add_library(mpt_ext_unrar STATIC ${MPT_UNRAR_SOURCES})
target_include_directories(mpt_ext_unrar PRIVATE ${X}/unrar)
target_compile_definitions(mpt_ext_unrar PRIVATE
  _UNIX NOVOLUME UNRAR RAR_NOCRYPT RARDLL SILENT)
target_compile_options(mpt_ext_unrar PRIVATE -w)
target_link_libraries(mpt_ext_unrar PRIVATE Threads::Threads)

# minizip: unarchiver/unzip.cpp includes <contrib/minizip/unzip.h>. Its
# folder is reached through a link in the build tree, because putting
# upstream/include/zlib itself on the path would shadow the system zlib.h.
set(MPT_MINIZIP_INCLUDE ${CMAKE_BINARY_DIR}/ext-include)
file(MAKE_DIRECTORY ${MPT_MINIZIP_INCLUDE}/contrib)
file(CREATE_LINK ${X}/zlib/contrib/minizip ${MPT_MINIZIP_INCLUDE}/contrib/minizip SYMBOLIC)
add_library(mpt_ext_minizip STATIC
  ${X}/zlib/contrib/minizip/ioapi.c
  ${X}/zlib/contrib/minizip/unzip.c)
target_include_directories(mpt_ext_minizip PUBLIC ${MPT_MINIZIP_INCLUDE})
target_compile_options(mpt_ext_minizip PRIVATE -w)
target_link_libraries(mpt_ext_minizip PUBLIC PkgConfig::MPT_DEPS)

add_library(linopenmpt_external INTERFACE)
target_compile_definitions(linopenmpt_external INTERFACE
  MPT_WITH_ANCIENT MPT_WITH_FLAC MPT_WITH_LAME MPT_WITH_LHASA MPT_WITH_MINIZIP
  MPT_WITH_OPUS MPT_WITH_OPUSENC MPT_WITH_OPUSFILE MPT_WITH_UNRAR
  MPT_WITH_VORBISENC)
target_include_directories(linopenmpt_external INTERFACE ${MPT_LAME_INCLUDE_DIR})
target_link_libraries(linopenmpt_external INTERFACE
  mpt_ext_ancient mpt_ext_unrar mpt_ext_minizip
  PkgConfig::MPT_EXT_SYSTEM ${MPT_LAME_LIBRARY})
