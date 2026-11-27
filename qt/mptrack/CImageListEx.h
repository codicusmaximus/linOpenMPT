/*
 * CImageListEx.h
 * --------------
 * Purpose: A class that extends MFC's CImageList to handle alpha-blended images properly.
 * Notes  : mfcless twin of upstream's mptrack/CImageListEx.h. A list of
 *          same-sized images cut from one PNG strip of the resources.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mpt/base/span.hpp"

#include <QtGui/QIcon>
#include <QtGui/QPixmap>

#include <vector>

#include "mfcless/Graphics.h"

OPENMPT_NAMESPACE_BEGIN

class CImageListEx
{
public:
	bool Create(uint32 resourceID, int cx, int cy, int nInitial, int nGrow, double scaling, bool disabled, const mpt::span<const int> invertImages = {});

	// mfcless: the images.
	int GetImageCount() const { return static_cast<int>(m_images.size()); }
	QPixmap GetImage(int index) const { return (index >= 0 && index < GetImageCount()) ? m_images[index] : QPixmap(); }
	QIcon GetIcon(int index) const { return QIcon(GetImage(index)); }
	QSize GetImageSize() const { return m_size; }
	// ImageList_Draw (ILD_NORMAL)
	void Draw(mfcless::DC *dc, int index, mfcless::Point pt) const
	{
		if(dc && dc->GetPainter() && index >= 0 && index < GetImageCount())
			dc->GetPainter()->drawPixmap(QPoint(pt.x, pt.y), m_images[index]);
	}
	std::vector<QIcon> GetIcons() const
	{
		std::vector<QIcon> icons;
		for(const auto &image : m_images)
			icons.emplace_back(image);
		return icons;
	}

private:
	std::vector<QPixmap> m_images;
	QSize m_size;
};

OPENMPT_NAMESPACE_END
