# linopenmpt_resources: upstream's mptrack.rc compiled to a Win32 .res file
# and linked into the binary, as rc.exe and the linker do on Windows
# (mfcqt_add_resources). The script is written for rc.exe, with Windows
# paths.
add_library(linopenmpt_resources STATIC)
add_library(linopenmpt::resources ALIAS linopenmpt_resources)
target_link_libraries(linopenmpt_resources PUBLIC mfcqt::mfcqt)
target_include_directories(linopenmpt_resources PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
mfcqt_add_resources(linopenmpt_resources ${MPT_UPSTREAM_DIR}/mptrack/mptrack.rc
  FUNCTION LinOpenMPTRegisterMptrackResources
  WINDOWS_PATHS)
