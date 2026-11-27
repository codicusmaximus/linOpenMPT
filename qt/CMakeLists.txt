# The mfcless tracker: upstream OpenMPT's engine and logic with a native Qt6
# UI and no MFC. Each upstream mptrack/ UI file has a twin here with the same
# name, classes and functions (docs/MFCLESS-MAP.md).
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)
set(CMAKE_AUTOMOC ON)

add_executable(openmpt-qt main.cpp)
target_link_libraries(openmpt-qt PRIVATE mpt_soundlib mpt_sounddevice Qt6::Widgets)
target_compile_options(openmpt-qt PRIVATE -fpermissive -w)
install(TARGETS openmpt-qt RUNTIME DESTINATION bin)

if(LINOPENMPT_BUILD_TESTS)
  add_executable(mfcless_engine_test tests/engine_test.cpp)
  target_link_libraries(mfcless_engine_test PRIVATE "$<LINK_GROUP:RESCAN,mpt_tracker_logic,mpt_soundlib>" mpt_tracker_logic)
  target_compile_options(mfcless_engine_test PRIVATE -fpermissive -w)
  add_test(NAME mfcless_engine_mod COMMAND mfcless_engine_test ${MPT_UPSTREAM_DIR}/test/test.mod ${CMAKE_CURRENT_BINARY_DIR}/engine_mod.it)
  add_test(NAME mfcless_engine_s3m COMMAND mfcless_engine_test ${MPT_UPSTREAM_DIR}/test/test.s3m ${CMAKE_CURRENT_BINARY_DIR}/engine_s3m.it)
endif()
