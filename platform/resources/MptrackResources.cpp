// Upstream's resources (mptrack.rc, compiled by tools/rc/rc.py) embedded in
// the binary, and registered as the executable's resources.
#include <afxwin.h>

#include "MptrackResources.h"

asm(".section .rodata\n"
	".balign 16\n"
	".globl linopenmpt_mptrack_res_begin\n"
	".hidden linopenmpt_mptrack_res_begin\n"
	"linopenmpt_mptrack_res_begin:\n"
	".incbin \"" LINOPENMPT_MPTRACK_RES "\"\n"
	".globl linopenmpt_mptrack_res_end\n"
	".hidden linopenmpt_mptrack_res_end\n"
	"linopenmpt_mptrack_res_end:\n"
	".previous\n");

extern "C" const unsigned char linopenmpt_mptrack_res_begin[];
extern "C" const unsigned char linopenmpt_mptrack_res_end[];

void LinOpenMPTRegisterMptrackResources()
{
	LinOpenMPTRegisterResources(::GetModuleHandle(nullptr), linopenmpt_mptrack_res_begin,
		static_cast<size_t>(linopenmpt_mptrack_res_end - linopenmpt_mptrack_res_begin));
}
