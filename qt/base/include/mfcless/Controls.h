/*
 * Controls.h
 * ----------
 * Purpose: Qt widgets for the Windows common controls that Qt has no direct
 *          counterpart for: the up-down (spin) button that steps a buddy edit
 *          field, and the tab control, whose pages are sibling controls
 *          rather than children.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include <QtWidgets/QWidget>

class QLineEdit;
class QTabBar;
class QToolButton;

namespace mfcless
{

// msctls_updown32. Steps a value in a range (which may run backwards, as
// Windows' default of 100..0 does) and, with SetBuddyInt, writes it into the
// buddy edit field.
class UpDown : public QWidget
{
	Q_OBJECT
public:
	explicit UpDown(bool vertical, QWidget *parent = nullptr);

	void SetBuddy(QLineEdit *buddy) { m_buddy = buddy; }
	QLineEdit *GetBuddy() const { return m_buddy; }
	void SetBuddyInt(bool on) { m_buddyInt = on; }
	void SetWrap(bool on) { m_wrap = on; }

	void SetRange(int lower, int upper);
	int GetLower() const { return m_lower; }
	int GetUpper() const { return m_upper; }
	void SetPos(int pos);
	int GetPos() const;

	QSize sizeHint() const override;

signals:
	// delta is +1 for the up (or right) arrow, -1 for the other one, as
	// Windows' UDN_DELTAPOS reports it. Setting accepted to false keeps the
	// position.
	void deltaPos(int pos, int delta, bool *accepted);
	void posChanged(int pos);

private:
	void Step(int delta);

	QToolButton *m_up = nullptr, *m_down = nullptr;
	QLineEdit *m_buddy = nullptr;
	bool m_vertical = true, m_buddyInt = false, m_wrap = false;
	int m_lower = 100, m_upper = 0, m_pos = 0;
};

// SysTabControl32: a tab bar over a framed area. The area is empty: dialogs
// place the pages' controls on top of it, as Windows dialogs do.
class TabControl : public QWidget
{
	Q_OBJECT
public:
	explicit TabControl(QWidget *parent = nullptr);
	QTabBar *tabBar() const { return m_bar; }
	// The area below the tabs, in this widget's coordinates.
	QRect DisplayRect() const;

protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	QTabBar *m_bar = nullptr;
};

} // namespace mfcless
