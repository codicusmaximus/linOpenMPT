# The Qt6 application (plan step 3). For now this only checks that Qt6 is
# available so the toolchain is verified in CI from day one.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)
message(STATUS "Qt6 ${Qt6_VERSION} found; application arrives in plan step 3")
