# Apply patches/*.patch to upstream/ at configure time, once.
# Each patch is skipped if it is already applied, so re-running CMake is safe.
# See patches/README.md for the policy (keep this list as short as possible).

find_package(Git REQUIRED)
file(GLOB LINOPENMPT_PATCHES ${CMAKE_CURRENT_SOURCE_DIR}/patches/*.patch)
list(SORT LINOPENMPT_PATCHES)
foreach(patch ${LINOPENMPT_PATCHES})
  get_filename_component(name ${patch} NAME)
  execute_process(COMMAND ${GIT_EXECUTABLE} apply --reverse --check ${patch}
    WORKING_DIRECTORY ${MPT_UPSTREAM_DIR} RESULT_VARIABLE already OUTPUT_QUIET ERROR_QUIET)
  if(already EQUAL 0)
    continue()
  endif()
  execute_process(COMMAND ${GIT_EXECUTABLE} apply ${patch}
    WORKING_DIRECTORY ${MPT_UPSTREAM_DIR} RESULT_VARIABLE failed)
  if(failed)
    message(FATAL_ERROR "Upstream patch ${name} does not apply. Refresh it for the current upstream version.")
  endif()
  message(STATUS "Applied upstream patch ${name}")
endforeach()
