# The Wine bridge for Windows VST2 plugins: linopenmpt-vstbridge.exe, a
# Windows program built with MinGW, which the tracker runs in Wine (see
# protocol.h). Without a MinGW compiler there is no bridge, and the tracker
# says so when asked to load a Windows plugin. With tests, also the bridge's
# test plugin (BridgeTestGain.dll).
find_program(LINOPENMPT_MINGW_CXX NAMES x86_64-w64-mingw32-g++ x86_64-w64-mingw32-g++-win32 x86_64-w64-mingw32-g++-posix)
if(NOT LINOPENMPT_MINGW_CXX)
  message(STATUS "linOpenMPT: no MinGW (x86_64-w64-mingw32-g++), so no bridge for Windows plugins")
  return()
endif()

set(VSTBRIDGE_FLAGS -std=c++20 -O2 -Wall -DNOMINMAX
  -I${MPT_UPSTREAM_DIR} -I${MPT_UPSTREAM_DIR}/src -I${CMAKE_CURRENT_SOURCE_DIR}
  -static -static-libgcc -static-libstdc++)
set(VSTBRIDGE_EXE ${CMAKE_CURRENT_BINARY_DIR}/linopenmpt-vstbridge.exe)
add_custom_command(OUTPUT ${VSTBRIDGE_EXE}
  COMMAND ${LINOPENMPT_MINGW_CXX} ${VSTBRIDGE_FLAGS} -DMPT_INLINE_NS=linopenmpt_vstbridge
    ${CMAKE_CURRENT_SOURCE_DIR}/bridge.cpp -o ${VSTBRIDGE_EXE} -lws2_32
  DEPENDS bridge.cpp protocol.h opcodes.h ${MPT_UPSTREAM_DIR}/mptrack/plugins/VstDefinitions.h
  COMMENT "Building linopenmpt-vstbridge.exe (MinGW)"
  VERBATIM)
add_custom_target(linopenmpt_vstbridge ALL DEPENDS ${VSTBRIDGE_EXE})
install(PROGRAMS ${VSTBRIDGE_EXE} DESTINATION lib/linopenmpt)
set(LINOPENMPT_VSTBRIDGE_EXE ${VSTBRIDGE_EXE} PARENT_SCOPE)

if(LINOPENMPT_BUILD_TESTS)
  set(VSTBRIDGE_TEST_DLL ${CMAKE_CURRENT_BINARY_DIR}/BridgeTestGain.dll)
  add_custom_command(OUTPUT ${VSTBRIDGE_TEST_DLL}
    COMMAND ${LINOPENMPT_MINGW_CXX} ${VSTBRIDGE_FLAGS} -shared -DMPT_INLINE_NS=linopenmpt_vsttest
      ${CMAKE_CURRENT_SOURCE_DIR}/testplugin.cpp -o ${VSTBRIDGE_TEST_DLL} -lgdi32
    DEPENDS testplugin.cpp ${MPT_UPSTREAM_DIR}/mptrack/plugins/VstDefinitions.h
    COMMENT "Building BridgeTestGain.dll (MinGW)"
    VERBATIM)
  add_custom_target(linopenmpt_vstbridge_test_plugin ALL DEPENDS ${VSTBRIDGE_TEST_DLL})
  set(LINOPENMPT_VSTBRIDGE_TEST_DLL ${VSTBRIDGE_TEST_DLL} PARENT_SCOPE)
endif()
