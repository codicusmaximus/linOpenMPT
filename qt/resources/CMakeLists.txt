# mfcless resources: upstream's mptrack/mptrack.rc, compiled to the standard
# .res format (the one rc.exe writes) and embedded in the executable. The
# twins read menus, dialog templates, strings, accelerators and bitmaps from
# it at run time (mfcless/Resources.h), so upstream's changes to the script
# reach the Qt UI with no work here, and every control sits where the
# template puts it.
#
# The compiler is mfc-qt's (mfc-qt/tools/rc.py, a build tool only; nothing of
# mfc-qt is linked). It preprocesses the script with mfc-qt's Windows headers
# for the resource IDs those define (IDOK, ...).
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
set(tools ${CMAKE_SOURCE_DIR}/mfc-qt/tools)
set(dir ${CMAKE_CURRENT_BINARY_DIR})
set(rc ${dir}/mptrack.rc)
set(res ${dir}/mptrack.res)
add_custom_command(
  OUTPUT ${rc}
  COMMAND ${Python3_EXECUTABLE} ${tools}/prepare_rc.py ${MPT_UPSTREAM_DIR}/mptrack/mptrack.rc ${rc} ${rc}.d
  DEPENDS ${tools}/prepare_rc.py ${MPT_UPSTREAM_DIR}/mptrack/mptrack.rc
  DEPFILE ${rc}.d
  COMMENT "Preparing mptrack.rc"
  VERBATIM)
add_custom_command(
  OUTPUT ${res}
  COMMAND ${Python3_EXECUTABLE} ${tools}/rc.py --cpp ${CMAKE_CXX_COMPILER}
    -I ${CMAKE_SOURCE_DIR}/mfc-qt/rc -I ${MPT_UPSTREAM_DIR}/mptrack -I ${CMAKE_SOURCE_DIR}/qt/base/include
    --depfile ${res}.d ${rc} ${res}
  DEPENDS ${tools}/rc.py ${rc}
  DEPFILE ${res}.d
  COMMENT "Compiling mptrack.rc"
  VERBATIM)

set(source ${dir}/mptrack_res.cpp)
file(WRITE ${source}.in [=[
// Generated: mptrack.res embedded in the binary (qt/resources/CMakeLists.txt).
#include <cstddef>
asm(".section .rodata\n"
	".balign 16\n"
	".globl mfcless_mptrack_res_begin\n"
	".hidden mfcless_mptrack_res_begin\n"
	"mfcless_mptrack_res_begin:\n"
	".incbin \"@RES@\"\n"
	".globl mfcless_mptrack_res_end\n"
	".hidden mfcless_mptrack_res_end\n"
	"mfcless_mptrack_res_end:\n"
	".previous\n");
extern "C" const unsigned char mfcless_mptrack_res_begin[];
extern "C" const unsigned char mfcless_mptrack_res_end[];
namespace mfcless
{
const unsigned char *EmbeddedResources(std::size_t &size)
{
	size = static_cast<std::size_t>(mfcless_mptrack_res_end - mfcless_mptrack_res_begin);
	return mfcless_mptrack_res_begin;
}
} // namespace mfcless
]=])
set(RES ${res})
configure_file(${source}.in ${source} @ONLY)
set_source_files_properties(${source} PROPERTIES OBJECT_DEPENDS ${res})

add_library(mfcless_resources STATIC ${source})
