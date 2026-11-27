// mfcless: the INI file half of upstream's mptrack/Settings.cpp
// (IniFileSettingsBackend's raw read and write functions), on
// mfcless::IniFile instead of the Win32 private profile API. Same functions,
// in the same order as upstream; the rest of Settings.cpp is upstream's.

#include "stdafx.h"
#include "Settings.h"

#include "mfcless/IniFile.h"

OPENMPT_NAMESPACE_BEGIN


namespace
{
std::wstring Section(const SettingPath &path) { return mpt::ToWide(path.GetSection()); }
std::wstring Key(const SettingPath &path) { return mpt::ToWide(path.GetKey()); }
} // namespace


std::vector<std::byte> IniFileSettingsBackend::ReadSettingRaw(const SettingPath &path, const std::vector<std::byte> &def) const
{
	auto result = mfcless::IniFile::ReadBinary(filename.AsNative(), Section(path), Key(path), def.size());
	return result ? *result : def;
}

mpt::ustring IniFileSettingsBackend::ReadSettingRaw(const SettingPath &path, const mpt::ustring &def) const
{
	auto result = mfcless::IniFile::ReadString(filename.AsNative(), Section(path), Key(path));
	return result ? mpt::ToUnicode(*result) : def;
}

double IniFileSettingsBackend::ReadSettingRaw(const SettingPath &path, double def) const
{
	auto result = mfcless::IniFile::ReadString(filename.AsNative(), Section(path), Key(path));
	return result ? mpt::parse<double>(*result) : def;
}

int32 IniFileSettingsBackend::ReadSettingRaw(const SettingPath &path, int32 def) const
{
	return mfcless::IniFile::ReadInt(filename.AsNative(), Section(path), Key(path), def);
}

bool IniFileSettingsBackend::ReadSettingRaw(const SettingPath &path, bool def) const
{
	return mfcless::IniFile::ReadInt(filename.AsNative(), Section(path), Key(path), def ? 1 : 0) ? true : false;
}


void IniFileSettingsBackend::WriteSettingRaw(const SettingPath &path, const std::vector<std::byte> &val)
{
	mfcless::IniFile::WriteBinary(filename.AsNative(), Section(path), Key(path), val);
}

void IniFileSettingsBackend::WriteSettingRaw(const SettingPath &path, const mpt::ustring &val)
{
	// Files are UTF-8, so any string is stored as it is (upstream re-encodes
	// ANSI files to UTF-16 here, which Windows needs).
	mfcless::IniFile::WriteString(filename.AsNative(), Section(path), Key(path), mpt::ToWide(val));
}

void IniFileSettingsBackend::WriteSettingRaw(const SettingPath &path, double val)
{
	mfcless::IniFile::WriteString(filename.AsNative(), Section(path), Key(path), mpt::wfmt::val(val));
}

void IniFileSettingsBackend::WriteSettingRaw(const SettingPath &path, int32 val)
{
	mfcless::IniFile::WriteString(filename.AsNative(), Section(path), Key(path), mpt::wfmt::val(val));
}

void IniFileSettingsBackend::WriteSettingRaw(const SettingPath &path, bool val)
{
	mfcless::IniFile::WriteString(filename.AsNative(), Section(path), Key(path), mpt::wfmt::val(val));
}

void IniFileSettingsBackend::RemoveSettingRaw(const SettingPath &path)
{
	mfcless::IniFile::RemoveKey(filename.AsNative(), Section(path), Key(path));
}

void IniFileSettingsBackend::RemoveSectionRaw(const mpt::ustring &section)
{
	mfcless::IniFile::RemoveSection(filename.AsNative(), mpt::ToWide(section));
}


IniFileSettingsBackend::IniFileSettingsBackend(const mpt::PathString &filename)
	: filename(filename)
{
	return;
}

IniFileSettingsBackend::~IniFileSettingsBackend()
{
	return;
}

void IniFileSettingsBackend::ConvertToUnicode(const mpt::ustring &)
{
	// UTF-8 files hold any text already.
}


OPENMPT_NAMESPACE_END
