// linOpenMPT compatibility layer: MFC/ATL CString.
//
// A value-semantics reimplementation of ATL's CStringT on top of
// std::basic_string, with the subset of the MFC API that OpenMPT uses.
// CString is wide (UNICODE build), as in OpenMPT on Windows.
//
// Differences from MFC worth knowing:
//  - wchar_t is 32-bit on Linux, so CStringW holds UTF-32, not UTF-16.
//    Upstream's mpt::transcode handles this transparently.
//  - Format()/AppendFormat() accept MFC conventions (%s means "same width as
//    this string", %S the other width, %I64d) and translate them for libc.
//  - Narrow/wide implicit conversion is disabled, as upstream requires
//    (_CSTRING_DISABLE_NARROW_WIDE_CONVERSION).

#pragma once

#include "linopenmpt/win32_types.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cwctype>
#include <cctype>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#ifndef _CSTRING_DISABLE_NARROW_WIDE_CONVERSION
#define _CSTRING_DISABLE_NARROW_WIDE_CONVERSION
#endif

namespace linopenmpt::detail
{

template <typename T> struct char_ops;

template <> struct char_ops<char>
{
	static char lower(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
	static char upper(char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }
	static bool space(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }
	static int vformat(char *buf, std::size_t n, const char *fmt, va_list ap) { return std::vsnprintf(buf, n, fmt, ap); }
};

template <> struct char_ops<wchar_t>
{
	static wchar_t lower(wchar_t c) { return static_cast<wchar_t>(std::towlower(static_cast<wint_t>(c))); }
	static wchar_t upper(wchar_t c) { return static_cast<wchar_t>(std::towupper(static_cast<wint_t>(c))); }
	static bool space(wchar_t c) { return std::iswspace(static_cast<wint_t>(c)) != 0; }
	static int vformat(wchar_t *buf, std::size_t n, const wchar_t *fmt, va_list ap) { return std::vswprintf(buf, n, fmt, ap); }
};

// Rewrite an MFC-style printf format string for glibc.
template <typename T>
std::basic_string<T> translate_format(const T *fmt)
{
	constexpr bool wide = sizeof(T) != 1;
	std::basic_string<T> out;
	for(const T *p = fmt; *p; ++p)
	{
		out.push_back(*p);
		if(*p != T('%'))
			continue;
		++p;
		if(*p == T('%'))
		{
			out.push_back(*p);
			continue;
		}
		// flags, width, precision
		while(*p && std::basic_string_view<T>(wide ? (const T *)L"-+ #0123456789.*" : (const T *)"-+ #0123456789.*").find(*p) != std::basic_string_view<T>::npos)
			out.push_back(*p++);
		// length modifiers
		bool hasLength = false;
		if(p[0] == T('I') && p[1] == T('6') && p[2] == T('4'))
		{
			out.append(2, T('l'));
			p += 3;
			hasLength = true;
		} else if(p[0] == T('I') && p[1] == T('3') && p[2] == T('2'))
		{
			p += 3;
			hasLength = true;
		} else if(p[0] == T('I'))
		{
			out.push_back(T('z'));
			++p;
			hasLength = true;
		}
		while(*p == T('h') || *p == T('l') || *p == T('L') || *p == T('z') || *p == T('j') || *p == T('t') || *p == T('q'))
		{
			out.push_back(*p++);
			hasLength = true;
		}
		if(!*p)
			break;
		T conv = *p;
		if(!hasLength && (conv == T('s') || conv == T('c')))
		{
			if(wide)
				out.push_back(T('l'));   // %s in a wide format means wide
			out.push_back(conv);
		} else if(!hasLength && (conv == T('S') || conv == T('C')))
		{
			if(!wide)
				out.push_back(T('l'));   // %S means "the other width"
			out.push_back(static_cast<T>(conv == T('S') ? T('s') : T('c')));
		} else
		{
			out.push_back(conv);
		}
	}
	return out;
}

} // namespace linopenmpt::detail

// The string resource with this id, in both encodings (platform/mfc).
bool LinOpenMPTLoadStringResource(UINT id, std::wstring &wide, std::string &utf8);

template <typename T>
class CStringT
{
public:
	using XCHAR = T;
	using PXSTR = T *;
	using PCXSTR = const T *;
	using YCHAR = std::conditional_t<sizeof(T) == 1, wchar_t, char>;
	using std_string = std::basic_string<T>;

private:
	std_string m_str;
	using ops = linopenmpt::detail::char_ops<T>;
	static std::size_t len(PCXSTR s) { return s ? std::char_traits<T>::length(s) : 0; }

public:
	CStringT() = default;
	CStringT(const CStringT &) = default;
	CStringT(CStringT &&) noexcept = default;
	CStringT &operator=(const CStringT &) = default;
	CStringT &operator=(CStringT &&) noexcept = default;
	CStringT(PCXSTR s) : m_str(s ? s : std_string()) { }
	CStringT(PCXSTR s, int n) : m_str(s ? std_string(s, static_cast<std::size_t>(std::max(n, 0))) : std_string()) { }
	CStringT(T ch, int n = 1) : m_str(static_cast<std::size_t>(std::max(n, 0)), ch) { }
	explicit CStringT(const std_string &s) : m_str(s) { }
	explicit CStringT(std::basic_string_view<T> s) : m_str(s) { }
	CStringT &operator=(PCXSTR s) { m_str = s ? s : std_string(); return *this; }
	CStringT &operator=(T ch) { m_str.assign(1, ch); return *this; }

	// --- access
	int GetLength() const noexcept { return static_cast<int>(m_str.size()); }
	int GetAllocLength() const noexcept { return static_cast<int>(m_str.capacity()); }
	bool IsEmpty() const noexcept { return m_str.empty(); }
	void Empty() noexcept { m_str.clear(); }
	void SetString(const T *str) { m_str = str ? std::basic_string<T>(str) : std::basic_string<T>(); }
	void SetString(const T *str, int length) { m_str.assign(str, static_cast<std::size_t>(length)); }
	PCXSTR GetString() const noexcept { return m_str.c_str(); }
	operator PCXSTR() const noexcept { return m_str.c_str(); }
	T GetAt(int i) const { return m_str.at(static_cast<std::size_t>(i)); }
	void SetAt(int i, T c) { m_str.at(static_cast<std::size_t>(i)) = c; }
	T operator[](int i) const { return m_str[static_cast<std::size_t>(i)]; }
	const std_string &str() const noexcept { return m_str; }

	// --- buffer access (MFC style)
	void Preallocate(int n) { m_str.reserve(static_cast<std::size_t>(std::max(n, 0))); }
	PXSTR GetBuffer() { return m_str.data(); }
	PXSTR GetBuffer(int minLength)
	{
		if(minLength > GetLength())
			m_str.resize(static_cast<std::size_t>(minLength), T(0));
		return m_str.data();
	}
	PXSTR GetBufferSetLength(int n)
	{
		m_str.resize(static_cast<std::size_t>(std::max(n, 0)), T(0));
		return m_str.data();
	}
	void ReleaseBuffer(int newLength = -1)
	{
		if(newLength < 0)
			newLength = static_cast<int>(len(m_str.c_str()));
		m_str.resize(static_cast<std::size_t>(newLength));
	}
	void ReleaseBufferSetLength(int n) { ReleaseBuffer(n); }
	void Truncate(int n) { if(n < GetLength()) m_str.resize(static_cast<std::size_t>(std::max(n, 0))); }
	void FreeExtra() { m_str.shrink_to_fit(); }

	// --- modification
	CStringT &Append(PCXSTR s) { if(s) m_str.append(s); return *this; }
	CStringT &Append(PCXSTR s, int n) { if(s && n > 0) m_str.append(s, static_cast<std::size_t>(n)); return *this; }
	CStringT &Append(const CStringT &s) { m_str.append(s.m_str); return *this; }
	CStringT &AppendChar(T c) { m_str.push_back(c); return *this; }
	CStringT &operator+=(const CStringT &s) { m_str += s.m_str; return *this; }
	CStringT &operator+=(PCXSTR s) { if(s) m_str += s; return *this; }
	CStringT &operator+=(T c) { m_str += c; return *this; }

	int Insert(int i, PCXSTR s)
	{
		i = std::clamp(i, 0, GetLength());
		m_str.insert(static_cast<std::size_t>(i), s ? s : std_string());
		return GetLength();
	}
	int Insert(int i, T c)
	{
		i = std::clamp(i, 0, GetLength());
		m_str.insert(static_cast<std::size_t>(i), 1, c);
		return GetLength();
	}
	int Delete(int i, int n = 1)
	{
		if(i < 0)
			i = 0;
		if(i < GetLength() && n > 0)
			m_str.erase(static_cast<std::size_t>(i), static_cast<std::size_t>(n));
		return GetLength();
	}
	int Replace(T from, T to)
	{
		int count = 0;
		for(auto &c : m_str)
			if(c == from) { c = to; ++count; }
		return count;
	}
	int Replace(PCXSTR from, PCXSTR to)
	{
		const std::size_t fl = len(from);
		if(fl == 0)
			return 0;
		const std_string t = to ? to : std_string();
		int count = 0;
		for(std::size_t pos = m_str.find(from); pos != std_string::npos; pos = m_str.find(from, pos + t.size()))
		{
			m_str.replace(pos, fl, t);
			++count;
		}
		return count;
	}
	int Remove(T c)
	{
		const auto before = m_str.size();
		m_str.erase(std::remove(m_str.begin(), m_str.end(), c), m_str.end());
		return static_cast<int>(before - m_str.size());
	}
	CStringT &MakeLower() { for(auto &c : m_str) c = ops::lower(c); return *this; }
	CStringT &MakeUpper() { for(auto &c : m_str) c = ops::upper(c); return *this; }
	CStringT &MakeReverse() { std::reverse(m_str.begin(), m_str.end()); return *this; }

	CStringT &TrimLeft() { auto it = std::find_if_not(m_str.begin(), m_str.end(), ops::space); m_str.erase(m_str.begin(), it); return *this; }
	CStringT &TrimRight() { while(!m_str.empty() && ops::space(m_str.back())) m_str.pop_back(); return *this; }
	CStringT &Trim() { return TrimRight().TrimLeft(); }
	CStringT &TrimLeft(T c) { m_str.erase(0, m_str.find_first_not_of(c)); return *this; }
	CStringT &TrimRight(T c) { auto p = m_str.find_last_not_of(c); m_str.erase(p == std_string::npos ? 0 : p + 1); return *this; }
	CStringT &Trim(T c) { return TrimRight(c).TrimLeft(c); }
	CStringT &TrimLeft(PCXSTR set) { m_str.erase(0, m_str.find_first_not_of(set)); return *this; }
	CStringT &TrimRight(PCXSTR set) { auto p = m_str.find_last_not_of(set); m_str.erase(p == std_string::npos ? 0 : p + 1); return *this; }
	CStringT &Trim(PCXSTR set) { return TrimRight(set).TrimLeft(set); }

	// --- extraction
	CStringT Left(int n) const { return CStringT(m_str.substr(0, static_cast<std::size_t>(std::clamp(n, 0, GetLength())))); }
	CStringT Right(int n) const
	{
		n = std::clamp(n, 0, GetLength());
		return CStringT(m_str.substr(m_str.size() - static_cast<std::size_t>(n)));
	}
	CStringT Mid(int first) const { return Mid(first, GetLength()); }
	CStringT Mid(int first, int count) const
	{
		first = std::clamp(first, 0, GetLength());
		count = std::clamp(count, 0, GetLength() - first);
		return CStringT(m_str.substr(static_cast<std::size_t>(first), static_cast<std::size_t>(count)));
	}
	CStringT SpanIncluding(PCXSTR set) const { return Left(static_cast<int>(std::min(m_str.find_first_not_of(set), m_str.size()))); }
	CStringT SpanExcluding(PCXSTR set) const { return Left(static_cast<int>(std::min(m_str.find_first_of(set), m_str.size()))); }
	CStringT Tokenize(PCXSTR delims, int &start) const
	{
		if(start < 0 || start >= GetLength())
		{
			start = -1;
			return CStringT();
		}
		std::size_t b = m_str.find_first_not_of(delims, static_cast<std::size_t>(start));
		if(b == std_string::npos)
		{
			start = -1;
			return CStringT();
		}
		std::size_t e = m_str.find_first_of(delims, b);
		if(e == std_string::npos)
			e = m_str.size();
		start = static_cast<int>(std::min(e + 1, m_str.size() + 1));
		return CStringT(m_str.substr(b, e - b));
	}

	// --- searching
	int Find(T c, int start = 0) const { return pos(m_str.find(c, static_cast<std::size_t>(std::max(start, 0)))); }
	int Find(PCXSTR s, int start = 0) const { return pos(m_str.find(s, static_cast<std::size_t>(std::max(start, 0)))); }
	int ReverseFind(T c) const { return pos(m_str.rfind(c)); }
	int FindOneOf(PCXSTR set) const { return pos(m_str.find_first_of(set)); }

	// --- comparison
	int Compare(PCXSTR s) const { return m_str.compare(s ? s : std_string()); }
	int CompareNoCase(PCXSTR s) const
	{
		CStringT a(*this), b(s);
		a.MakeLower();
		b.MakeLower();
		return a.m_str.compare(b.m_str);
	}
	int Collate(PCXSTR s) const { return Compare(s); }
	int CollateNoCase(PCXSTR s) const { return CompareNoCase(s); }

	// --- formatting
	void FormatV(PCXSTR fmt, va_list ap)
	{
		const std_string f = linopenmpt::detail::translate_format(fmt);
		std::vector<T> buf(256);
		for(;;)
		{
			va_list copy;
			va_copy(copy, ap);
			int n = ops::vformat(buf.data(), buf.size(), f.c_str(), copy);
			va_end(copy);
			if(n >= 0 && static_cast<std::size_t>(n) < buf.size())
			{
				m_str.assign(buf.data(), static_cast<std::size_t>(n));
				return;
			}
			buf.resize(n >= 0 ? static_cast<std::size_t>(n) + 1 : buf.size() * 2);
		}
	}
	void Format(PCXSTR fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		FormatV(fmt, ap);
		va_end(ap);
	}
	void AppendFormatV(PCXSTR fmt, va_list ap)
	{
		CStringT tmp;
		tmp.FormatV(fmt, ap);
		m_str += tmp.m_str;
	}
	void AppendFormat(PCXSTR fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		AppendFormatV(fmt, ap);
		va_end(ap);
	}

	// COM string (BSTR); only meaningful for wide strings.
	BSTR AllocSysString() const;

	// A string from the resources (AfxGetResourceHandle()), as in MFC.
	BOOL LoadString(UINT id)
	{
		std::wstring wide;
		std::string utf8;
		const bool found = LinOpenMPTLoadStringResource(id, wide, utf8);
		if constexpr(std::is_same_v<T, wchar_t>)
			m_str = wide;
		else
			m_str = utf8;
		return found ? TRUE : FALSE;
	}

	// --- operators
	friend CStringT operator+(const CStringT &a, const CStringT &b) { CStringT r(a); r += b; return r; }
	friend CStringT operator+(const CStringT &a, PCXSTR b) { CStringT r(a); r += b; return r; }
	friend CStringT operator+(PCXSTR a, const CStringT &b) { CStringT r(a); r += b; return r; }
	friend CStringT operator+(const CStringT &a, T b) { CStringT r(a); r += b; return r; }
	friend CStringT operator+(T a, const CStringT &b) { CStringT r(a); r += b; return r; }
	friend bool operator==(const CStringT &a, const CStringT &b) { return a.m_str == b.m_str; }
	friend bool operator==(const CStringT &a, PCXSTR b) { return a.Compare(b) == 0; }
	friend bool operator==(PCXSTR a, const CStringT &b) { return b.Compare(a) == 0; }
	friend bool operator==(const CStringT &a, T b) { return a.m_str.size() == 1 && a.m_str[0] == b; }
	friend bool operator!=(const CStringT &a, const CStringT &b) { return !(a == b); }
	friend bool operator!=(const CStringT &a, PCXSTR b) { return !(a == b); }
	friend bool operator!=(PCXSTR a, const CStringT &b) { return !(a == b); }
	friend bool operator<(const CStringT &a, const CStringT &b) { return a.m_str < b.m_str; }
	friend bool operator>(const CStringT &a, const CStringT &b) { return a.m_str > b.m_str; }
	friend bool operator<=(const CStringT &a, const CStringT &b) { return a.m_str <= b.m_str; }
	friend bool operator>=(const CStringT &a, const CStringT &b) { return a.m_str >= b.m_str; }

private:
	static int pos(std::size_t p) { return p == std_string::npos ? -1 : static_cast<int>(p); }
};

using CStringA = CStringT<char>;
using CStringW = CStringT<wchar_t>;
using CString = CStringW;

// Hash support so CString works as an unordered_map key.
template <typename T>
struct std::hash<CStringT<T>>
{
	std::size_t operator()(const CStringT<T> &s) const noexcept { return std::hash<std::basic_string<T>>{}(s.str()); }
};
