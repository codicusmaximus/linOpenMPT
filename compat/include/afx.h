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

// MFC CTime: seconds since the epoch, local-time formatting.
class CTime
{
	std::time_t m_time = 0;
public:
	CTime() = default;
	explicit CTime(std::time_t t) : m_time(t) { }
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
