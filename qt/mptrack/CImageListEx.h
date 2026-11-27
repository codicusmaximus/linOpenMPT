/*
 * CImageListEx.h
 * --------------
 * Purpose: A class that extends MFC's CImageList to handle alpha-blended images properly. Also provided 1-bit transparency fallback when needed.
 * Notes  : mfcless twin of upstream's mptrack/CImageListEx.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include <QtCore/QObject>
#include "mpt/base/span.hpp"

OPENMPT_NAMESPACE_BEGIN

class CImageListEx : public QObject
{
	Q_OBJECT
public:
	bool Create(uint32 resourceID, int cx, int cy, int nInitial, int nGrow, CDC *dc, double scaling, bool disabled, const mpt::span<const int> invertImages = {});
};

OPENMPT_NAMESPACE_END
