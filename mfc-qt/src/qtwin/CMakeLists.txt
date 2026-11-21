# mfcqt_qtwin: Win32 windowing, messages and GDI implemented on Qt6
# widgets. Every HWND is a QWidget; Qt events become window messages, so the
# MFC layer and upstream's window procedures run unchanged on top of it.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)
add_library(mfcqt_qtwin STATIC user32.cpp gdi32.cpp keymap.cpp system.cpp resource.cpp nonclient.cpp menu.cpp dialog.cpp controls.cpp edit.cpp listbox.cpp combobox.cpp msgbox.cpp commctrl.cpp imagelist.cpp tabctrl.cpp headerctrl.cpp listview.cpp treeview.cpp tooltip.cpp toolbar.cpp comboboxex.cpp syslink.cpp statusbar.cpp dragdrop.cpp shell.cpp gdiplus.cpp propsheet.cpp accel.cpp mdi.cpp embed.cpp accessible.cpp)
add_library(mfcqt::qtwin ALIAS mfcqt_qtwin)
target_include_directories(mfcqt_qtwin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mfcqt_qtwin PUBLIC mfcqt_win32 Qt6::Core Qt6::Gui Qt6::Widgets)
set_target_properties(mfcqt_qtwin PROPERTIES AUTOMOC OFF)


# xcb, optional: other programs' X11 windows inside ours (embed.cpp), such as
# plugin editors running in Wine.
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(XCB IMPORTED_TARGET xcb)
endif()
if(XCB_FOUND)
  target_link_libraries(mfcqt_qtwin PRIVATE PkgConfig::XCB)
  target_compile_definitions(mfcqt_qtwin PRIVATE MFCQT_WITH_XCB)
else()
  message(STATUS "mfc-qt: no xcb development files (libxcb1-dev); embedded X11 windows may sit in the wrong place")
endif()
