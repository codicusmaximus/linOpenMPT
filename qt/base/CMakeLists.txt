# mfcless base: what upstream's tracker configuration (MODPLUG_TRACKER) needs
# on Linux when there is no MFC or Win32 layer. Nothing in here may be named
# after MFC or Win32 (no CString, CWnd, DWORD, ...): upstream code that uses
# such names gets a Qt twin in qt/ or a patch in patches/mfcless/ instead.
#
# Sources compiled inside mpt_common, because they need upstream's headers.
set(LINOPENMPT_COMPAT_MPT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/mpt_extras.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/System.cpp PARENT_SCOPE)
set(LINOPENMPT_FORCE_INCLUDES
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${MPT_UPSTREAM_DIR}/build/pch/PCH.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/include/mfcless/mpt_extras.h>"
  PARENT_SCOPE)
set(LINOPENMPT_COMPAT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)

# The engine includes the tracker's headers ("../mptrack/Moddoc.h"), whose
# twins are Qt classes, so everything built in the tracker configuration
# sees Qt. MPT_MFCLESS marks the few spots in patched upstream files that
# differ from the MFC build.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)
set(CMAKE_AUTOMOC ON)
add_library(mfcless_base STATIC src/IniFile.cpp src/Resources.cpp src/Controls.cpp src/DialogBuilder.cpp src/MessageMap.cpp src/Keyboard.cpp src/Menus.cpp src/Window.cpp src/Graphics.cpp
	src/ListCtrl.cpp src/Button.cpp src/Clipboard.cpp src/DropTarget.cpp src/StatusBar.cpp src/ForeignWindows.cpp
  include/mfcless/Controls.h include/mfcless/DialogBuilder.h include/mfcless/MessageMap.h include/mfcless/Keyboard.h include/mfcless/Menus.h include/mfcless/Window.h include/mfcless/Graphics.h include/mfcless/Geometry.h
  include/mfcless/Button.h include/mfcless/ListCtrl.h include/mfcless/StatusBar.h include/mfcless/DropTarget.h)
target_include_directories(mfcless_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(mfcless_base PUBLIC MPT_MFCLESS)
target_link_libraries(mfcless_base PUBLIC Qt6::Widgets)
# xcb, optional: other programs' X11 windows inside ours (plugin editors).
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(MFCLESS_XCB IMPORTED_TARGET xcb)
endif()
if(MFCLESS_XCB_FOUND)
  target_link_libraries(mfcless_base PRIVATE PkgConfig::MFCLESS_XCB)
  target_compile_definitions(mfcless_base PRIVATE MFCLESS_WITH_XCB)
endif()
