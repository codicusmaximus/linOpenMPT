# linopenmpt_compat: stand-ins for the MFC and Win32 pieces that OpenMPT's
# non-UI code uses (CString, Win32 base types, MFC class skeletons, a few
# functions). Its include directory provides <afx.h> and friends, so upstream
# code that includes them compiles unchanged.
add_library(linopenmpt_compat STATIC src/win32.cpp)
add_library(linopenmpt::compat ALIAS linopenmpt_compat)
target_include_directories(linopenmpt_compat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(linopenmpt_compat PUBLIC MPT_WITH_MFC UNICODE _UNICODE)

# Sources compiled inside mpt_common, because they need upstream's headers.
set(LINOPENMPT_COMPAT_MPT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/mpt_extras.cpp PARENT_SCOPE)

# Same include order as upstream's Windows build: its precompiled header
# (build/pch/PCH.h, which pulls in MFC and the common mpt/ headers) is
# force-included into every C++ file, then our Windows-only mpt extras.
set(LINOPENMPT_FORCE_INCLUDES
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${MPT_UPSTREAM_DIR}/build/pch/PCH.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/include/linopenmpt/mpt_extras.h>"
  PARENT_SCOPE)
