# linopenmpt_platform: Linux implementations of the tracker-application
# services that upstream's engine calls into (settings, main frame, app
# object, document hooks). See docs/ARCHITECTURE.md.
add_library(linopenmpt_platform STATIC stubs.cpp)
target_link_libraries(linopenmpt_platform PUBLIC mpt_soundlib)
target_compile_options(linopenmpt_platform PRIVATE -fpermissive -w)
