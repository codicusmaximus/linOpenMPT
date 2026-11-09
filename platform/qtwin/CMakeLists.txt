# linopenmpt_qtwin: Win32 windowing, messages and GDI implemented on Qt6
# widgets. Every HWND is a QWidget; Qt events become window messages, so the
# MFC layer and upstream's window procedures run unchanged on top of it.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)
add_library(linopenmpt_qtwin STATIC user32.cpp gdi32.cpp keymap.cpp system.cpp resource.cpp nonclient.cpp menu.cpp dialog.cpp controls.cpp edit.cpp listbox.cpp combobox.cpp msgbox.cpp commctrl.cpp imagelist.cpp tabctrl.cpp headerctrl.cpp listview.cpp treeview.cpp tooltip.cpp toolbar.cpp comboboxex.cpp syslink.cpp propsheet.cpp accel.cpp)
add_library(linopenmpt::qtwin ALIAS linopenmpt_qtwin)
target_include_directories(linopenmpt_qtwin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(linopenmpt_qtwin PUBLIC linopenmpt_compat Qt6::Core Qt6::Gui Qt6::Widgets)
set_target_properties(linopenmpt_qtwin PROPERTIES AUTOMOC OFF)
