// mfcless base: INI files (see mfcless/IniFile.h).
// The parser and writer are the ones mfc-qt uses (src/win32/win32_profile.cpp),
// so both builds read and write the same files the same way.

#include "mfcless/IniFile.h"

#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

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

std::optional<std::wstring> Lookup(const std::string &file, const wchar_t *section, const wchar_t *key)
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

bool Store(const std::string &file, const wchar_t *section, const wchar_t *key, const wchar_t *value)
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

namespace mfcless::IniFile
{

std::optional<std::wstring> ReadString(const std::string &file, const std::wstring &section, const std::wstring &key)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	auto v = Lookup(file, section.c_str(), key.c_str());
	if(!v)
		return std::nullopt;
	std::wstring value = *v;
	if(value.size() >= 2 && ((value.front() == L'"' && value.back() == L'"') || (value.front() == L'\'' && value.back() == L'\'')))
		value = value.substr(1, value.size() - 2);
	return value;
}

int ReadInt(const std::string &file, const std::wstring &section, const std::wstring &key, int def)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	auto v = Lookup(file, section.c_str(), key.c_str());
	if(!v)
		return def;
	const std::wstring t = Trim(*v);
	std::size_t i = 0;
	bool neg = false;
	if(!t.empty() && (t[0] == L'-' || t[0] == L'+'))
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
	// As Windows: the result is a UINT, which callers cast back to int.
	return static_cast<int>(static_cast<unsigned int>(neg ? -result : result));
}

std::optional<std::vector<std::byte>> ReadBinary(const std::string &file, const std::wstring &section, const std::wstring &key, std::size_t size)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	auto v = Lookup(file, section.c_str(), key.c_str());
	if(!v)
		return std::nullopt;
	const std::wstring t = Trim(*v);
	if(t.size() != 2u * (size + 1))
		return std::nullopt;
	auto hex = [](wchar_t c) -> int {
		if(c >= L'0' && c <= L'9') return c - L'0';
		if(c >= L'A' && c <= L'F') return c - L'A' + 10;
		if(c >= L'a' && c <= L'f') return c - L'a' + 10;
		return -1;
	};
	std::vector<std::byte> bytes(size + 1);
	unsigned char sum = 0;
	for(std::size_t i = 0; i <= size; ++i)
	{
		const int hi = hex(t[2 * i]), lo = hex(t[2 * i + 1]);
		if(hi < 0 || lo < 0)
			return std::nullopt;
		bytes[i] = static_cast<std::byte>(hi * 16 + lo);
		if(i < size)
			sum = static_cast<unsigned char>(sum + hi * 16 + lo);
	}
	if(static_cast<std::byte>(sum) != bytes[size])
		return std::nullopt;
	bytes.resize(size);
	return bytes;
}

bool WriteString(const std::string &file, const std::wstring &section, const std::wstring &key, const std::wstring &value)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return Store(file, section.c_str(), key.c_str(), value.c_str());
}

bool WriteBinary(const std::string &file, const std::wstring &section, const std::wstring &key, const std::vector<std::byte> &value)
{
	static const wchar_t digits[] = L"0123456789ABCDEF";
	std::wstring hex;
	unsigned char sum = 0;
	for(std::byte b : value)
	{
		const auto c = static_cast<unsigned char>(b);
		hex += digits[c >> 4];
		hex += digits[c & 15];
		sum = static_cast<unsigned char>(sum + c);
	}
	hex += digits[sum >> 4];
	hex += digits[sum & 15];
	std::lock_guard<std::mutex> lock(g_mutex);
	return Store(file, section.c_str(), key.c_str(), hex.c_str());
}

bool RemoveKey(const std::string &file, const std::wstring &section, const std::wstring &key)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return Store(file, section.c_str(), key.c_str(), nullptr);
}

bool RemoveSection(const std::string &file, const std::wstring &section)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return Store(file, section.c_str(), nullptr, nullptr);
}

} // namespace mfcless::IniFile
