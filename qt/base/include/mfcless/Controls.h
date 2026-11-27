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

#include <QtCore/QElapsedTimer>
#include <QtCore/QPointer>
#include <QtWidgets/QLineEdit>
#include <vector>
#include <QtWidgets/QGroupBox>
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
	explicit UpDown(QWidget *parent = nullptr);

	void SetVertical(bool vertical);
	bool IsVertical() const { return m_vertical; }

	// With UDS_ALIGNRIGHT or UDS_ALIGNLEFT, attaching a buddy moves the
	// up-down onto that edge of the buddy and narrows the buddy, as
	// UDM_SETBUDDY does.
	enum class Align { None, Right, Left };
	void SetAlign(Align align) { m_align = align; }
	void SetBuddy(QLineEdit *buddy);
	// The buddy was made again in the same place (SubclassDlgItem): it keeps
	// the up-down where it is.
	void ReplaceBuddy(QLineEdit *buddy);
	// UDS_ARROWKEYS: the up and down arrow keys in the buddy step it.
	void SetArrowKeys(bool on) { m_arrowKeys = on; }
	QLineEdit *GetBuddy() const { return m_buddy; }
	void SetBuddyInt(bool on) { m_buddyInt = on; }
	void SetWrap(bool on) { m_wrap = on; }

	void SetRange(int lower, int upper);
	int GetLower() const { return m_lower; }
	int GetUpper() const { return m_upper; }
	void GetRange(int &lower, int &upper) const { lower = m_lower; upper = m_upper; }
	void GetRange32(int &lower, int &upper) const { GetRange(lower, upper); }
	// UDACCEL: after holding an arrow for seconds, it steps by increment.
	struct Accel
	{
		uint32_t seconds = 0;
		uint32_t increment = 1;
	};
	void SetAccel(std::vector<Accel> accel) { m_accel = std::move(accel); }
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
	QPointer<QLineEdit> m_buddy;
	bool eventFilter(QObject *watched, QEvent *e) override;

	Align m_align = Align::None;
	bool m_arrowKeys = false;
	bool m_vertical = true, m_buddyInt = false, m_wrap = false;
	int m_lower = 100, m_upper = 0, m_pos = 0;
	// Windows' default acceleration
	std::vector<Accel> m_accel{{0, 1}, {2, 5}, {5, 20}};
	QElapsedTimer m_held;
};

// A Windows group box: an etched frame whose top line runs through the
// caption. The controls it frames are its siblings, on the dialog's
// background, which upstream's layouts are made for.
class GroupFrame : public QGroupBox
{
	Q_OBJECT
public:
	using QGroupBox::QGroupBox;

protected:
	void paintEvent(QPaintEvent *e) override;
};

// SysTabControl32: a tab bar over a framed area. The area is empty: dialogs
// place the pages' controls on top of it, as Windows dialogs do.
class TabControl : public QWidget
{
	Q_OBJECT
public:
	explicit TabControl(QWidget *parent = nullptr);
	QTabBar *tabBar() const { return m_bar; }
	// CTabCtrl's calls for the tabs.
	int InsertItem(int index, const QString &text);
	int InsertItem(int index, const char8_t *text);
	int currentIndex() const;
	void setCurrentIndex(int index);
	int count() const;
	// The area below the tabs, in this widget's coordinates.
	QRect DisplayRect() const;

protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	QTabBar *m_bar = nullptr;
};

} // namespace mfcless
