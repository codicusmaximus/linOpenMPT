add_executable(render_module render_module.cpp)
target_link_libraries(render_module PRIVATE mpt_libopenmpt)

add_executable(sounddev_list sounddev_list.cpp)
target_link_libraries(sounddev_list PRIVATE mpt_sounddevice)
target_compile_options(sounddev_list PRIVATE -fpermissive -w)

foreach(f test.mod test.s3m)
  add_test(NAME render_${f} COMMAND render_module ${MPT_UPSTREAM_DIR}/test/${f} 3)
endforeach()
# Structural test files, silent by design: check they load and render.
foreach(f test.xm test.mptm)
  add_test(NAME render_${f} COMMAND render_module ${MPT_UPSTREAM_DIR}/test/${f} 3 --allow-silent)
endforeach()
add_test(NAME sounddev_list COMMAND sounddev_list)

# Plan step 2: tracker-configuration engine (load, render, edit, save, reload).
if(TARGET mpt_soundlib)
  add_executable(tracker_engine tracker_engine.cpp)
  target_link_libraries(tracker_engine PRIVATE mpt_soundlib linopenmpt_platform)
  target_compile_options(tracker_engine PRIVATE -fpermissive -w)
  add_test(NAME tracker_engine_mod COMMAND tracker_engine ${MPT_UPSTREAM_DIR}/test/test.mod ${CMAKE_CURRENT_BINARY_DIR}/tracker_engine_mod.it)
  add_test(NAME tracker_engine_s3m COMMAND tracker_engine ${MPT_UPSTREAM_DIR}/test/test.s3m ${CMAKE_CURRENT_BINARY_DIR}/tracker_engine_s3m.it)
  set_tests_properties(tracker_engine_mod tracker_engine_s3m PROPERTIES
    ENVIRONMENT "LINOPENMPT_CONFIG_DIR=${CMAKE_CURRENT_BINARY_DIR}/tracker_engine_config")
endif()

# MFC message maps in compat/ (command routing used by every menu and dialog).
add_executable(msgmap_test msgmap_test.cpp)
target_link_libraries(msgmap_test PRIVATE linopenmpt_compat)
add_test(NAME msgmap_test COMMAND msgmap_test)
