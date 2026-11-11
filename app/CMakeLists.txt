# The openmpt executable: upstream's tracker (mpt_tracker_ui, compiled
# unchanged) on the MFC layer, the Win32 layer on Qt and the tracker's
# resources, plus the few files that replace upstream's Windows-only ones
# (docs/PORTING-STATUS.md lists them).
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)

if(TARGET mpt_tracker_ui)
  add_executable(openmpt
    main.cpp
    FileDialog.cpp
    WineIntegration.cpp
    $<TARGET_OBJECTS:mpt_tracker_ui>
    $<TARGET_OBJECTS:mpt_tracker_vst>)
  target_compile_options(openmpt PRIVATE -fpermissive -w)
  target_link_libraries(openmpt PRIVATE
    mpt_tracker_logic
    linopenmpt_crash
    linopenmpt_mfc
    linopenmpt_resources
    linopenmpt_qtwin
    linopenmpt_compat
    mpt_rtmidi)
  # The Win32 layer and the code above it call each other both ways.
  set_property(TARGET linopenmpt_compat APPEND PROPERTY LINK_INTERFACE_MULTIPLICITY 3)
  # Symbol names in crash backtraces.
  set_target_properties(openmpt PROPERTIES OUTPUT_NAME openmpt ENABLE_EXPORTS ON)
  install(TARGETS openmpt RUNTIME DESTINATION bin)
endif()

# The real application, started and closed (and, with a JACK server
# available, playing through JACK).
if(TARGET openmpt AND LINOPENMPT_BUILD_TESTS)
  add_test(NAME app_start COMMAND ${CMAKE_SOURCE_DIR}/tools/smoke/app_start_test.sh
    $<TARGET_FILE:openmpt> ${MPT_UPSTREAM_DIR}/test/test.mod)
  # Every view and dialog of the screenshot pass, opened and closed.
  add_test(NAME app_views COMMAND ${CMAKE_SOURCE_DIR}/tools/screenshots/pass.sh
    $<TARGET_FILE:openmpt> ${CMAKE_CURRENT_BINARY_DIR}/app_views)
  set_tests_properties(app_views PROPERTIES TIMEOUT 1200)
  if(LINOPENMPT_PLUGIN_MODULES)
    # The plugin entries use modules make_plugin_module writes.
    set_tests_properties(app_views PROPERTIES FIXTURES_REQUIRED plugin_modules
      ENVIRONMENT "LINOPENMPT_PLUGIN_MODULES=${LINOPENMPT_PLUGIN_MODULES}")
  endif()
  if(LINOPENMPT_PLUGIN_MODULES)
    # A Linux VST2 plugin's own editor, embedded, under Xvfb.
    add_test(NAME app_vst_editor COMMAND ${CMAKE_SOURCE_DIR}/tools/smoke/app_vst_editor_test.sh
      $<TARGET_FILE:openmpt> ${LINOPENMPT_PLUGIN_MODULES}/vst.mptm ${CMAKE_CURRENT_BINARY_DIR}/app_vst_editor)
    set_tests_properties(app_vst_editor PROPERTIES FIXTURES_REQUIRED plugin_modules SKIP_RETURN_CODE 77)
  endif()
  find_program(JACKD_EXECUTABLE jackd)
  if(LINOPENMPT_WITH_JACK AND JACKD_EXECUTABLE)
    add_executable(jack_meter ${CMAKE_SOURCE_DIR}/tools/smoke/jack_meter.c)
    target_link_libraries(jack_meter PRIVATE PkgConfig::JACK m)
    add_test(NAME app_jack_playback COMMAND ${CMAKE_SOURCE_DIR}/tools/smoke/app_jack_test.sh
      $<TARGET_FILE:openmpt> ${MPT_UPSTREAM_DIR}/test/test.mod $<TARGET_FILE:jack_meter>)
  endif()
endif()
