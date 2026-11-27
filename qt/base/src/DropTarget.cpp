/*
 * DropTarget.cpp
 * --------------
 * Purpose: A window's drag and drop handler as upstream writes one.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/DropTarget.h"
#include "mfcless/Keyboard.h"

#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QWidget>

namespace mfcless
{

namespace
{

Qt::DropAction ToQt(uint32_t effect)
{
	if(effect & drop::Move)
		return Qt::MoveAction;
	if(effect & drop::Copy)
		return Qt::CopyAction;
	if(effect & drop::Link)
		return Qt::LinkAction;
	return Qt::IgnoreAction;
}

uint32_t FromQt(Qt::DropAction action)
{
	switch(action)
	{
	case Qt::MoveAction: return drop::Move;
	case Qt::CopyAction: return drop::Copy;
	case Qt::LinkAction: return drop::Link;
	default: return drop::None;
	}
}

uint32_t KeyState(const QDropEvent *e)
{
	uint32_t keys = 0;
	if(e->buttons() & Qt::LeftButton)
		keys |= MouseKey::Left;
	if(e->buttons() & Qt::RightButton)
		keys |= MouseKey::Right;
	if(e->buttons() & Qt::MiddleButton)
		keys |= MouseKey::Middle;
	if(e->modifiers() & Qt::ShiftModifier)
		keys |= MouseKey::Shift;
	if(e->modifiers() & Qt::ControlModifier)
		keys |= MouseKey::Control;
	return keys;
}

} // namespace


bool DropTarget::Register(QWidget *widget)
{
	if(!widget)
		return false;
	m_widget = widget;
	QWidget *target = widget;
	if(auto *area = qobject_cast<QAbstractScrollArea *>(widget))
		target = area->viewport();
	target->setAcceptDrops(true);
	target->installEventFilter(this);
	if(target != widget)
		widget->setAcceptDrops(true);
	return true;
}


bool DropTarget::eventFilter(QObject *, QEvent *e)
{
	switch(e->type())
	{
	case QEvent::DragEnter:
	case QEvent::DragMove:
	{
		auto *de = static_cast<QDragMoveEvent *>(e);
		const Point pt = de->position().toPoint();
		const uint32_t effect = (e->type() == QEvent::DragEnter)
			? OnDragEnter(m_widget, de->mimeData(), KeyState(de), pt)
			: OnDragOver(m_widget, de->mimeData(), KeyState(de), pt);
		if(effect & (drop::Copy | drop::Move | drop::Link))
		{
			de->setDropAction(ToQt(effect));
			de->accept();
		} else
		{
			de->ignore();
		}
		return true;
	}
	case QEvent::DragLeave:
		OnDragLeave(m_widget);
		return true;
	case QEvent::Drop:
	{
		auto *de = static_cast<QDropEvent *>(e);
		if(OnDrop(m_widget, de->mimeData(), FromQt(de->dropAction()), de->position().toPoint()))
			de->acceptProposedAction();
		else
			de->ignore();
		return true;
	}
	default:
		return false;
	}
}


uint32_t DropTarget::OnDragEnter(QWidget *, const QMimeData *, uint32_t, Point)
{
	return drop::None;
}

uint32_t DropTarget::OnDragOver(QWidget *, const QMimeData *, uint32_t, Point)
{
	return drop::None;
}

void DropTarget::OnDragLeave(QWidget *)
{
}

bool DropTarget::OnDrop(QWidget *, const QMimeData *, uint32_t, Point)
{
	return false;
}

} // namespace mfcless
