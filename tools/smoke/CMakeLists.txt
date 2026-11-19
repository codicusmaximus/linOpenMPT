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
  # Stream export encoders and sample decoders, round trip; a module in a
  # ZIP archive (made here with Python's zipfile, as no zip tool is assumed).
  add_executable(codecs_test codecs_test.cpp)
  target_link_libraries(codecs_test PRIVATE mpt_soundlib linopenmpt_platform)
  target_compile_options(codecs_test PRIVATE -fpermissive -w)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_FOUND)
    add_test(NAME codecs_zip COMMAND ${Python3_EXECUTABLE} -c
      "import zipfile; zipfile.ZipFile('${CMAKE_CURRENT_BINARY_DIR}/codecs_test.zip', 'w', zipfile.ZIP_DEFLATED).write('${MPT_UPSTREAM_DIR}/test/test.xm', 'test.xm')")
    set_tests_properties(codecs_zip PROPERTIES FIXTURES_SETUP codecs_zip)
    add_test(NAME codecs_test COMMAND codecs_test ${MPT_UPSTREAM_DIR}/test/test.mod ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/codecs_test.zip)
    set_tests_properties(codecs_test PROPERTIES FIXTURES_REQUIRED codecs_zip)
  else()
    add_test(NAME codecs_test COMMAND codecs_test ${MPT_UPSTREAM_DIR}/test/test.mod ${CMAKE_CURRENT_BINARY_DIR})
  endif()
  set_tests_properties(codecs_test PROPERTIES
    ENVIRONMENT "LINOPENMPT_CONFIG_DIR=${CMAKE_CURRENT_BINARY_DIR}/codecs_test_config")
  # Modules with OpenMPT's built-in plugins, and with one that is missing,
  # for the screenshot pass (app_views).
  add_executable(make_plugin_module make_plugin_module.cpp)
  target_link_libraries(make_plugin_module PRIVATE mpt_soundlib linopenmpt_platform)
  target_compile_options(make_plugin_module PRIVATE -fpermissive -w)
  add_test(NAME make_plugin_module COMMAND make_plugin_module ${MPT_UPSTREAM_DIR}/test/test.mptm
    ${CMAKE_CURRENT_BINARY_DIR}/plugins.mptm ${CMAKE_CURRENT_BINARY_DIR}/missing-plugin.mptm
    ${CMAKE_CURRENT_BINARY_DIR}/vst.mptm ${CMAKE_CURRENT_BINARY_DIR}/opl.mptm ${CMAKE_CURRENT_BINARY_DIR}/wine.mptm)
  set_tests_properties(make_plugin_module PROPERTIES FIXTURES_SETUP plugin_modules
    ENVIRONMENT "LINOPENMPT_CONFIG_DIR=${CMAKE_CURRENT_BINARY_DIR}/make_plugin_module_config")
  set(LINOPENMPT_PLUGIN_MODULES ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
  # Native Linux VST2 hosting with a real plugin: DISTRHO's MVerb
  # (dpf-plugins-vst); skipped when it is not installed.
  set(LINOPENMPT_TEST_VST /usr/lib/vst/MVerb-vst.so CACHE FILEPATH "A Linux VST2 effect plugin for vst_host_test")
  add_executable(vst_host_test vst_host_test.cpp)
  target_link_libraries(vst_host_test PRIVATE mpt_soundlib linopenmpt_platform)
  target_compile_options(vst_host_test PRIVATE -fpermissive -w)
  add_test(NAME vst_host_test COMMAND vst_host_test ${LINOPENMPT_TEST_VST} ${MPT_UPSTREAM_DIR}/test/test.mod)
  set_tests_properties(vst_host_test PROPERTIES SKIP_RETURN_CODE 77
    ENVIRONMENT "LINOPENMPT_CONFIG_DIR=${CMAKE_CURRENT_BINARY_DIR}/vst_host_test_config")
  if(LINOPENMPT_VSTBRIDGE_TEST_DLL)
    # A Windows VST2 plugin through the Wine bridge, in a Wine folder of the
    # build's own (made first, as the tests would race to make it).
    set(LINOPENMPT_TEST_WINEPREFIX ${CMAKE_BINARY_DIR}/wineprefix)
    add_test(NAME wine_prefix COMMAND sh -c "command -v wine >/dev/null || exit 77; WINEDEBUG=-all wineboot --init && wineserver --wait")
    set_tests_properties(wine_prefix PROPERTIES FIXTURES_SETUP wine_prefix SKIP_RETURN_CODE 77 TIMEOUT 300
      ENVIRONMENT "WINEPREFIX=${LINOPENMPT_TEST_WINEPREFIX}")
    add_test(NAME vst_bridge_test COMMAND vst_host_test ${LINOPENMPT_VSTBRIDGE_TEST_DLL} ${MPT_UPSTREAM_DIR}/test/test.mod)
    # Wine processes of one Wine folder share one desktop, on one X display
    # (or none): the tests that run Wine take turns (also app_wine_editor).
    set_tests_properties(vst_bridge_test PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED wine_prefix
      RESOURCE_LOCK wine_display ENVIRONMENT "LINOPENMPT_CONFIG_DIR=${CMAKE_CURRENT_BINARY_DIR}/vst_bridge_test_config;LINOPENMPT_VSTBRIDGE=${LINOPENMPT_VSTBRIDGE_EXE};WINEPREFIX=${LINOPENMPT_TEST_WINEPREFIX}")
    if(LINOPENMPT_VSTBRIDGE32_TEST_DLL)
      # The same plugin built 32-bit, through the 32-bit bridge (found next
      # to the 64-bit one). Without Wine's 32-bit part, skipped.
      add_test(NAME vst_bridge32_test COMMAND vst_host_test ${LINOPENMPT_VSTBRIDGE32_TEST_DLL} ${MPT_UPSTREAM_DIR}/test/test.mod)
      set_tests_properties(vst_bridge32_test PROPERTIES SKIP_RETURN_CODE 77 FIXTURES_REQUIRED wine_prefix
        RESOURCE_LOCK wine_display ENVIRONMENT "LINOPENMPT_CONFIG_DIR=${CMAKE_CURRENT_BINARY_DIR}/vst_bridge32_test_config;LINOPENMPT_VSTBRIDGE=${LINOPENMPT_VSTBRIDGE_EXE};WINEPREFIX=${LINOPENMPT_TEST_WINEPREFIX}")
    endif()
    set(LINOPENMPT_TEST_WINEPREFIX ${LINOPENMPT_TEST_WINEPREFIX} PARENT_SCOPE)
  endif()
  set_tests_properties(tracker_engine_mod tracker_engine_s3m PROPERTIES
    ENVIRONMENT "LINOPENMPT_CONFIG_DIR=${CMAKE_CURRENT_BINARY_DIR}/tracker_engine_config")
endif()

# mfc-qt with OpenMPT's own resources (mptrack.rc): its dialogs, menus,
# frames and bars, offscreen. mfc-qt's own tests are in mfc-qt/tests.

# Upstream's resources (mptrack.rc) through the resource API, offscreen.
if(TARGET linopenmpt_resources)
  add_executable(resource_test resource_test.cpp)
  target_link_libraries(resource_test PRIVATE mfcqt_mfc linopenmpt_resources)
  target_include_directories(resource_test PRIVATE ${MPT_UPSTREAM_DIR}/mptrack)
  target_compile_definitions(resource_test PRIVATE "LINOPENMPT_UPSTREAM_DIR=\"${MPT_UPSTREAM_DIR}\"")
  add_test(NAME resource_test COMMAND resource_test)
  set_tests_properties(resource_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# Borders, scroll bars and menus around the client area, offscreen.
if(TARGET linopenmpt_resources)
  add_executable(nonclient_test nonclient_test.cpp)
  target_link_libraries(nonclient_test PRIVATE mfcqt_mfc linopenmpt_resources)
  target_include_directories(nonclient_test PRIVATE ${MPT_UPSTREAM_DIR}/mptrack)
  add_test(NAME nonclient_test COMMAND nonclient_test)
  set_tests_properties(nonclient_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# The dialog manager and the standard controls, with upstream's dialog
# templates, offscreen.
if(TARGET linopenmpt_resources)
  add_executable(dialog_test dialog_test.cpp)
  target_link_libraries(dialog_test PRIVATE mfcqt_mfc linopenmpt_resources)
  target_include_directories(dialog_test PRIVATE ${MPT_UPSTREAM_DIR}/mptrack)
  add_test(NAME dialog_test COMMAND dialog_test)
  set_tests_properties(dialog_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_executable(frame_test frame_test.cpp)
  target_link_libraries(frame_test PRIVATE mfcqt_mfc linopenmpt_resources)
  target_include_directories(frame_test PRIVATE ${MPT_UPSTREAM_DIR}/mptrack)
  add_test(NAME frame_test COMMAND frame_test)
  set_tests_properties(frame_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_executable(bars_test bars_test.cpp)
  target_link_libraries(bars_test PRIVATE mfcqt_mfc linopenmpt_resources)
  target_include_directories(bars_test PRIVATE ${MPT_UPSTREAM_DIR}/mptrack)
  add_test(NAME bars_test COMMAND bars_test)
  set_tests_properties(bars_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
