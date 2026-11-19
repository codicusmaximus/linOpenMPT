# OpenMPT's own additions to the MFC and Win32 headers mfc-qt provides:
# pieces of Windows-only mpt code that upstream's non-UI code calls, and the
# plugin bridge's binary type checks.
#
# Sources compiled inside mpt_common, because they need upstream's headers.
set(LINOPENMPT_COMPAT_MPT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/mpt_extras.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/vst_bridge.cpp PARENT_SCOPE)

# Same include order as upstream's Windows build: its precompiled header
# (build/pch/PCH.h, which pulls in MFC and the common mpt/ headers) is
# force-included into every C++ file, then our Windows-only mpt extras.
set(LINOPENMPT_FORCE_INCLUDES
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${MPT_UPSTREAM_DIR}/build/pch/PCH.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/include/linopenmpt/mpt_extras.h>"
  PARENT_SCOPE)
set(LINOPENMPT_COMPAT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)
