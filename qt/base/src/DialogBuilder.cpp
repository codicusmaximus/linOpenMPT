/*
 * DialogBuilder.cpp
 * -----------------
 * Purpose: Builds Qt widgets from upstream's dialog templates.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/DialogBuilder.h"
#include "mfcless/Controls.h"
#include "mfcless/Button.h"
#include "mfcless/ListCtrl.h"

#include <QtCore/QEvent>
#include <QtGui/QFontInfo>
#include <QtGui/QFontMetrics>

#include <cmath>
#include <type_traits>
#include <QtGui/QRegularExpressionValidator>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QSlider>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTreeWidget>

namespace mfcless
{

namespace
{

QString Q(const std::u16string &s) { return QString::fromStdU16String(s); }

bool IsClass(const std::u16string &cls, const char *name)
{
	return Q(cls).compare(QLatin1String(name), Qt::CaseInsensitive) == 0;
}

// The control types of the templates, from their class and style.
enum class Kind
{
	PushButton, CheckBox, RadioButton, GroupBox,
	Label, Icon, Frame, HLine, VLine,
	Edit, MultiEdit, ComboBox, ListBox, ScrollBar,
	Slider, UpDown, Progress, ListView, TreeView, Tab, ToolBar, Link,
	Unknown,
};

Kind KindOf(const res::DialogItem &item)
{
	const auto &cls = item.className;
	const uint32_t s = item.style;
	if(IsClass(cls, "BUTTON"))
	{
		switch(s & 0x0F)
		{
		case 2: case 3: case 5: case 6: return Kind::CheckBox;
		case 4: case 9: return Kind::RadioButton;
		case 7: return Kind::GroupBox;
		default: return Kind::PushButton;
		}
	}
	if(IsClass(cls, "STATIC"))
	{
		switch(s & 0x1F)
		{
		case 0x03: case 0x0E: return Kind::Icon;
		case 0x10: return Kind::HLine;
		case 0x11: return Kind::VLine;
		case 0x04: case 0x05: case 0x06: case 0x07: case 0x08: case 0x09: case 0x12: return Kind::Frame;
		default: return Kind::Label;
		}
	}
	if(IsClass(cls, "EDIT"))
		return (s & 0x4) ? Kind::MultiEdit : Kind::Edit;
	if(IsClass(cls, "COMBOBOX") || IsClass(cls, "ComboBoxEx32"))
		return Kind::ComboBox;
	if(IsClass(cls, "LISTBOX"))
		return Kind::ListBox;
	if(IsClass(cls, "SCROLLBAR"))
		return Kind::ScrollBar;
	if(IsClass(cls, "msctls_trackbar32"))
		return Kind::Slider;
	if(IsClass(cls, "msctls_updown32"))
		return Kind::UpDown;
	if(IsClass(cls, "msctls_progress32"))
		return Kind::Progress;
	if(IsClass(cls, "SysListView32"))
		return Kind::ListView;
	if(IsClass(cls, "SysTreeView32"))
		return Kind::TreeView;
	if(IsClass(cls, "SysTabControl32"))
		return Kind::Tab;
	if(IsClass(cls, "ToolbarWindow32"))
		return Kind::ToolBar;
	if(IsClass(cls, "SysLink"))
		return Kind::Link;
	if(IsClass(cls, "RichEdit20W") || IsClass(cls, "RICHEDIT50W") || IsClass(cls, "RichEdit20A"))
		return Kind::MultiEdit;
	return Kind::Unknown;
}

Qt::Alignment TextAlignment(uint32_t style, bool isStatic)
{
	Qt::Alignment a;
	if(isStatic)
	{
		switch(style & 0x1F)
		{
		case 1: a = Qt::AlignHCenter; break;
		case 2: a = Qt::AlignRight; break;
		default: a = Qt::AlignLeft; break;
		}
		a |= (style & 0x200) ? Qt::AlignVCenter : Qt::AlignTop;  // SS_CENTERIMAGE
	} else
	{
		switch(style & 0x3)  // ES_CENTER 1, ES_RIGHT 2
		{
		case 1: a = Qt::AlignHCenter; break;
		case 2: a = Qt::AlignRight; break;
		default: a = Qt::AlignLeft; break;
		}
		a |= Qt::AlignVCenter;
	}
	return a;
}

// The control of type T for a template item: a new one, or the one the
// dialog made itself if it is a T. Null if the dialog's control is another
// kind of widget, which is then left as it is.
template <typename T, typename New = std::nullptr_t>
T *Make(QWidget *given, QWidget *parent, New make = nullptr)
{
	if(given)
		return qobject_cast<T *>(given);
	if constexpr(std::is_same_v<New, std::nullptr_t>)
		return new T(parent);
	else
		return make(parent);
}

void FrameFor(QFrame *f, const res::DialogItem &item)
{
	if(item.exStyle & 0x200)  // WS_EX_CLIENTEDGE
		f->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	else if(item.exStyle & 0x20000)  // WS_EX_STATICEDGE
		f->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	else if(item.style & Style::Border)
		f->setFrameStyle(QFrame::Box | QFrame::Plain);
	else
		f->setFrameStyle(QFrame::NoFrame);
}

} // namespace


QFont DialogFont(const res::DialogTemplate &tmpl)
{
	// The application font's family, at the template's point size relative
	// to the 12 pixels of Windows' 9 pt default, as the MFC build does.
	QFont font = QApplication::font();
	const int ui = QFontInfo(font).pixelSize();
	const double windowsPixels = tmpl.pointSize * 96.0 / 72.0;
	font.setPixelSize(std::max(1, static_cast<int>(std::lround(windowsPixels * ui / 12.0))));
	if(tmpl.weight >= 600)
		font.setBold(true);
	font.setItalic(tmpl.italic);
	return font;
}


res::DialogUnits UnitsFor(const QWidget *dialog)
{
	const QFontMetrics fm(dialog ? dialog->font() : QApplication::font());
	// Windows' GdiGetCharDimensions: the average width of the 52 letters.
	const int width = fm.horizontalAdvance(QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"));
	res::DialogUnits u;
	u.baseX = std::max(1, (width + 26) / 52);
	u.baseY = std::max(1, fm.height());
	return u;
}


QWidget *CreateControl(const res::DialogItem &item, QWidget *parent, QWidget *given)
{
	const QString text = Q(item.text);
	const uint32_t s = item.style;
	// A control the dialog made itself is set up as the template's control
	// would be if it is of the same Qt class, and taken as it is otherwise.
	QWidget *w = given;
	switch(KindOf(item))
	{
	case Kind::PushButton:
	{
		auto *b = Make<QPushButton>(given, parent, [](QWidget *p) { return new Button(p); });
		if(!b) break;
		b->setText(text);
		b->setDefault((s & 0x0F) == 1);  // BS_DEFPUSHBUTTON
		b->setAutoDefault(false);
		w = b;
		break;
	}
	case Kind::CheckBox:
		if(s & 0x1000)  // BS_PUSHLIKE
		{
			auto *b = Make<QPushButton>(given, parent);
			if(!b) break;
			b->setText(text);
			b->setCheckable(true);
			b->setAutoDefault(false);
			w = b;
		} else
		{
			auto *b = Make<QCheckBox>(given, parent);
			if(!b) break;
			b->setText(text);
			b->setTristate((s & 0x0F) == 5 || (s & 0x0F) == 6);
			w = b;
		}
		break;
	case Kind::RadioButton:
		if(s & 0x1000)
		{
			auto *b = Make<QPushButton>(given, parent);
			if(!b) break;
			b->setText(text);
			b->setCheckable(true);
			b->setAutoDefault(false);
			w = b;
		} else
		{
			auto *b = Make<QRadioButton>(given, parent);
			if(!b) break;
			b->setText(text);
			b->setAutoExclusive(false);  // grouped by WS_GROUP in BuildDialog
			w = b;
		}
		break;
	case Kind::GroupBox:
	{
		auto *g = Make<QGroupBox>(given, parent, [](QWidget *p) { return new GroupFrame(QString(), p); });
		if(!g) break;
		g->setTitle(text);
		if((s & 0x300) == 0x300)  // BS_CENTER
			g->setAlignment(Qt::AlignHCenter);
		else if(s & 0x200)  // BS_RIGHT
			g->setAlignment(Qt::AlignRight);
		w = g;
	}
		break;
	case Kind::Label:
	{
		auto *l = Make<QLabel>(given, parent);
		if(!l) break;
		l->setTextFormat(Qt::PlainText);
		l->setAlignment(TextAlignment(s, true));
		// SS_LEFTNOWORDWRAP 0xB, SS_SIMPLE 0xC, SS_ENDELLIPSIS etc. do not wrap.
		const uint32_t type = s & 0x1F;
		l->setWordWrap(type != 0x0B && type != 0x0C && !(s & 0xC000));
		QString t = text;
		if(s & 0x80)  // SS_NOPREFIX: '&' is literal
			t.replace(QLatin1Char('&'), QLatin1String("&&"));
		l->setText(t);
		FrameFor(l, item);
		if(s & 0x1000)  // SS_SUNKEN
			l->setFrameStyle(QFrame::Panel | QFrame::Sunken);
		w = l;
		break;
	}
	case Kind::Icon:
	{
		auto *l = Make<QLabel>(given, parent);
		if(!l) break;
		l->setAlignment(Qt::AlignLeft | Qt::AlignTop);
		if(item.textResource)
		{
			const QImage img = ((s & 0x1F) == 0x03) ? res::Icon(item.textResource, 32) : res::Bitmap(item.textResource);
			l->setPixmap(QPixmap::fromImage(img));
		}
		w = l;
		break;
	}
	case Kind::Frame:
	{
		auto *f = Make<QFrame>(given, parent);
		if(!f) break;
		f->setFrameStyle(QFrame::Box | QFrame::Sunken);
		w = f;
		break;
	}
	case Kind::HLine:
	case Kind::VLine:
	{
		auto *f = Make<QFrame>(given, parent);
		if(!f) break;
		f->setFrameShape(KindOf(item) == Kind::HLine ? QFrame::HLine : QFrame::VLine);
		f->setFrameShadow(QFrame::Sunken);
		w = f;
		break;
	}
	case Kind::Edit:
	{
		auto *e = Make<QLineEdit>(given, parent);
		if(!e) break;
		e->setText(text);
		e->setAlignment(TextAlignment(s, false));
		e->setReadOnly(s & 0x800);
		if(s & 0x20)
			e->setEchoMode(QLineEdit::Password);
		e->setProperty("mfclessNumber", (s & 0x2000) != 0);
		if(s & 0x2000)  // ES_NUMBER
			e->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9]*")), e));
		if(!(item.exStyle & 0x200) && !(s & Style::Border))
			e->setFrame(false);
		w = e;
		break;
	}
	case Kind::MultiEdit:
	{
		auto *e = Make<QPlainTextEdit>(given, parent);
		if(!e) break;
		e->setPlainText(text);
		e->setReadOnly(s & 0x800);
		e->setVerticalScrollBarPolicy((s & Style::VScroll) ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
		e->setHorizontalScrollBarPolicy((s & Style::HScroll) ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
		e->setLineWrapMode((s & Style::HScroll) || (s & 0x80) ? QPlainTextEdit::NoWrap : QPlainTextEdit::WidgetWidth);
		FrameFor(e, item);
		w = e;
		break;
	}
	case Kind::ComboBox:
	{
		auto *c = Make<QComboBox>(given, parent);
		if(!c) break;
		const uint32_t type = s & 0x3;  // CBS_SIMPLE 1, CBS_DROPDOWN 2, CBS_DROPDOWNLIST 3
		c->setEditable(type != 3);
		c->setInsertPolicy(QComboBox::NoInsert);
		c->setProperty("mfclessSorted", (s & 0x100) != 0);  // CBS_SORT
		c->setMaxVisibleItems(20);
		w = c;
		break;
	}
	case Kind::ListBox:
	{
		auto *l = Make<QListWidget>(given, parent);
		if(!l) break;
		if(s & 0x8)  // LBS_MULTIPLESEL
			l->setSelectionMode(QAbstractItemView::MultiSelection);
		else if(s & 0x800)  // LBS_EXTENDEDSEL
			l->setSelectionMode(QAbstractItemView::ExtendedSelection);
		else if(s & 0x4000)  // LBS_NOSEL
			l->setSelectionMode(QAbstractItemView::NoSelection);
		l->setSortingEnabled(s & 0x2);  // LBS_SORT
		w = l;
		break;
	}
	case Kind::ScrollBar:
		if(auto *sb = Make<QScrollBar>(given, parent))
		{
			sb->setOrientation((s & 0x1) ? Qt::Vertical : Qt::Horizontal);
			w = sb;
		}
		break;
	case Kind::Slider:
	{
		auto *sl = Make<QSlider>(given, parent);
		if(!sl) break;
		sl->setOrientation((s & 0x2) ? Qt::Vertical : Qt::Horizontal);
		sl->setRange(0, 100);
		if(s & 0x10)  // TBS_NOTICKS
			sl->setTickPosition(QSlider::NoTicks);
		else if(s & 0x8)  // TBS_BOTH
			sl->setTickPosition(QSlider::TicksBothSides);
		else if(s & 0x4)  // TBS_TOP / TBS_LEFT
			sl->setTickPosition(QSlider::TicksAbove);
		else
			sl->setTickPosition(QSlider::TicksBelow);
		w = sl;
		break;
	}
	case Kind::UpDown:
	{
		auto *u = Make<UpDown>(given, parent);
		if(!u) break;
		u->SetVertical(!(s & 0x40));  // UDS_HORZ 0x40
		u->SetBuddyInt(s & 0x2);
		u->SetWrap(s & 0x1);
		w = u;
		break;
	}
	case Kind::Progress:
	{
		auto *p = Make<QProgressBar>(given, parent);
		if(!p) break;
		p->setOrientation((s & 0x4) ? Qt::Vertical : Qt::Horizontal);
		p->setTextVisible(false);
		w = p;
		break;
	}
	case Kind::ListView:
		if((s & 0x3) == 1)  // LVS_REPORT
		{
			auto *t = Make<QTreeWidget>(given, parent, [](QWidget *p) { return new ListCtrl(p); });
			if(!t) break;
			t->setHeaderHidden(s & 0x4000);  // LVS_NOCOLUMNHEADER
			t->setSelectionMode((s & 0x4) ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection);
			w = t;
		} else
		{
			auto *l = Make<QListWidget>(given, parent);
			if(!l) break;
			l->setViewMode((s & 0x3) == 0 ? QListView::IconMode : QListView::ListMode);
			l->setSelectionMode((s & 0x4) ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection);
			w = l;
		}
		break;
	case Kind::TreeView:
	{
		auto *t = Make<QTreeWidget>(given, parent);
		if(!t) break;
		t->setHeaderHidden(true);
		t->setRootIsDecorated(s & 0x4);  // TVS_LINESATROOT
		w = t;
		break;
	}
	case Kind::Tab:
		if(auto *t = Make<TabControl>(given, parent))
			w = t;
		break;
	case Kind::ToolBar:
	{
		auto *t = Make<QToolBar>(given, parent);
		if(!t) break;
		t->setIconSize(QSize(16, 16));
		w = t;
		break;
	}
	case Kind::Link:
	{
		auto *l = Make<QLabel>(given, parent);
		if(!l) break;
		l->setText(text);
		l->setTextFormat(Qt::RichText);
		l->setOpenExternalLinks(false);
		l->setTextInteractionFlags(Qt::TextBrowserInteraction);
		w = l;
		break;
	}
	case Kind::Unknown:
		// Custom window classes: a placeholder the dialog replaces with
		// its own widget.
		w = given ? given : new QWidget(parent);
		break;
	}
	w->setProperty("mfcless_built", true);
	w->setProperty("mfcless_id", item.id);
	w->setProperty("mfcless_class", Q(item.className));
	w->setProperty("mfcless_style", item.style);
	w->setObjectName(QString::number(item.id));
	w->setEnabled(!(item.style & Style::Disabled));
	if(!(item.style & Style::TabStop) && KindOf(item) != Kind::Label)
		w->setFocusPolicy(w->focusPolicy() == Qt::NoFocus ? Qt::NoFocus : Qt::ClickFocus);
	if(item.helpId)
		w->setProperty("mfcless_help", item.helpId);
	return w;
}


bool BuildDialog(QWidget *dialog, uint32_t templateId, BuiltDialog &out, const ControlFactory &factory)
{
	auto tmpl = res::Dialog(templateId);
	if(!tmpl)
		return false;
	out.dlgTemplate = *tmpl;
	if(tmpl->pointSize && tmpl->pointSize != 0x7FFF)
		dialog->setFont(DialogFont(*tmpl));
	out.units = UnitsFor(dialog);
	const res::DialogUnits &u = out.units;
	out.size = QSize(u.X(tmpl->cx), u.Y(tmpl->cy));
	if(!tmpl->caption.empty())
		dialog->setWindowTitle(Q(tmpl->caption));

	QButtonGroup *radioGroup = nullptr;
	QWidget *lastFocusable = nullptr;
	QLabel *pendingLabel = nullptr;
	std::vector<QGroupBox *> groupBoxes;
	for(std::size_t i = 0; i < tmpl->items.size(); i++)
	{
		const res::DialogItem &item = tmpl->items[i];
		QWidget *given = factory ? factory(item, dialog) : nullptr;
		if(given && given->parentWidget() != dialog)
			given->setParent(dialog);
		// A control that the factory made from the template is set up already.
		QWidget *w = (given && given->property("mfcless_built").toBool()) ? given : CreateControl(item, dialog, given);
		QRect rect(u.X(item.x), u.Y(item.y), u.X(item.cx), u.Y(item.cy));
		const Kind kind = KindOf(item);
		if(kind == Kind::ComboBox)
		{
			// The template's height is that of the drop-down list.
			rect.setHeight(std::min(rect.height(), std::max(u.Y(12), w->sizeHint().height())));
		}
		if(kind == Kind::UpDown && (item.style & 0x10) && lastFocusable)  // UDS_AUTOBUDDY
		{
			auto *ud = static_cast<UpDown *>(w);
			if(auto *edit = qobject_cast<QLineEdit *>(lastFocusable))
				ud->SetBuddy(edit);
			// UDS_ALIGNRIGHT 0x4 / UDS_ALIGNLEFT 0x8: take the space from the buddy.
			if(item.style & 0xC)
			{
				QRect b = lastFocusable->geometry();
				const int width = std::max(rect.width(), 16);
				if(item.style & 0x4)
				{
					rect = QRect(b.right() + 1 - width, b.top(), width, b.height());
					b.setRight(b.right() - width);
				} else
				{
					rect = QRect(b.left(), b.top(), width, b.height());
					b.setLeft(b.left() + width);
				}
				lastFocusable->setGeometry(b);
			}
		}
		w->setGeometry(rect);
		if(item.style & Style::Visible)
			w->show();
		else
			w->hide();

		// Radio buttons: a group runs from a control with WS_GROUP to the next.
		if(item.style & Style::Group)
			radioGroup = nullptr;
		if(kind == Kind::RadioButton && qobject_cast<QAbstractButton *>(w))
		{
			if(!radioGroup)
			{
				radioGroup = new QButtonGroup(dialog);
				radioGroup->setExclusive((item.style & 0x0F) == 9);  // BS_AUTORADIOBUTTON
				out.radioGroups.push_back(radioGroup);
			}
			radioGroup->addButton(static_cast<QAbstractButton *>(w));
		}

		// A label's '&' mnemonic focuses the next control that takes focus.
		if(pendingLabel && w->focusPolicy() != Qt::NoFocus && kind != Kind::Label && kind != Kind::GroupBox)
		{
			pendingLabel->setBuddy(w);
			pendingLabel = nullptr;
		}
		if(kind == Kind::Label && !(item.style & 0x80) && item.text.find(u'&') != std::u16string::npos)
			pendingLabel = static_cast<QLabel *>(w);
		if(kind == Kind::GroupBox)
			groupBoxes.push_back(static_cast<QGroupBox *>(w));

		if(w->focusPolicy() != Qt::NoFocus && kind != Kind::Label && kind != Kind::GroupBox)
			lastFocusable = w;
		out.all.push_back(w);
		if(item.id != 0 && item.id != 0xFFFF && item.id != 0xFFFFFFFF)
			out.items.emplace(item.id, w);
	}
	// Group boxes are drawn behind the controls they frame.
	for(QGroupBox *g : groupBoxes)
		g->lower();
	// Tab order is template order.
	QWidget *prev = nullptr;
	for(QWidget *w : out.all)
	{
		if(w->focusPolicy() == Qt::NoFocus)
			continue;
		if(prev)
			QWidget::setTabOrder(prev, w);
		prev = w;
	}

	// DLGINIT: combo box strings.
	for(const auto &[id, strings] : res::DialogInit(templateId))
	{
		auto it = out.items.find(id);
		if(it == out.items.end())
			continue;
		if(auto *combo = qobject_cast<QComboBox *>(it->second))
		{
			for(const auto &s : strings)
				combo->addItem(QString::fromLocal8Bit(s.data(), static_cast<qsizetype>(s.size())));
		}
	}

	dialog->resize(out.size);
	return true;
}


DialogLayout::DialogLayout(QWidget *dialog, const BuiltDialog &built, std::vector<res::DialogLayoutItem> rules)
	: QObject(dialog), m_dialog(dialog), m_baseSize(built.size)
{
	for(std::size_t i = 0; i < built.all.size() && i < rules.size(); i++)
	{
		const auto &r = rules[i];
		if(r.moveX || r.moveY || r.sizeX || r.sizeY)
			m_entries.push_back({built.all[i], built.all[i]->geometry(), r});
	}
	dialog->installEventFilter(this);
}

bool DialogLayout::eventFilter(QObject *watched, QEvent *e)
{
	if(watched == m_dialog && e->type() == QEvent::Resize && m_enabled)
		Apply();
	return false;
}

void DialogLayout::Replace(QWidget *old, QWidget *now)
{
	for(auto &entry : m_entries)
	{
		if(entry.widget == old)
			entry.widget = now;
	}
}


void DialogLayout::SetEnabled(bool enable)
{
	m_enabled = enable;
	if(enable)
		Apply();
}


void DialogLayout::Rebase(QWidget *control)
{
	for(auto &entry : m_entries)
	{
		if(entry.widget == control)
		{
			const int dx = m_dialog->width() - m_baseSize.width(), dy = m_dialog->height() - m_baseSize.height();
			const QRect g = control->geometry();
			entry.rect = QRect(g.x() - dx * entry.rule.moveX / 100, g.y() - dy * entry.rule.moveY / 100,
				g.width() - dx * entry.rule.sizeX / 100, g.height() - dy * entry.rule.sizeY / 100);
		}
	}
}

void DialogLayout::Apply()
{
	const int dx = std::max(0, m_dialog->width() - m_baseSize.width());
	const int dy = std::max(0, m_dialog->height() - m_baseSize.height());
	for(const auto &entry : m_entries)
	{
		const auto &r = entry.rule;
		entry.widget->setGeometry(entry.rect.x() + dx * r.moveX / 100, entry.rect.y() + dy * r.moveY / 100,
			entry.rect.width() + dx * r.sizeX / 100, entry.rect.height() + dy * r.sizeY / 100);
	}
}

} // namespace mfcless
