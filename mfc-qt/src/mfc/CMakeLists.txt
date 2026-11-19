# mfcqt_mfc: the MFC classes upstream's UI is written against (CWnd and
# message maps, CDC and GDI objects, the application object and its message
# loop), implemented on the Win32 API only, as MFC is on Windows. The Win32
# API comes from mfcqt_qtwin.
add_library(mfcqt_mfc STATIC wnd.cpp wndfuncs.cpp gdi.cpp thread.cpp menu.cpp dialog.cpp controls.cpp cmnctrls.cpp tooltips.cpp propsheet.cpp layout.cpp frame.cpp docview.cpp splitter.cpp bars.cpp docking.cpp app.cpp afxextras.cpp)
add_library(mfcqt::mfc ALIAS mfcqt_mfc)
target_include_directories(mfcqt_mfc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mfcqt_mfc PUBLIC mfcqt_qtwin)
