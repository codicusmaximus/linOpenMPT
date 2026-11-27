# Apply patches/*.patch to upstream/ at configure time, once.
# Each patch is skipped if it is already applied, so re-running CMake is safe.
# See patches/README.md for the policy (keep this list as short as possible).
#
# The mfcless build (LINOPENMPT_UI=qt) does not touch upstream/, which the
# MFC build uses as it is. It builds from a git worktree of the submodule in
# the build folder (upstream-mfcless/), with patches/*.patch and then
# patches/mfcless/*.patch applied. To make or refresh an mfcless patch, edit
# the file in that worktree and save `git diff` of it into patches/mfcless/.

find_package(Git REQUIRED)

function(linopenmpt_apply_patches dir)
  foreach(patch ${ARGN})
    get_filename_component(name ${patch} NAME)
    execute_process(COMMAND ${GIT_EXECUTABLE} apply --reverse --check ${patch}
      WORKING_DIRECTORY ${dir} RESULT_VARIABLE already OUTPUT_QUIET ERROR_QUIET)
    if(already EQUAL 0)
      continue()
    endif()
    execute_process(COMMAND ${GIT_EXECUTABLE} apply ${patch}
      WORKING_DIRECTORY ${dir} RESULT_VARIABLE failed)
    if(failed)
      message(FATAL_ERROR "Upstream patch ${name} does not apply in ${dir}. Refresh it for the current upstream version.")
    endif()
    message(STATUS "Applied upstream patch ${name}")
  endforeach()
endfunction()

file(GLOB LINOPENMPT_PATCHES ${CMAKE_CURRENT_SOURCE_DIR}/patches/*.patch)
list(SORT LINOPENMPT_PATCHES)

if(LINOPENMPT_UI STREQUAL "qt")
  set(tree ${CMAKE_BINARY_DIR}/upstream-mfcless)
  execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY ${MPT_UPSTREAM_DIR} OUTPUT_VARIABLE upstream_head OUTPUT_STRIP_TRAILING_WHITESPACE)
  # The worktree is upstream's release with patches/*.patch committed on top
  # (so `git diff` in it shows only mfcless changes). It is made again when
  # either changes.
  set(stamp_text "${upstream_head}")
  foreach(patch ${LINOPENMPT_PATCHES})
    file(SHA256 ${patch} hash)
    string(APPEND stamp_text " ${hash}")
  endforeach()
  set(stamp ${CMAKE_BINARY_DIR}/upstream-mfcless.stamp)
  set(old_stamp "")
  if(EXISTS ${stamp} AND EXISTS ${tree}/.git)
    file(READ ${stamp} old_stamp)
  endif()
  if(NOT old_stamp STREQUAL stamp_text)
    if(EXISTS ${tree})
      execute_process(COMMAND ${GIT_EXECUTABLE} worktree remove --force ${tree}
        WORKING_DIRECTORY ${MPT_UPSTREAM_DIR} OUTPUT_QUIET ERROR_QUIET)
      file(REMOVE_RECURSE ${tree})
    endif()
    execute_process(COMMAND ${GIT_EXECUTABLE} worktree prune WORKING_DIRECTORY ${MPT_UPSTREAM_DIR})
    execute_process(COMMAND ${GIT_EXECUTABLE} worktree add --detach ${tree} ${upstream_head}
      WORKING_DIRECTORY ${MPT_UPSTREAM_DIR} RESULT_VARIABLE failed OUTPUT_QUIET)
    if(failed)
      message(FATAL_ERROR "Could not make the mfcless upstream worktree in ${tree}")
    endif()
    linopenmpt_apply_patches(${tree} ${LINOPENMPT_PATCHES})
    execute_process(COMMAND ${GIT_EXECUTABLE} -c user.name=linopenmpt -c user.email=linopenmpt@localhost
      commit -q -a --allow-empty -m "linopenmpt patches" WORKING_DIRECTORY ${tree})
    file(WRITE ${stamp} "${stamp_text}")
  endif()
  file(GLOB LINOPENMPT_MFCLESS_PATCHES ${CMAKE_CURRENT_SOURCE_DIR}/patches/mfcless/*.patch)
  list(SORT LINOPENMPT_MFCLESS_PATCHES)
  linopenmpt_apply_patches(${tree} ${LINOPENMPT_MFCLESS_PATCHES})
  # The Qt twins of upstream's mptrack/ files (qt/mptrack/) take the place
  # of upstream's in the worktree, as links, so that upstream code including
  # "../mptrack/Moddoc.h" gets the twin.
  file(GLOB_RECURSE LINOPENMPT_TWINS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/qt/mptrack
    ${CMAKE_CURRENT_SOURCE_DIR}/qt/mptrack/*)
  foreach(twin ${LINOPENMPT_TWINS})
    get_filename_component(twin_dir ${tree}/mptrack/${twin} DIRECTORY)
    file(MAKE_DIRECTORY ${twin_dir})
    file(REMOVE ${tree}/mptrack/${twin})
    file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/qt/mptrack/${twin} ${tree}/mptrack/${twin} SYMBOLIC)
  endforeach()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${LINOPENMPT_MFCLESS_PATCHES} ${LINOPENMPT_PATCHES} ${CMAKE_CURRENT_SOURCE_DIR}/qt/mptrack)
  set(MPT_UPSTREAM_DIR ${tree})
else()
  linopenmpt_apply_patches(${MPT_UPSTREAM_DIR} ${LINOPENMPT_PATCHES})
endif()
