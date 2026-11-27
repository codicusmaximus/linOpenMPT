/*
 * DialogBuilder.h
 * ---------------
 * Purpose: Builds Qt widgets from a dialog template of upstream's mptrack.rc,
 *          at the template's positions, so that every dialog keeps the layout
 *          of the Windows version. Also applies the template's DLGINIT combo
 *          box strings and its AFX_DIALOG_LAYOUT resize rules.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "mfcless/Resources.h"

#include <QtCore/QObject>
#include <QtCore/QRect>
#include <QtCore/QSize>
#include <QtGui/QFont>

#include <functional>
#include <map>
#include <vector>

class QWidget;
class QButtonGroup;

namespace mfcless
{

// Control style bits from the templates that callers may want to look at.
namespace Style
{
	inline constexpr uint32_t Child = 0x40000000, Visible = 0x10000000, Disabled = 0x08000000,
		Caption = 0x00C00000, Border = 0x00800000, VScroll = 0x00200000, HScroll = 0x00100000,
		ThickFrame = 0x00040000, Group = 0x00020000, TabStop = 0x00010000;
}

struct BuiltDialog
{
	res::DialogTemplate dlgTemplate;
	res::DialogUnits units;
	QSize size;  // client size in pixels
	// Controls by resource ID. Controls with ID 0 or -1 (static labels) are
	// in `all` only.
	std::map<uint32_t, QWidget *> items;
	std::vector<QWidget *> all;  // in template order
	std::vector<QButtonGroup *> radioGroups;
};

// The dialog units for the dialog's font, by Windows' rule.
res::DialogUnits UnitsFor(const QWidget *dialog);
// The font for a template: the application's, at the template's size.
QFont DialogFont(const res::DialogTemplate &tmpl);

// Makes the dialog's own control for a template item (one of a class of
// its own, as upstream's DDX_Control binds), or returns nullptr for the
// template's usual control.
using ControlFactory = std::function<QWidget *(const res::DialogItem &item, QWidget *parent)>;

// Creates the template's controls as children of dialog and sizes it. False
// if the template is missing.
bool BuildDialog(QWidget *dialog, uint32_t templateId, BuiltDialog &out, const ControlFactory &factory = {});

// Creates one control of a template, or sets up given as that control.
QWidget *CreateControl(const res::DialogItem &item, QWidget *parent, QWidget *given = nullptr);

// AFX_DIALOG_LAYOUT: moves and resizes the controls as the dialog grows.
class DialogLayout : public QObject
{
	Q_OBJECT
public:
	DialogLayout(QWidget *dialog, const BuiltDialog &built, std::vector<res::DialogLayoutItem> rules);
	bool eventFilter(QObject *watched, QEvent *e) override;
	// Call after moving a control by hand, so that later resizes start from
	// its new place.
	void Rebase(QWidget *control);
	// A control made again in the place of another (SubclassDlgItem).
	void Replace(QWidget *old, QWidget *now);
	// Stops or resumes following the dialog's size (a minimized plugin editor).
	void SetEnabled(bool enable);
	void Apply();
	// Moves the whole layout down by dy and makes the dialog's base size
	// that much taller (room for a menu bar above the template's controls).
	void ShiftContent(int dy);

private:
	bool m_enabled = true;
	struct Entry
	{
		QWidget *widget;
		QRect rect;
		res::DialogLayoutItem rule;
	};
	QWidget *m_dialog;
	QSize m_baseSize;
	std::vector<Entry> m_entries;
};

} // namespace mfcless
