/*
 * CListCtrl.h
 * -----------
 * Purpose: A class that extends MFC's CListCtrl with some more functionality and to handle unicode strings in ANSI builds.
 * Notes  : mfcless twin of upstream's mptrack/CListCtrl.h.
 *          Both classes are list controls of mfcless::ListCtrl, which
 *          handles Unicode itself, so the ANSI-build helpers are left out.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/ListCtrl.h"

#include "HighDPISupport.h"

OPENMPT_NAMESPACE_BEGIN

class CListCtrlEx : public mfcless::ListCtrl
{
	Q_OBJECT
public:
	using mfcless::ListCtrl::ListCtrl;

	struct Header
	{
		const mpt::uchar *text = nullptr;
		int width = 0;
		uint32 mask = 0;
	};

	void SetHeaders(const mpt::span<const Header> &header)
	{
		for(int i = 0; i < static_cast<int>(header.size()); i++)
		{
			int width = header[i].width;
			InsertColumn(i, header[i].text, header[i].mask, width >= 0 ? HighDPISupport::ScalePixels(width, this) : 16);
			if(width < 0)
				SetColumnWidth(i, width);
		}
	}

	void SetColumnWidths(const mpt::span<const Header> &header)
	{
		for(int i = 0; i < static_cast<int>(header.size()); i++)
		{
			if(int width = header[i].width; width > 0)
				SetColumnWidth(i, HighDPISupport::ScalePixels(width, this));
		}
	}

	void SetItemDataPtr(int item, void *value)
	{
		SetItemData(item, reinterpret_cast<uintptr_t>(value));
	}

	void *GetItemDataPtr(int item)
	{
		return reinterpret_cast<void *>(GetItemData(item));
	}

	// mfcless: group view always works, as the list shows groups itself.
	static bool EnableGroupView(mfcless::ListCtrl &listCtrl)
	{
		return listCtrl.mfcless::ListCtrl::EnableGroupView(true);
	}

	bool EnableGroupView() { return EnableGroupView(*this); }
};


class CMFCListCtrlEx : public mfcless::ListCtrl
{
	Q_OBJECT
public:
	using mfcless::ListCtrl::ListCtrl;

	struct Header
	{
		const mpt::uchar *text = nullptr;
		int width = 0;
		uint32 mask = 0;
	};
	void SetHeaders(const mpt::span<const Header> &header)
	{
		for(int i = 0; i < static_cast<int>(header.size()); i++)
		{
			InsertColumn(i, header[i].text, header[i].mask, HighDPISupport::ScalePixels(header[i].width, this));
		}
	}

	bool EnableGroupView() { return CListCtrlEx::EnableGroupView(*this); }
};


OPENMPT_NAMESPACE_END
