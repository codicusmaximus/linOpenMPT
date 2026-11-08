// linOpenMPT compatibility layer: the Win32 "private profile" (INI file) API.
//
// OpenMPT stores its settings through GetPrivateProfileString and friends
// (mptrack/Settings.cpp). This reimplements those functions with Windows'
// semantics: case-insensitive section and key names, original order kept,
// binary structs stored as hex plus a checksum byte, NULL key or value to
// delete. Files are written as UTF-8; UTF-16LE files (as written by OpenMPT
// on Windows) and UTF-8 files with a BOM are read too, so an existing
// OpenMPT configuration can be copied over.

#include "linopenmpt/win32_api.h"

#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <sys/stat.h>

namespace
{

// ---- text encoding -------------------------------------------------------

std::wstring DecodeUTF8(const std::string &s)
{
	std::wstring out;
	out.reserve(s.size());
	for(std::size_t i = 0; i < s.size();)
	{
		const unsigned char c = static_cast<unsigned char>(s[i]);
		char32_t cp;
		int extra;
		if(c < 0x80) { cp = c; extra = 0; }
		else if((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
		else if((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
		else if((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
		else { out.push_back(static_cast<wchar_t>(c)); ++i; continue; }   // invalid: treat as Latin-1
		if(i + extra >= s.size() + (extra ? 0 : 1) && extra)
		{
			out.push_back(static_cast<wchar_t>(c));
			++i;
			continue;
		}
		bool ok = true;
		for(int k = 1; k <= extra; ++k)
		{
			const unsigned char cc = static_cast<unsigned char>(s[i + k]);
			if((cc & 0xC0) != 0x80) { ok = false; break; }
			cp = (cp << 6) | (cc & 0x3F);
		}
		if(!ok)
		{
			out.push_back(static_cast<wchar_t>(c));
			++i;
			continue;
		}
		out.push_back(static_cast<wchar_t>(cp));
		i += 1 + extra;
	}
	return out;
}

std::string EncodeUTF8(const std::wstring &s)
{
	std::string out;
	out.reserve(s.size());
	for(wchar_t wc : s)
	{
		const char32_t cp = static_cast<char32_t>(wc);
		if(cp < 0x80) out.push_back(static_cast<char>(cp));
		else if(cp < 0x800) { out.push_back(static_cast<char>(0xC0 | (cp >> 6))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
		else if(cp < 0x10000) { out.push_back(static_cast<char>(0xE0 | (cp >> 12))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
		else { out.push_back(static_cast<char>(0xF0 | (cp >> 18))); out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
	}
	return out;
}

std::wstring DecodeUTF16LE(const std::string &s, std::size_t start)
{
	std::wstring out;
	for(std::size_t i = start; i + 1 < s.size(); i += 2)
	{
		char32_t u = static_cast<unsigned char>(s[i]) | (static_cast<unsigned char>(s[i + 1]) << 8);
		if(u >= 0xD800 && u < 0xDC00 && i + 3 < s.size())
		{
			const char32_t lo = static_cast<unsigned char>(s[i + 2]) | (static_cast<unsigned char>(s[i + 3]) << 8);
			if(lo >= 0xDC00 && lo < 0xE000)
			{
				u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
				i += 2;
			}
		}
		out.push_back(static_cast<wchar_t>(u));
	}
	return out;
}

std::wstring Lower(std::wstring s)
{
	for(auto &c : s)
		c = static_cast<wchar_t>(std::towlower(static_cast<wint_t>(c)));
	return s;
}

std::wstring Trim(const std::wstring &s)
{
	const auto b = s.find_first_not_of(L" \t\r");
	if(b == std::wstring::npos)
		return {};
	const auto e = s.find_last_not_of(L" \t\r");
	return s.substr(b, e - b + 1);
}

// ---- in-memory INI document ------------------------------------------------

struct Entry
{
	std::wstring key;   // empty for comment / blank lines
	std::wstring value; // raw line when key is empty
};
struct Section
{
	std::wstring name;
	std::vector<Entry> entries;
};
struct Document
{
	std::vector<Section> sections;   // sections[0] holds lines before the first header
	std::optional<std::filesystem::file_time_type> mtime;
	std::uintmax_t size = 0;

	Section *Find(const std::wstring &name)
	{
		const std::wstring l = Lower(name);
		for(auto &s : sections)
			if(&s != &sections[0] && Lower(s.name) == l)
				return &s;
		return nullptr;
	}
	static Entry *FindKey(Section &s, const std::wstring &key)
	{
		const std::wstring l = Lower(key);
		for(auto &e : s.entries)
			if(!e.key.empty() && Lower(e.key) == l)
				return &e;
		return nullptr;
	}
};

std::mutex g_mutex;
std::map<std::string, Document> g_cache;

Document Parse(const std::string &bytes)
{
	std::wstring text;
	if(bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE)
		text = DecodeUTF16LE(bytes, 2);
	else if(bytes.size() >= 3 && bytes.compare(0, 3, "\xEF\xBB\xBF") == 0)
		text = DecodeUTF8(bytes.substr(3));
	else
		text = DecodeUTF8(bytes);

	Document doc;
	doc.sections.push_back(Section{});
	std::wstring line;
	std::wistringstream in(text);
	while(std::getline(in, line))
	{
		if(!line.empty() && line.back() == L'\r')
			line.pop_back();
		const std::wstring t = Trim(line);
		if(t.size() >= 2 && t.front() == L'[' && t.find(L']') != std::wstring::npos)
		{
			doc.sections.push_back(Section{t.substr(1, t.find(L']') - 1), {}});
			continue;
		}
		const auto eq = line.find(L'=');
		if(eq != std::wstring::npos && !t.empty() && t[0] != L';' && t[0] != L'#')
		{
			const std::wstring key = Trim(line.substr(0, eq));
			if(!key.empty())
			{
				doc.sections.back().entries.push_back(Entry{key, Trim(line.substr(eq + 1))});
				continue;
			}
		}
		doc.sections.back().entries.push_back(Entry{{}, line});
	}
	return doc;
}

Document &Load(const std::string &file)
{
	namespace fs = std::filesystem;
	std::error_code ec;
	const auto mtime = fs::last_write_time(file, ec);
	const bool exists = !ec;
	const auto size = exists ? fs::file_size(file, ec) : 0;
	auto it = g_cache.find(file);
	if(it != g_cache.end())
	{
		const Document &d = it->second;
		if((!exists && !d.mtime) || (exists && d.mtime && *d.mtime == mtime && d.size == size))
			return it->second;
	}
	Document doc;
	if(exists)
	{
		std::ifstream f(file, std::ios::binary);
		std::ostringstream ss;
		ss << f.rdbuf();
		doc = Parse(ss.str());
		doc.mtime = mtime;
		doc.size = size;
	} else
	{
		doc.sections.push_back(Section{});
	}
	return g_cache[file] = std::move(doc);
}

bool Save(const std::string &file, Document &doc)
{
	std::wstring text;
	for(std::size_t i = 0; i < doc.sections.size(); ++i)
	{
		const Section &s = doc.sections[i];
		if(i > 0)
			text += L"[" + s.name + L"]\n";
		for(const Entry &e : s.entries)
			text += e.key.empty() ? e.value + L"\n" : e.key + L"=" + e.value + L"\n";
	}
	const std::string bytes = EncodeUTF8(text);
	const std::string tmp = file + ".tmp";
	{
		std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
		if(!f)
			return false;
		f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		if(!f)
			return false;
	}
	std::error_code ec;
	std::filesystem::rename(tmp, file, ec);
	if(ec)
		return false;
	doc.mtime = std::filesystem::last_write_time(file, ec);
	doc.size = bytes.size();
	return true;
}

std::optional<std::wstring> Lookup(const char *file, const wchar_t *section, const wchar_t *key)
{
	Document &doc = Load(file);
	Section *s = doc.Find(section);
	if(!s)
		return std::nullopt;
	Entry *e = Document::FindKey(*s, key);
	if(!e)
		return std::nullopt;
	return e->value;
}

bool Store(const char *file, const wchar_t *section, const wchar_t *key, const wchar_t *value)
{
	Document &doc = Load(file);
	Section *s = doc.Find(section);
	if(!key)
	{
		// delete the whole section
		if(s)
			doc.sections.erase(doc.sections.begin() + (s - doc.sections.data()));
		return Save(file, doc);
	}
	if(!s)
	{
		if(!value)
			return true;
		doc.sections.push_back(Section{section, {}});
		s = &doc.sections.back();
	}
	Entry *e = Document::FindKey(*s, key);
	if(!value)
	{
		if(e)
			s->entries.erase(s->entries.begin() + (e - s->entries.data()));
	} else if(e)
	{
		e->value = value;
	} else
	{
		// insert after the last key, before trailing blank lines
		auto pos = s->entries.end();
		while(pos != s->entries.begin() && std::prev(pos)->key.empty() && Trim(std::prev(pos)->value).empty())
			--pos;
		s->entries.insert(pos, Entry{key, value});
	}
	return Save(file, doc);
}

} // namespace

DWORD GetPrivateProfileString(LPCWSTR section, LPCWSTR key, LPCWSTR def, LPWSTR out, DWORD size, const char *file)
{
	if(!out || size == 0)
		return 0;
	std::lock_guard<std::mutex> lock(g_mutex);
	std::wstring value;
	if(section && key)
	{
		auto v = Lookup(file, section, key);
		value = v ? *v : Trim(def ? def : L"");
	} else
	{
		// Section or key enumeration (double-NUL-terminated list).
		Document &doc = Load(file);
		if(!section)
		{
			for(std::size_t i = 1; i < doc.sections.size(); ++i)
				value += doc.sections[i].name + L'\0';
		} else if(Section *s = doc.Find(section))
		{
			for(const Entry &e : s->entries)
				if(!e.key.empty())
					value += e.key + L'\0';
		}
		if(value.size() + 1 > size)
		{
			if(size >= 2)
			{
				std::wmemcpy(out, value.data(), size - 2);
				out[size - 2] = 0;
				out[size - 1] = 0;
			} else
			{
				out[0] = 0;
			}
			return size - 2;
		}
		std::wmemcpy(out, value.data(), value.size());
		out[value.size()] = 0;
		return static_cast<DWORD>(value.empty() ? 0 : value.size());
	}
	// Windows strips a single pair of surrounding quotes.
	if(value.size() >= 2 && ((value.front() == L'"' && value.back() == L'"') || (value.front() == L'\'' && value.back() == L'\'')))
		value = value.substr(1, value.size() - 2);
	const std::size_t n = std::min<std::size_t>(value.size(), size - 1);
	std::wmemcpy(out, value.data(), n);
	out[n] = 0;
	return static_cast<DWORD>(n);
}

UINT GetPrivateProfileInt(LPCWSTR section, LPCWSTR key, INT def, const char *file)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	auto v = Lookup(file, section, key);
	if(!v)
		return static_cast<UINT>(def);
	const std::wstring t = Trim(*v);
	if(t.empty())
		return 0;
	std::size_t i = 0;
	bool neg = false;
	if(t[0] == L'-' || t[0] == L'+')
	{
		neg = t[0] == L'-';
		++i;
	}
	long long result = 0;
	int base = 10;
	if(i + 1 < t.size() && t[i] == L'0' && (t[i + 1] == L'x' || t[i + 1] == L'X'))
	{
		base = 16;
		i += 2;
	}
	for(; i < t.size(); ++i)
	{
		int d;
		if(t[i] >= L'0' && t[i] <= L'9') d = t[i] - L'0';
		else if(base == 16 && t[i] >= L'a' && t[i] <= L'f') d = t[i] - L'a' + 10;
		else if(base == 16 && t[i] >= L'A' && t[i] <= L'F') d = t[i] - L'A' + 10;
		else break;
		result = result * base + d;
	}
	return static_cast<UINT>(neg ? -result : result);
}

BOOL GetPrivateProfileStruct(LPCWSTR section, LPCWSTR key, LPVOID data, UINT size, const char *file)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	auto v = Lookup(file, section, key);
	if(!v)
		return FALSE;
	const std::wstring t = Trim(*v);
	if(t.size() != 2u * (size + 1))
		return FALSE;
	auto hex = [](wchar_t c) -> int {
		if(c >= L'0' && c <= L'9') return c - L'0';
		if(c >= L'A' && c <= L'F') return c - L'A' + 10;
		if(c >= L'a' && c <= L'f') return c - L'a' + 10;
		return -1;
	};
	std::vector<BYTE> bytes(size + 1);
	for(UINT i = 0; i <= size; ++i)
	{
		const int hi = hex(t[2 * i]), lo = hex(t[2 * i + 1]);
		if(hi < 0 || lo < 0)
			return FALSE;
		bytes[i] = static_cast<BYTE>(hi * 16 + lo);
	}
	BYTE sum = 0;
	for(UINT i = 0; i < size; ++i)
		sum = static_cast<BYTE>(sum + bytes[i]);
	if(sum != bytes[size])
		return FALSE;
	std::memcpy(data, bytes.data(), size);
	return TRUE;
}

BOOL WritePrivateProfileString(LPCWSTR section, LPCWSTR key, LPCWSTR value, const char *file)
{
	if(!section)
		return FALSE;
	std::lock_guard<std::mutex> lock(g_mutex);
	return Store(file, section, key, value) ? TRUE : FALSE;
}

BOOL WritePrivateProfileStruct(LPCWSTR section, LPCWSTR key, LPVOID data, UINT size, const char *file)
{
	if(!section || !key)
		return FALSE;
	static const wchar_t digits[] = L"0123456789ABCDEF";
	std::wstring hex;
	BYTE sum = 0;
	if(data)
	{
		const BYTE *p = static_cast<const BYTE *>(data);
		for(UINT i = 0; i < size; ++i)
		{
			hex += digits[p[i] >> 4];
			hex += digits[p[i] & 15];
			sum = static_cast<BYTE>(sum + p[i]);
		}
		hex += digits[sum >> 4];
		hex += digits[sum & 15];
	}
	std::lock_guard<std::mutex> lock(g_mutex);
	return Store(file, section, key, data ? hex.c_str() : nullptr) ? TRUE : FALSE;
}

BOOL WritePrivateProfileSection(LPCWSTR section, LPCWSTR data, const char *file)
{
	if(!section)
		return FALSE;
	std::lock_guard<std::mutex> lock(g_mutex);
	Document &doc = Load(file);
	Section *s = doc.Find(section);
	if(!data)
	{
		if(s)
			doc.sections.erase(doc.sections.begin() + (s - doc.sections.data()));
		return Save(file, doc) ? TRUE : FALSE;
	}
	if(!s)
	{
		doc.sections.push_back(Section{section, {}});
		s = &doc.sections.back();
	}
	s->entries.clear();
	for(const wchar_t *p = data; *p; p += std::wcslen(p) + 1)
	{
		const std::wstring line = p;
		const auto eq = line.find(L'=');
		if(eq != std::wstring::npos)
			s->entries.push_back(Entry{Trim(line.substr(0, eq)), Trim(line.substr(eq + 1))});
		else
			s->entries.push_back(Entry{{}, line});
	}
	return Save(file, doc) ? TRUE : FALSE;
}

BOOL IsTextUnicode(const void *, int, LPINT)
{
	// Our INI files are UTF-8 and can store any Unicode text, so upstream's
	// "convert ANSI ini file to Unicode" step never needs to run.
	return TRUE;
}

BOOL CopyFile(const char *from, const char *to, BOOL failIfExists)
{
	std::error_code ec;
	const auto opts = failIfExists ? std::filesystem::copy_options::none : std::filesystem::copy_options::overwrite_existing;
	return std::filesystem::copy_file(from, to, opts, ec) && !ec ? TRUE : FALSE;
}
