# linopenmpt_platform: Linux implementations of the application services
# upstream's engine and app logic call into (the CTrackApp application
# object, main-frame hooks, message boxes, document hooks). Headless for now;
# the Qt application (plan step 3) replaces the UI placeholders.
#
# Upstream's classes derive from MFC's, whose vtables live in linopenmpt_mfc,
# so the platform layer always links it (and through it Qt).
add_subdirectory(qtwin)
add_subdirectory(mfc)

add_library(linopenmpt_platform STATIC TrackApp.cpp UiBoundary.cpp ModDocHooks.cpp)
add_library(linopenmpt::platform ALIAS linopenmpt_platform)
target_include_directories(linopenmpt_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(linopenmpt_platform PUBLIC mpt_tracker_logic linopenmpt_mfc)
target_compile_options(linopenmpt_platform PRIVATE -fpermissive -w)
