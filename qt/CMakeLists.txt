# The mfcless tracker: upstream OpenMPT's engine and logic with a native Qt6
# UI and no MFC. Each upstream mptrack/ UI file has a twin here with the same
# name, classes and functions (docs/MFCLESS-MAP.md).
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)
set(CMAKE_AUTOMOC ON)

add_subdirectory(resources)

# The twins' sources, compiled from their links in the patched upstream tree
# so that their upstream-relative includes ("../soundlib/...") resolve.
file(GLOB_RECURSE MFCLESS_TWIN_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/mptrack ${CMAKE_CURRENT_SOURCE_DIR}/mptrack/*.cpp)
list(TRANSFORM MFCLESS_TWIN_SOURCES PREPEND ${MPT_UPSTREAM_DIR}/mptrack/)
add_library(mfcless_ui STATIC ${MFCLESS_TWIN_SOURCES})
target_link_libraries(mfcless_ui PUBLIC mpt_tracker_logic mfcless_base mfcless_resources Qt6::Widgets)
target_compile_options(mfcless_ui PRIVATE -fpermissive -w)

add_executable(openmpt-qt main.cpp)
target_link_libraries(openmpt-qt PRIVATE mpt_soundlib mpt_sounddevice Qt6::Widgets)
target_compile_options(openmpt-qt PRIVATE -fpermissive -w)
install(TARGETS openmpt-qt RUNTIME DESTINATION bin)

if(LINOPENMPT_BUILD_TESTS)
  add_executable(mfcless_engine_test tests/engine_test.cpp)
  target_link_libraries(mfcless_engine_test PRIVATE "$<LINK_GROUP:RESCAN,mpt_tracker_logic,mpt_soundlib>" mpt_tracker_logic)
  target_compile_options(mfcless_engine_test PRIVATE -fpermissive -w)
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
