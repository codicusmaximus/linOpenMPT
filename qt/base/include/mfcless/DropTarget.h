/*
 * DropTarget.h
 * ------------
 * Purpose: A window's drag and drop handler as upstream writes one: told
 *          when something is dragged into, over and out of the window, and
 *          when it is dropped, with the effect to show (copy, move, ...).
 * Notes  : Register installs it on a widget (and accepts drops there). The
 *          dragged data is Qt's QMimeData.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "mfcless/Geometry.h"

#include <QtCore/QObject>

#include <cstdint>

class QMimeData;
class QWidget;

namespace mfcless
{

namespace drop
{
	inline constexpr uint32_t None = 0, Copy = 1, Move = 2, Link = 4, Scroll = 0x80000000u;
}

class DropTarget : public QObject
{
	Q_OBJECT
public:
	bool Register(QWidget *widget);

	// keyState: mfcless::MouseKey flags. Return the drop::... effect.
	virtual uint32_t OnDragEnter(QWidget *wnd, const QMimeData *data, uint32_t keyState, Point point);
	virtual uint32_t OnDragOver(QWidget *wnd, const QMimeData *data, uint32_t keyState, Point point);
	virtual void OnDragLeave(QWidget *wnd);
	virtual bool OnDrop(QWidget *wnd, const QMimeData *data, uint32_t dropEffect, Point point);

protected:
	bool eventFilter(QObject *watched, QEvent *e) override;

	QWidget *m_widget = nullptr;
};

} // namespace mfcless
