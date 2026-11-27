// mfcless base: INI files, in the format the MFC build (mfc-qt) and OpenMPT
// on Windows use, so all of them can share one mptrack.ini.
//
// Windows' private profile semantics: case-insensitive section and key names,
// original order kept, binary data stored as hex plus a checksum byte.
// Written as UTF-8; UTF-16LE (as OpenMPT writes it on Windows) and UTF-8 with
// a BOM are read too.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace mfcless::IniFile
{

// The value, or nothing if the key or section is missing. A single pair of
// surrounding quotes is removed, as Windows does.
std::optional<std::wstring> ReadString(const std::string &file, const std::wstring &section, const std::wstring &key);
// As Windows' GetPrivateProfileInt: decimal or 0x hex, with a sign; text after
// the number is ignored.
int ReadInt(const std::string &file, const std::wstring &section, const std::wstring &key, int def);
// Binary data of exactly the given size, or nothing if missing or corrupt.
std::optional<std::vector<std::byte>> ReadBinary(const std::string &file, const std::wstring &section, const std::wstring &key, std::size_t size);

bool WriteString(const std::string &file, const std::wstring &section, const std::wstring &key, const std::wstring &value);
bool WriteBinary(const std::string &file, const std::wstring &section, const std::wstring &key, const std::vector<std::byte> &value);
bool RemoveKey(const std::string &file, const std::wstring &section, const std::wstring &key);
bool RemoveSection(const std::string &file, const std::wstring &section);

} // namespace mfcless::IniFile
