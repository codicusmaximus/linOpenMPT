# linopenmpt_platform: Linux implementations of the application services
# upstream's engine and app logic call into (the CTrackApp application
# object, main-frame hooks, message boxes, document hooks). Headless for now;
# the Qt application (plan step 3) replaces the UI placeholders.
#
# Upstream's classes derive from MFC's, whose vtables live in linopenmpt_mfc,
# so the platform layer always links it (and through it Qt).
add_subdirectory(qtwin)
add_subdirectory(mfc)
add_subdirectory(resources)

# Crash and assertion reporting, shared by the application and the tools.
add_library(linopenmpt_crash STATIC ExceptionHandler.cpp)
target_link_libraries(linopenmpt_crash PUBLIC mpt_tracker_logic)
target_compile_options(linopenmpt_crash PRIVATE -fpermissive -w)

# With upstream's VST host (mpt_tracker_vst), so the tools load plugins.
add_library(linopenmpt_platform STATIC TrackApp.cpp UiBoundary.cpp ModDocHooks.cpp $<TARGET_OBJECTS:mpt_tracker_vst>)
add_library(linopenmpt::platform ALIAS linopenmpt_platform)
target_include_directories(linopenmpt_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(linopenmpt_platform PUBLIC mpt_tracker_logic linopenmpt_crash linopenmpt_mfc linopenmpt_resources)
target_compile_options(linopenmpt_platform PRIVATE -fpermissive -w)
