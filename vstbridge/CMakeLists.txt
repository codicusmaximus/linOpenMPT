# The Wine bridge for Windows VST2 plugins: linopenmpt-vstbridge.exe (for
# 64-bit plugins) and linopenmpt-vstbridge32.exe (for 32-bit ones), Windows
# programs built with MinGW, which the tracker runs in Wine (see
# protocol.h). Without a MinGW compiler there is no bridge, and the tracker
# says so when asked to load a Windows plugin; without the 32-bit one, only
# 64-bit plugins load. With tests, also the bridge's test plugin
# (BridgeTestGain.dll, and BridgeTestGain32.dll).
find_program(LINOPENMPT_MINGW_CXX NAMES x86_64-w64-mingw32-g++ x86_64-w64-mingw32-g++-win32 x86_64-w64-mingw32-g++-posix)
find_program(LINOPENMPT_MINGW32_CXX NAMES i686-w64-mingw32-g++ i686-w64-mingw32-g++-win32 i686-w64-mingw32-g++-posix)
if(NOT LINOPENMPT_MINGW_CXX)
  message(STATUS "linOpenMPT: no MinGW (x86_64-w64-mingw32-g++), so no bridge for Windows plugins")
  return()
endif()
if(NOT LINOPENMPT_MINGW32_CXX)
  message(STATUS "linOpenMPT: no 32-bit MinGW (i686-w64-mingw32-g++), so no bridge for 32-bit Windows plugins")
endif()

set(VSTBRIDGE_FLAGS -std=c++20 -O2 -Wall
  -I${MPT_UPSTREAM_DIR} -I${MPT_UPSTREAM_DIR}/src -I${CMAKE_CURRENT_SOURCE_DIR}
  -static -static-libgcc -static-libstdc++)

# The bridge and its test plugin for one architecture: suffix "" (64-bit)
# or "32".
function(linopenmpt_add_vstbridge compiler suffix)
  set(exe ${CMAKE_CURRENT_BINARY_DIR}/linopenmpt-vstbridge${suffix}.exe)
  add_custom_command(OUTPUT ${exe}
    COMMAND ${compiler} ${VSTBRIDGE_FLAGS} -DMPT_INLINE_NS=linopenmpt_vstbridge
      ${CMAKE_CURRENT_SOURCE_DIR}/bridge.cpp -o ${exe} -lws2_32
    DEPENDS bridge.cpp protocol.h opcodes.h ${MPT_UPSTREAM_DIR}/mptrack/plugins/VstDefinitions.h
    COMMENT "Building linopenmpt-vstbridge${suffix}.exe (MinGW)"
    VERBATIM)
  add_custom_target(linopenmpt_vstbridge${suffix} ALL DEPENDS ${exe})
  install(PROGRAMS ${exe} DESTINATION lib/linopenmpt)
  set(LINOPENMPT_VSTBRIDGE${suffix}_EXE ${exe} PARENT_SCOPE)

  if(LINOPENMPT_BUILD_TESTS)
    set(dll ${CMAKE_CURRENT_BINARY_DIR}/BridgeTestGain${suffix}.dll)
    add_custom_command(OUTPUT ${dll}
      COMMAND ${compiler} ${VSTBRIDGE_FLAGS} -shared -DMPT_INLINE_NS=linopenmpt_vsttest
        ${CMAKE_CURRENT_SOURCE_DIR}/testplugin.cpp -o ${dll} -lgdi32
      DEPENDS testplugin.cpp ${MPT_UPSTREAM_DIR}/mptrack/plugins/VstDefinitions.h
      COMMENT "Building BridgeTestGain${suffix}.dll (MinGW)"
      VERBATIM)
    add_custom_target(linopenmpt_vstbridge${suffix}_test_plugin ALL DEPENDS ${dll})
    set(LINOPENMPT_VSTBRIDGE${suffix}_TEST_DLL ${dll} PARENT_SCOPE)
  endif()
endfunction()

linopenmpt_add_vstbridge(${LINOPENMPT_MINGW_CXX} "")
set(LINOPENMPT_VSTBRIDGE_EXE ${LINOPENMPT_VSTBRIDGE_EXE} PARENT_SCOPE)
set(LINOPENMPT_VSTBRIDGE_TEST_DLL ${LINOPENMPT_VSTBRIDGE_TEST_DLL} PARENT_SCOPE)
if(LINOPENMPT_MINGW32_CXX)
  linopenmpt_add_vstbridge(${LINOPENMPT_MINGW32_CXX} 32)
  set(LINOPENMPT_VSTBRIDGE32_EXE ${LINOPENMPT_VSTBRIDGE32_EXE} PARENT_SCOPE)
  set(LINOPENMPT_VSTBRIDGE32_TEST_DLL ${LINOPENMPT_VSTBRIDGE32_TEST_DLL} PARENT_SCOPE)
endif()
