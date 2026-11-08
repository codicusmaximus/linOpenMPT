add_executable(render_module render_module.cpp)
target_link_libraries(render_module PRIVATE mpt_libopenmpt)

add_executable(sounddev_list sounddev_list.cpp)
target_link_libraries(sounddev_list PRIVATE mpt_sounddevice)
target_compile_options(sounddev_list PRIVATE -fpermissive -w)

foreach(f test.mod test.s3m)
  add_test(NAME render_${f} COMMAND render_module ${MPT_UPSTREAM_DIR}/test/${f} 3)
endforeach()
# Structural test files, silent by design: check they load and render.
foreach(f test.xm test.mptm)
  add_test(NAME render_${f} COMMAND render_module ${MPT_UPSTREAM_DIR}/test/${f} 3 --allow-silent)
endforeach()
add_test(NAME sounddev_list COMMAND sounddev_list)
