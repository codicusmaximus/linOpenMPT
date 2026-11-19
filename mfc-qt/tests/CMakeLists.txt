# mfc-qt's own tests, offscreen (QT_QPA_PLATFORM=offscreen): the Win32 layer
# (windows, messages, GDI, input), the MFC layer (CWnd and message maps, CDC,
# CWinApp's message loop, touch and accessibility), message map lookup and
# command routing, the common controls, and resources compiled from a
# script of the tests' own (resources.rc).
find_package(Qt6 REQUIRED COMPONENTS Test)

# Targets and tests are named mfcqt_<name>, apart from a host project's.
function(mfcqt_test name)
  add_executable(mfcqt_${name} ${name}.cpp ${ARGN})
  target_link_libraries(mfcqt_${name} PRIVATE mfcqt::mfcqt Qt6::Test)
  add_test(NAME mfcqt_${name} COMMAND mfcqt_${name})
  set_tests_properties(mfcqt_${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

mfcqt_test(qtwin_test)
mfcqt_test(mfc_test)
mfcqt_test(msgmap_test)
mfcqt_test(commctrl_test)
mfcqt_test(resource_test)
mfcqt_add_resources(mfcqt_resource_test ${CMAKE_CURRENT_SOURCE_DIR}/resources.rc FUNCTION RegisterTestResources)
