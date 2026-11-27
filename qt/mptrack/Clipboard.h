/*
* Clipboard.h
* -----------
* Purpose: RAII wrapper around operating system clipboard
* Notes  : mfcless twin of upstream's mptrack/Clipboard.h. The data goes
*          through a buffer of our own to Qt's clipboard (mfcless::clip).
* Authors: OpenMPT Devs
* The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
*/

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Clipboard.h"

#include <vector>

class Clipboard
{
public:
	// Open clipboard for writing (size > 0) or reading (size == 0).
	Clipboard(uint32 clipFormat, size_t size = 0)
		: m_clipFormat(clipFormat)
	{
		m_opened = true;
		if(size > 0)
		{
			m_buffer.assign(size, std::byte{0});
			m_writing = true;
		} else
		{
			const QByteArray data = mfcless::clip::GetData(m_clipFormat);
			m_buffer.assign(reinterpret_cast<const std::byte *>(data.constData()), reinterpret_cast<const std::byte *>(data.constData()) + data.size());
		}
		if(!m_buffer.empty())
			m_data = mpt::as_span(m_buffer);
	}

	bool IsValid() const
	{
		return m_opened && m_data.data();
	}

	template<typename T>
	T *As()
	{
		return mpt::byte_cast<T*>(m_data.data());
	}

	mpt::byte_span Get()
	{
		return m_data;
	}

	std::string_view GetString() const
	{
		if(m_data.data())
			return {mpt::byte_cast<const char *>(m_data.data()), m_data.size()};
		else
			return {};
	}

	std::wstring_view GetWideString() const
	{
		if(m_data.data() && m_clipFormat == mfcless::clip::UnicodeText)
			return {static_cast<const wchar_t *>(static_cast<const void *>(m_data.data())), m_data.size() / sizeof(wchar_t)};
		else
			return {};
	}

	Clipboard &operator =(mpt::const_byte_span data)
	{
		MPT_ASSERT(m_data.size() >= data.size());
		std::copy(data.begin(), data.end(), m_data.begin());
		return *this;
	}

	template <typename T>
	Clipboard &operator =(const T &v)
	{
		mpt::const_byte_span data = mpt::as_raw_memory(v);
		MPT_ASSERT(m_data.size() >= data.size());
		std::copy(data.begin(), data.end(), m_data.begin());
		return *this;
	}

	void Close()
	{
		if(m_writing)
		{
			mfcless::clip::SetData(m_clipFormat, QByteArray(reinterpret_cast<const char *>(m_buffer.data()), static_cast<qsizetype>(m_buffer.size())));
			m_writing = false;
		}
		m_opened = false;
	}

	~Clipboard()
	{
		Close();
	}

protected:
	std::vector<std::byte> m_buffer;
	mpt::byte_span m_data;
	bool m_writing = false;
	uint32 m_clipFormat;
	bool m_opened = false;
};
