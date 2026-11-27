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

// The dialog units for the font that dialogs use. Windows derives them from
// the template's font; here the application's font is used, so that text fits
// the controls on every desktop.
res::DialogUnits UnitsFor(const QWidget *dialog);

// Creates the template's controls as children of dialog and sizes it. False
// if the template is missing.
bool BuildDialog(QWidget *dialog, uint32_t templateId, BuiltDialog &out);

// Creates one control of a template.
QWidget *CreateControl(const res::DialogItem &item, QWidget *parent);

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

private:
	void Apply();
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
