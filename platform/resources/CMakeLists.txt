# linopenmpt_resources: upstream's mptrack.rc compiled to a Win32 .res file
# and linked into the binary, as rc.exe and the linker do on Windows.
# tools/rc/prepare_rc.py fixes Windows paths in the script, tools/rc/rc.py
# compiles it (llvm-rc and windres can't read all of it).
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)

set(LINOPENMPT_RC_TOOLS ${PROJECT_SOURCE_DIR}/tools/rc)
set(rc_prepared ${CMAKE_CURRENT_BINARY_DIR}/mptrack.rc)
set(rc_res ${CMAKE_CURRENT_BINARY_DIR}/mptrack.res)

add_custom_command(
  OUTPUT ${rc_prepared}
  COMMAND Python3::Interpreter ${LINOPENMPT_RC_TOOLS}/prepare_rc.py
          ${MPT_UPSTREAM_DIR}/mptrack/mptrack.rc ${rc_prepared} ${rc_prepared}.d
  DEPENDS ${LINOPENMPT_RC_TOOLS}/prepare_rc.py ${MPT_UPSTREAM_DIR}/mptrack/mptrack.rc
  DEPFILE ${rc_prepared}.d
  COMMENT "Preparing mptrack.rc"
  VERBATIM)

add_custom_command(
  OUTPUT ${rc_res}
  COMMAND Python3::Interpreter ${LINOPENMPT_RC_TOOLS}/rc.py --cpp ${CMAKE_CXX_COMPILER}
          -I ${PROJECT_SOURCE_DIR}/compat/rc -I ${MPT_UPSTREAM_DIR}/mptrack
          --depfile ${rc_res}.d ${rc_prepared} ${rc_res}
  DEPENDS ${LINOPENMPT_RC_TOOLS}/rc.py ${rc_prepared}
  DEPFILE ${rc_res}.d
  COMMENT "Compiling mptrack.rc"
  VERBATIM)
add_custom_target(linopenmpt_mptrack_res DEPENDS ${rc_res})

add_library(linopenmpt_resources STATIC MptrackResources.cpp)
add_library(linopenmpt::resources ALIAS linopenmpt_resources)
target_link_libraries(linopenmpt_resources PUBLIC linopenmpt_qtwin)
target_compile_definitions(linopenmpt_resources PRIVATE "LINOPENMPT_MPTRACK_RES=\"${rc_res}\"")
set_source_files_properties(MptrackResources.cpp PROPERTIES OBJECT_DEPENDS ${rc_res})
add_dependencies(linopenmpt_resources linopenmpt_mptrack_res)
target_include_directories(linopenmpt_resources PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
