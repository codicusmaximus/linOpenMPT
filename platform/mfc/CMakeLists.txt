# linopenmpt_mfc: the MFC classes upstream's UI is written against (CWnd and
# message maps, CDC and GDI objects, the application object and its message
# loop), implemented on the Win32 API only, as MFC is on Windows. The Win32
# API comes from linopenmpt_qtwin.
add_library(linopenmpt_mfc STATIC wnd.cpp wndfuncs.cpp gdi.cpp thread.cpp menu.cpp dialog.cpp controls.cpp cmnctrls.cpp tooltips.cpp propsheet.cpp layout.cpp)
add_library(linopenmpt::mfc ALIAS linopenmpt_mfc)
target_include_directories(linopenmpt_mfc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(linopenmpt_mfc PUBLIC linopenmpt_qtwin)
