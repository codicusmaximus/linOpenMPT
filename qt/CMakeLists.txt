# The mfcless tracker: upstream OpenMPT's engine and logic with a native Qt6
# UI and no MFC. Each upstream mptrack/ UI file has a twin here with the same
# name, classes and functions (docs/MFCLESS-MAP.md).
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)
set(CMAKE_AUTOMOC ON)

add_executable(openmpt-qt main.cpp)
target_link_libraries(openmpt-qt PRIVATE mpt_soundlib mpt_sounddevice Qt6::Widgets)
target_compile_options(openmpt-qt PRIVATE -fpermissive -w)
install(TARGETS openmpt-qt RUNTIME DESTINATION bin)
