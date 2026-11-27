/*
 * StatusBar.cpp
 * -------------
 * Purpose: The main window's status bar as upstream fills it.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/StatusBar.h"
#include "mfcless/Resources.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>

namespace mfcless
{

StatusBar::StatusBar(QWidget *parent)
	: Window(parent)
{
}


bool StatusBar::Create(QMainWindow *parent)
{
	parent->setStatusBar(this);
	return true;
}


bool StatusBar::SetIndicators(const uint32_t *ids, int count)
{
	for(PaneInfo &p : m_panes)
	{
		removeWidget(p.label);
		delete p.label;
	}
	m_panes.clear();
	for(int i = 0; i < count; i++)
	{
		PaneInfo p;
		p.id = ids[i];
		p.label = new QLabel(this);
		p.label->setText(ids[i] ? QString::fromStdU16String(res::String(ids[i])) : QString());
		if(i == 0)
		{
			addWidget(p.label, 1);
		} else
		{
			// CStatusBar panes draw their text from the left (a leading tab
			// would centre it, which OpenMPT does not use).
			p.label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
			p.label->setIndent(3);
			p.label->setFrameStyle(QFrame::Panel | QFrame::Sunken);
			addPermanentWidget(p.label);
		}
		m_panes.push_back(p);
	}
	return true;
}


int StatusBar::CommandToIndex(uint32_t id) const
{
	for(std::size_t i = 0; i < m_panes.size(); i++)
	{
		if(m_panes[i].id == id)
			return static_cast<int>(i);
	}
	return -1;
}


uint32_t StatusBar::GetItemID(int index) const
{
	return (index >= 0 && index < static_cast<int>(m_panes.size())) ? m_panes[index].id : 0;
}


void StatusBar::SetPaneInfo(int index, uint32_t id, uint32_t style, int width)
{
	if(index < 0 || index >= static_cast<int>(m_panes.size()))
		return;
	PaneInfo &p = m_panes[index];
	p.id = id;
	if(style & pane::Stretch)
	{
		p.label->setMinimumWidth(width);
		p.label->setMaximumWidth(QWIDGETSIZE_MAX);
	} else
	{
		// Room for the text and the frame.
		p.label->setFixedWidth(width + 2 * p.label->frameWidth() + 8);
	}
	p.label->setEnabled(!(style & pane::Disabled));
}


bool StatusBar::SetPaneText(int index, const Text &text, bool)
{
	if(index < 0 || index >= static_cast<int>(m_panes.size()))
		return false;
	m_panes[index].label->setText(text.value);
	return true;
}


void StatusBar::UpdatePanes(MessageMap *target)
{
	if(target == nullptr || !isVisible())
		return;
	for(const PaneInfo &p : m_panes)
	{
		if(p.id == 0)
			continue;
		CommandState state;
		state.id = p.id;
		target->DispatchCommandUpdate(p.id, state);
	}
}


QString StatusBar::GetPaneText(int index) const
{
	if(index < 0 || index >= static_cast<int>(m_panes.size()))
		return {};
	return m_panes[index].label->text();
}

} // namespace mfcless
