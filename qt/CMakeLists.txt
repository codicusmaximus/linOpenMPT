# The mfcless tracker: upstream OpenMPT's engine and logic with a native Qt6
# UI and no MFC. Each upstream mptrack/ UI file has a twin here with the same
# name, classes and functions (docs/MFCLESS-MAP.md).
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network)
set(CMAKE_AUTOMOC ON)

add_subdirectory(resources)

# The twins' sources, compiled from their links in the patched upstream tree
# so that their upstream-relative includes ("../soundlib/...") resolve.
# Drafts that qt/tools/twin.py made and nobody has finished yet are kept out
# of the build; the target mfcless_drafts compiles them, to see what is left.
# Re-run cmake after finishing one (taking its draft mark out).
file(GLOB_RECURSE MFCLESS_ALL_TWIN_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/mptrack ${CMAKE_CURRENT_SOURCE_DIR}/mptrack/*.cpp)
set(MFCLESS_TWIN_SOURCES)
set(MFCLESS_DRAFT_SOURCES)
foreach(source ${MFCLESS_ALL_TWIN_SOURCES})
  file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/mptrack/${source} draft LIMIT_COUNT 1 REGEX "Draft from qt/tools/twin.py")
  if(draft)
    list(APPEND MFCLESS_DRAFT_SOURCES ${MPT_UPSTREAM_DIR}/mptrack/${source})
  else()
    list(APPEND MFCLESS_TWIN_SOURCES ${MPT_UPSTREAM_DIR}/mptrack/${source})
  endif()
endforeach()
# Upstream mptrack/ files compiled from the worktree, unchanged or patched
# by patches/mfcless/ (mostly logic, with only a little UI code in them).
set(MFCLESS_UPSTREAM_UI_FILES Moddoc.cpp Modedit.cpp ModConvert.cpp AppendModule.cpp MPTHacks.cpp CommandSet.cpp plugins/MidiInOut.cpp)
list(TRANSFORM MFCLESS_UPSTREAM_UI_FILES PREPEND ${MPT_UPSTREAM_DIR}/mptrack/)
list(APPEND MFCLESS_TWIN_SOURCES ${MFCLESS_UPSTREAM_UI_FILES})
# The crash handler is linOpenMPT's own Linux one (platform/), shared with
# the MFC build.
# Base helpers that need upstream's string types.
# The twin headers are listed too, so that AUTOMOC finds those whose source
# is spelled differently (Mainfrm.h and MainFrm.cpp) or that have none
# (CListCtrl.h).
file(GLOB_RECURSE MFCLESS_TWIN_HEADER_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/mptrack ${CMAKE_CURRENT_SOURCE_DIR}/mptrack/*.h)
list(TRANSFORM MFCLESS_TWIN_HEADER_FILES PREPEND ${MPT_UPSTREAM_DIR}/mptrack/)
add_library(mfcless_ui STATIC ${MFCLESS_TWIN_SOURCES} ${MFCLESS_TWIN_HEADER_FILES} ${CMAKE_SOURCE_DIR}/platform/ExceptionHandler.cpp ${CMAKE_CURRENT_SOURCE_DIR}/base/src/Widgets.cpp)
target_include_directories(mfcless_ui PRIVATE ${CMAKE_SOURCE_DIR}/platform)
# The VST host's Linux side, shared with the MFC build: native (ELF)
# plugins load in-process, Windows ones through the Wine bridge (vstbridge/).
target_sources(mfcless_ui PRIVATE ${CMAKE_SOURCE_DIR}/vstbridge/host.cpp ${CMAKE_SOURCE_DIR}/vstbridge/native.cpp)
target_include_directories(mfcless_ui PRIVATE ${CMAKE_SOURCE_DIR}/vstbridge)
target_compile_definitions(mfcless_ui PRIVATE LINOPENMPT_VSTBRIDGE_INSTALL_DIR="${CMAKE_INSTALL_PREFIX}/lib/linopenmpt")
target_link_libraries(mfcless_ui PUBLIC mpt_tracker_logic mfcless_base mfcless_resources Qt6::Widgets Qt6::Network)
target_compile_options(mfcless_ui PRIVATE -w)

if(MFCLESS_DRAFT_SOURCES)
  add_library(mfcless_drafts OBJECT EXCLUDE_FROM_ALL ${MFCLESS_DRAFT_SOURCES})
  target_link_libraries(mfcless_drafts PRIVATE mpt_tracker_logic mfcless_base mfcless_resources Qt6::Widgets Qt6::Network)
  target_compile_options(mfcless_drafts PRIVATE -w)
endif()

# Each twin header compiled on its own (a check that it is self-contained).
file(GLOB_RECURSE MFCLESS_TWIN_HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/mptrack ${CMAKE_CURRENT_SOURCE_DIR}/mptrack/*.h)
set(MFCLESS_HEADER_CHECKS)
foreach(header ${MFCLESS_TWIN_HEADERS})
  string(MAKE_C_IDENTIFIER ${header} name)
  set(check ${CMAKE_CURRENT_BINARY_DIR}/header_checks/${name}.cpp)
  if(NOT EXISTS ${check})
    file(WRITE ${check} "#include \"${MPT_UPSTREAM_DIR}/mptrack/${header}\"\n")
  endif()
  list(APPEND MFCLESS_HEADER_CHECKS ${check})
endforeach()
add_library(mfcless_header_checks OBJECT EXCLUDE_FROM_ALL ${MFCLESS_HEADER_CHECKS})
target_link_libraries(mfcless_header_checks PRIVATE mpt_tracker_logic mfcless_base Qt6::Widgets)
target_compile_options(mfcless_header_checks PRIVATE -w)
set_target_properties(mfcless_header_checks PROPERTIES AUTOMOC OFF)

add_executable(openmpt-qt main.cpp)
target_link_libraries(openmpt-qt PRIVATE "$<LINK_GROUP:RESCAN,mfcless_ui,mpt_tracker_logic,mpt_soundlib>" mfcless_ui mpt_tracker_logic mfcless_base mfcless_resources)
target_compile_options(openmpt-qt PRIVATE -w)
install(TARGETS openmpt-qt RUNTIME DESTINATION bin)

if(LINOPENMPT_BUILD_TESTS)
  add_executable(mfcless_engine_test tests/engine_test.cpp)
  target_link_libraries(mfcless_engine_test PRIVATE "$<LINK_GROUP:RESCAN,mfcless_ui,mpt_tracker_logic,mpt_soundlib>" mfcless_ui mpt_tracker_logic mfcless_base mfcless_resources)
  target_compile_options(mfcless_engine_test PRIVATE -w)
  add_test(NAME mfcless_engine_mod COMMAND mfcless_engine_test ${MPT_UPSTREAM_DIR}/test/test.mod ${CMAKE_CURRENT_BINARY_DIR}/engine_mod.it)
  add_executable(mfcless_resources_test tests/resources_test.cpp)
  target_link_libraries(mfcless_resources_test PRIVATE mfcless_base mfcless_resources)
  add_test(NAME mfcless_resources COMMAND mfcless_resources_test)
  set_tests_properties(mfcless_resources PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
  add_executable(mfcless_dialogs_test tests/dialogs_test.cpp)
  target_link_libraries(mfcless_dialogs_test PRIVATE mfcless_base mfcless_resources)
  add_test(NAME mfcless_dialogs COMMAND mfcless_dialogs_test)
  set_tests_properties(mfcless_dialogs PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
  add_test(NAME mfcless_engine_s3m COMMAND mfcless_engine_test ${MPT_UPSTREAM_DIR}/test/test.s3m ${CMAKE_CURRENT_BINARY_DIR}/engine_s3m.it)
endif()
