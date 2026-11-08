// linOpenMPT compatibility layer: stand-in for MFC's <afx.h>.
// Provides the non-UI MFC pieces OpenMPT's shared code relies on.
// UI classes (CWnd, CDialog, ...) are intentionally not provided: the UI is
// rewritten in Qt. See docs/ARCHITECTURE.md.
#pragma once

#include "linopenmpt/win32_types.h"
#include "linopenmpt/win32_api.h"
#include "linopenmpt/mfc_cstring.h"

#include <algorithm>
#include <cassert>

#ifndef ASSERT
#if defined(NDEBUG)
#define ASSERT(x) ((void)0)
#else
#define ASSERT(x) assert(x)
#endif
#endif
#ifndef VERIFY
#if defined(NDEBUG)
#define VERIFY(x) ((void)(x))
#else
#define VERIFY(x) assert(x)
#endif
#endif
#ifndef TRACE
#define TRACE(...) ((void)0)
#endif
#ifndef afx_msg
#define afx_msg
#endif
#ifndef AFXAPI
#define AFXAPI
#endif

// Minimal MFC object roots, enough for class declarations that derive from
// them in shared (non-UI) code.
class CObject
{
public:
	virtual ~CObject() = default;
};

class CException : public CObject
{
public:
	virtual void Delete() { delete this; }
};
class CMemoryException : public CException { };
class CFileException : public CException { };

[[noreturn]] inline void AfxThrowMemoryException() { throw new CMemoryException(); }
