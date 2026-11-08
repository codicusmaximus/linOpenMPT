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
#include <ctime>
#include <cwchar>

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
#define ASSERT_VALID(p) ASSERT((p) != nullptr)
#else
#define VERIFY(x) assert(x)
#define ASSERT_VALID(p) ASSERT((p) != nullptr)
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

// MFC run-time class information, as in MFC: the tracker compares view
// class names (Childfrm.cpp, MainFrm.cpp) and document templates create
// views and frames through CreateObject().
class CObject;
struct CRuntimeClass
{
	LPCSTR m_lpszClassName;
	int m_nObjectSize;
	UINT m_wSchema;
	CObject *(*m_pfnCreateObject)();
	const CRuntimeClass *m_pBaseClass;

	CObject *CreateObject() const { return m_pfnCreateObject ? m_pfnCreateObject() : nullptr; }
	BOOL IsDerivedFrom(const CRuntimeClass *base) const
	{
		for(const CRuntimeClass *c = this; c != nullptr; c = c->m_pBaseClass)
		{
			if(c == base)
				return TRUE;
		}
		return FALSE;
	}
};
#define RUNTIME_CLASS(c) (const_cast<CRuntimeClass *>(&c::class##c))

#define DECLARE_DYNAMIC(c) \
public: \
	static const CRuntimeClass class##c; \
	CRuntimeClass *GetRuntimeClass() const override;
#define DECLARE_DYNCREATE(c) \
	DECLARE_DYNAMIC(c) \
	static CObject *CreateObject();
#define DECLARE_SERIAL(c) DECLARE_DYNCREATE(c)

#define LINOPENMPT_IMPLEMENT_RUNTIMECLASS(c, b, schema, create) \
	const CRuntimeClass c::class##c = {#c, static_cast<int>(sizeof(c)), schema, create, RUNTIME_CLASS(b)}; \
	CRuntimeClass *c::GetRuntimeClass() const { return RUNTIME_CLASS(c); }
#define IMPLEMENT_DYNAMIC(c, b) LINOPENMPT_IMPLEMENT_RUNTIMECLASS(c, b, 0xFFFF, nullptr)
#define IMPLEMENT_DYNCREATE(c, b) \
	CObject *c::CreateObject() { return new c; } \
	LINOPENMPT_IMPLEMENT_RUNTIMECLASS(c, b, 0xFFFF, c::CreateObject)
#define IMPLEMENT_SERIAL(c, b, s) \
	CObject *c::CreateObject() { return new c; } \
	LINOPENMPT_IMPLEMENT_RUNTIMECLASS(c, b, s, c::CreateObject)

// The compat layer's own MFC classes are header-only: these give them their
// run-time class inline.
#define LINOPENMPT_MFC_DYNAMIC(c) \
public: \
	static const CRuntimeClass class##c; \
	CRuntimeClass *GetRuntimeClass() const override { return RUNTIME_CLASS(c); }
#define LINOPENMPT_MFC_IMPLEMENT_DYNAMIC(c, b) \
	inline const CRuntimeClass c::class##c = {#c, static_cast<int>(sizeof(c)), 0xFFFF, nullptr, RUNTIME_CLASS(b)};

class CObject
{
public:
	static const CRuntimeClass classCObject;
	virtual CRuntimeClass *GetRuntimeClass() const { return RUNTIME_CLASS(CObject); }
	BOOL IsKindOf(const CRuntimeClass *c) const { return GetRuntimeClass()->IsDerivedFrom(c); }
	virtual ~CObject() = default;
};
inline const CRuntimeClass CObject::classCObject = {"CObject", static_cast<int>(sizeof(CObject)), 0xFFFF, nullptr, nullptr};

class CException : public CObject
{
public:
	virtual void Delete() { delete this; }
	virtual BOOL GetErrorMessage(LPTSTR error, UINT maxError, UINT *helpContext = nullptr) const { return FALSE; }
	// Shows the error in a message box (the platform layer overrides the
	// exception classes it throws).
	virtual int ReportError(UINT type = 0x10L /* MB_ICONERROR */, UINT messageID = 0) { return 0; }
};
class CMemoryException : public CException { };
class CFileException : public CException { };

// MFC CTime: seconds since the epoch, local-time formatting.
class CTime
{
	std::time_t m_time = 0;
public:
	CTime() = default;
	explicit CTime(std::time_t t) : m_time(t) { }
	explicit CTime(const FILETIME &ft, int /* dst */ = -1)
	{
		const unsigned long long ticks = (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
		constexpr unsigned long long epochDifference = 116444736000000000ull;  // 1601 to 1970, in 100 ns
		m_time = ticks >= epochDifference ? static_cast<std::time_t>((ticks - epochDifference) / 10000000ull) : 0;
	}
	static CTime GetCurrentTime() { return CTime(std::time(nullptr)); }
	std::time_t GetTime() const { return m_time; }
	CString Format(const wchar_t *fmt) const
	{
		std::tm tm{};
		localtime_r(&m_time, &tm);
		wchar_t buf[256];
		const std::size_t n = std::wcsftime(buf, 256, fmt, &tm);
		return CString(buf, static_cast<int>(n));
	}
	int GetYear() const { std::tm tm{}; localtime_r(&m_time, &tm); return tm.tm_year + 1900; }
	int GetMonth() const { std::tm tm{}; localtime_r(&m_time, &tm); return tm.tm_mon + 1; }
	int GetDay() const { std::tm tm{}; localtime_r(&m_time, &tm); return tm.tm_mday; }
	int GetHour() const { std::tm tm{}; localtime_r(&m_time, &tm); return tm.tm_hour; }
	int GetMinute() const { std::tm tm{}; localtime_r(&m_time, &tm); return tm.tm_min; }
	int GetSecond() const { std::tm tm{}; localtime_r(&m_time, &tm); return tm.tm_sec; }
};

[[noreturn]] inline void AfxThrowMemoryException() { throw new CMemoryException(); }
