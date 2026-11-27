# mfcless base: what upstream's tracker configuration (MODPLUG_TRACKER) needs
# on Linux when there is no MFC or Win32 layer. Nothing in here may be named
# after MFC or Win32 (no CString, CWnd, DWORD, ...): upstream code that uses
# such names gets a Qt twin in qt/ or a patch in patches/mfcless/ instead.
#
# Sources compiled inside mpt_common, because they need upstream's headers.
set(LINOPENMPT_COMPAT_MPT_SOURCES "" PARENT_SCOPE)
set(LINOPENMPT_FORCE_INCLUDES
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${MPT_UPSTREAM_DIR}/build/pch/PCH.h>"
  PARENT_SCOPE)
set(LINOPENMPT_COMPAT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)

add_library(mfcless_base INTERFACE)
target_include_directories(mfcless_base INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
