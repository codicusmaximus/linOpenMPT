# mfcqt_add_resources: an application's resource script (.rc), compiled to a
# Win32 .res file and linked into a target, as rc.exe and the linker do on
# Windows. FindResource(), LoadString(), dialogs, menus, icons and the rest
# then find it once the generated function has run (at startup, before the
# first window):
#
#   mfcqt_add_resources(<target> <script.rc>
#     FUNCTION <name>              # void <name>(): registers the resources
#                                  # as the executable's (GetModuleHandle(nullptr))
#     [INCLUDE_DIRECTORIES <dir>...]  # for #include in the script (its own
#                                  # directory and mfc-qt's afxres.h are searched)
#     [DEFINITIONS <name[=value]>...]
#     [WINDOWS_PATHS])             # the script was written for Windows: file
#                                  # paths with backslashes, in any case
#
# The script is run through the C preprocessor (the C++ compiler) with
# RC_INVOKED defined, as rc.exe does. mfc-qt/tools/rc.py compiles it (see
# there for the statements it reads: those Visual Studio writes).
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
# By path: the imported target is visible only in this directory.
set(MFCQT_PYTHON ${Python3_EXECUTABLE} CACHE INTERNAL "")
set(MFCQT_RC_TOOLS_DIR ${CMAKE_CURRENT_LIST_DIR}/../tools CACHE INTERNAL "")
set(MFCQT_RC_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/../rc CACHE INTERNAL "")
set(MFCQT_RC_TEMPLATE ${CMAKE_CURRENT_LIST_DIR}/resources.cpp.in CACHE INTERNAL "")

function(mfcqt_add_resources target script)
  cmake_parse_arguments(ARG "WINDOWS_PATHS" "FUNCTION" "INCLUDE_DIRECTORIES;DEFINITIONS" ${ARGN})
  if(NOT ARG_FUNCTION)
    message(FATAL_ERROR "mfcqt_add_resources(${target} ${script}): FUNCTION <name> is required")
  endif()
  get_filename_component(script ${script} ABSOLUTE)
  get_filename_component(script_dir ${script} DIRECTORY)
  get_filename_component(name ${script} NAME_WE)
  set(dir ${CMAKE_CURRENT_BINARY_DIR}/mfcqt_resources/${target}_${name})
  set(res ${dir}/${name}.res)

  set(input ${script})
  if(ARG_WINDOWS_PATHS)
    set(input ${dir}/${name}.rc)
    add_custom_command(
      OUTPUT ${input}
      COMMAND ${MFCQT_PYTHON} ${MFCQT_RC_TOOLS_DIR}/prepare_rc.py ${script} ${input} ${input}.d
      DEPENDS ${MFCQT_RC_TOOLS_DIR}/prepare_rc.py ${script}
      DEPFILE ${input}.d
      COMMENT "Preparing ${name}.rc"
      VERBATIM)
  endif()

  set(args --cpp ${CMAKE_CXX_COMPILER} -I ${MFCQT_RC_INCLUDE_DIR} -I ${script_dir})
  foreach(include IN LISTS ARG_INCLUDE_DIRECTORIES)
    get_filename_component(include ${include} ABSOLUTE)
    list(APPEND args -I ${include})
  endforeach()
  foreach(definition IN LISTS ARG_DEFINITIONS)
    list(APPEND args -D ${definition})
  endforeach()
  add_custom_command(
    OUTPUT ${res}
    COMMAND ${MFCQT_PYTHON} ${MFCQT_RC_TOOLS_DIR}/rc.py ${args} --depfile ${res}.d ${input} ${res}
    DEPENDS ${MFCQT_RC_TOOLS_DIR}/rc.py ${input}
    DEPFILE ${res}.d
    COMMENT "Compiling ${name}.rc"
    VERBATIM)

  # The .res, embedded as it is, and the function that registers it.
  string(MAKE_C_IDENTIFIER "mfcqt_res_${target}_${name}" MFCQT_RES_SYMBOL)
  set(MFCQT_RES_FILE ${res})
  set(MFCQT_RES_FUNCTION ${ARG_FUNCTION})
  set(source ${dir}/${name}_resources.cpp)
  configure_file(${MFCQT_RC_TEMPLATE} ${source} @ONLY)
  set_source_files_properties(${source} PROPERTIES OBJECT_DEPENDS ${res})
  target_sources(${target} PRIVATE ${source})
endfunction()
