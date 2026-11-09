// The "Edit" window class: single-line edits on QLineEdit, multi-line ones
// on QPlainTextEdit, inside the edit window.
//
// Keyboard input still runs through the window as WM_KEYDOWN and WM_CHAR,
// so subclassed edits (upstream's number fields filter WM_CHAR) and dialog
// navigation see it first; the edit's own window procedure then hands the
// key to the Qt widget. Text and positions follow Windows: multi-line text
// has "\r\n" line breaks, and positions count them as two characters.
#include "QtWin.h"

#include <QtGui/QClipboard>
#include <QtGui/QFocusEvent>
#include <QtGui/QTextBlock>
#include <QtGui/QTextCursor>
#include <QtGui/QTextDocument>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QScrollBar>

#include <algorithm>

namespace linopenmpt::qtwin
{

// Events the edit procedure hands back to its Qt widget pass the filter
// (and, when the widget ignores them, are not delivered again by its host).
bool g_reinjecting = false;

namespace
{

// The Qt key event being delivered as window messages.
QKeyEvent *g_currentKey = nullptr;

void Reinject(QWidget *target, QEvent *e)
{
	const bool was = g_reinjecting;
	g_reinjecting = true;
	QCoreApplication::sendEvent(target, e);
	g_reinjecting = was;
}

struct EditState : ControlState
{
	QLineEdit *line = nullptr;
	QPlainTextEdit *multi = nullptr;
	QObject *filter = nullptr;
	int limit = 30000;
	bool notify = true; // EN_CHANGE from Qt's change signals
	WCHAR passwordChar = 0;
	QWidget *Widget() const { return line ? static_cast<QWidget *>(line) : multi; }
	~EditState() override { delete filter; }
};

bool IsMultiline(HWND hwnd)
{
	return (::GetWindowLongPtr(hwnd, GWL_STYLE) & ES_MULTILINE) != 0;
}

bool InsideDialog(HWND hwnd)
{
	const HWND parent = ::GetParent(hwnd);
	return parent != nullptr && (HIWORD(::SendMessage(parent, DM_GETDEFID, 0, 0)) == DC_HASDEFID || ::GetWindowLongPtr(parent, DWLP_DLGPROC) != 0);
}

// ---- text and positions --------------------------------------------------------

QString QtText(const EditState &s)
{
	return s.line ? s.line->text() : s.multi->toPlainText();
}

// Windows text: CR LF line breaks.
QString WinText(const EditState &s)
{
	QString t = QtText(s);
	if(s.multi)
		t.replace(QLatin1Char('\n'), QLatin1String("\r\n"));
	return t;
}

int ToWin(const EditState &s, int qtPos)
{
	if(!s.multi)
		return qtPos;
	const QString t = s.multi->toPlainText();
	qtPos = std::clamp(qtPos, 0, static_cast<int>(t.size()));
	return qtPos + static_cast<int>(t.left(qtPos).count(QLatin1Char('\n')));
}

int ToQt(const EditState &s, int winPos)
{
	if(!s.multi)
		return std::clamp(winPos, 0, static_cast<int>(s.line->text().size()));
	const QString t = s.multi->toPlainText();
	int win = 0;
	for(int i = 0; i < t.size(); i++)
	{
		const int step = t.at(i) == QLatin1Char('\n') ? 2 : 1;
		if(win + step > winPos)
			return i;
		win += step;
	}
	return static_cast<int>(t.size());
}

void GetSelection(const EditState &s, int &start, int &end)
{
	if(s.line)
	{
		if(s.line->hasSelectedText())
		{
			start = s.line->selectionStart();
			end = start + static_cast<int>(s.line->selectedText().size());
		} else
		{
			start = end = s.line->cursorPosition();
		}
	} else
	{
		const QTextCursor c = s.multi->textCursor();
		start = ToWin(s, c.selectionStart());
		end = ToWin(s, c.selectionEnd());
	}
}

void SetSelection(EditState &s, int start, int end)
{
	const int length = static_cast<int>(WinText(s).size());
	if(start < 0)
	{
		// -1: no selection, the caret stays.
		int a, b;
		GetSelection(s, a, b);
		start = end = b;
	}
	if(end < 0 || end > length)
		end = length;
	start = std::min(start, length);
	const int anchor = std::min(start, end), caret = std::max(start, end);
	if(s.line)
	{
		s.line->setSelection(anchor, caret - anchor);
		if(anchor == caret)
			s.line->setCursorPosition(caret);
	} else
	{
		QTextCursor c = s.multi->textCursor();
		c.setPosition(ToQt(s, anchor));
		c.setPosition(ToQt(s, caret), QTextCursor::KeepAnchor);
		s.multi->setTextCursor(c);
	}
}

void ReplaceSelection(EditState &s, const QString &winText)
{
	QString text = winText;
	if(s.multi)
	{
		text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
		s.multi->insertPlainText(text);
	} else
	{
		text.remove(QLatin1Char('\r'));
		text.replace(QLatin1Char('\n'), QLatin1Char(' '));
		s.line->insert(text);
	}
}

QTextBlock Line(const EditState &s, int line)
{
	return s.multi->document()->findBlockByNumber(line);
}

int LineCount(const EditState &s)
{
	return s.multi ? s.multi->document()->blockCount() : 1;
}

int LineFromChar(const EditState &s, int winPos)
{
	if(!s.multi)
		return 0;
	if(winPos < 0)
	{
		int a, b;
		GetSelection(s, a, b);
		winPos = a;
	}
	return s.multi->document()->findBlock(ToQt(s, winPos)).blockNumber();
}

int LineIndex(const EditState &s, int line)
{
	if(!s.multi)
		return line <= 0 ? 0 : -1;
	if(line < 0)
		line = LineFromChar(s, -1);
	const QTextBlock b = Line(s, line);
	return b.isValid() ? ToWin(s, b.position()) : -1;
}

int LineLength(const EditState &s, int winPos)
{
	if(!s.multi)
		return static_cast<int>(s.line->text().size());
	if(winPos < 0)
	{
		// The unselected characters on the lines with the selection.
		int a, b;
		GetSelection(s, a, b);
		const QTextBlock first = s.multi->document()->findBlock(ToQt(s, a));
		const QTextBlock last = s.multi->document()->findBlock(ToQt(s, b));
		return (ToQt(s, a) - first.position()) + (last.position() + last.length() - 1 - ToQt(s, b));
	}
	const QTextBlock b = s.multi->document()->findBlock(ToQt(s, winPos));
	return b.isValid() ? b.length() - 1 : 0;
}

int FirstVisibleLine(const EditState &s)
{
	if(!s.multi)
		return 0;
	return s.multi->cursorForPosition(QPoint(0, 0)).block().blockNumber();
}

// ---- notifications and colours ---------------------------------------------------

void TextChanged(HWND hwnd)
{
	EditState *s = Control<EditState>(hwnd);
	if(s == nullptr)
		return;
	if(WindowData *d = Data(hwnd))
		d->text = WinText(*s).toStdWString();
	if(!s->notify)
		return;
	NotifyParent(hwnd, EN_UPDATE);
	if(Control<EditState>(hwnd))
		NotifyParent(hwnd, EN_CHANGE);
}

// The parent's WM_CTLCOLOREDIT (WM_CTLCOLORSTATIC when read-only or
// disabled) colours, as the Qt widget's palette.
void UpdateColors(HWND hwnd, EditState &s)
{
	QWidget *w = s.Widget();
	const LONG_PTR style = ::GetWindowLongPtr(hwnd, GWL_STYLE);
	const UINT msg = ((style & ES_READONLY) || !::IsWindowEnabled(hwnd)) ? WM_CTLCOLORSTATIC : WM_CTLCOLOREDIT;
	HDC dc = ::CreateCompatibleDC(nullptr);
	::SelectObject(dc, Data(hwnd)->font ? Data(hwnd)->font : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
	HBRUSH brush = ControlBrush(hwnd, dc, msg);
	const COLORREF text = ::GetTextColor(dc);
	LOGBRUSH lb{};
	const bool hasBrush = brush != nullptr && ::GetObject(brush, sizeof(lb), &lb) == sizeof(lb) && lb.lbStyle == BS_SOLID;
	::DeleteDC(dc);
	QPalette pal = w->palette();
	const QColor base = hasBrush ? ToQColor(lb.lbColor) : QApplication::palette().color(QPalette::Base);
	QPalette::ColorGroup groups[] = {QPalette::Active, QPalette::Inactive};
	for(QPalette::ColorGroup g : groups)
	{
		if(pal.color(g, QPalette::Base) != base)
			pal.setColor(g, QPalette::Base, base);
		if(pal.color(g, QPalette::Text) != ToQColor(text))
			pal.setColor(g, QPalette::Text, ToQColor(text));
	}
	if(pal != w->palette())
		w->setPalette(pal);
}

// ---- keyboard -----------------------------------------------------------------------

// Keys that move the caret or edit (handled at WM_KEYDOWN); text, Backspace
// and control characters come as WM_CHAR.
bool IsEditingKey(int key)
{
	switch(key)
	{
	case Qt::Key_Left:
	case Qt::Key_Right:
	case Qt::Key_Up:
	case Qt::Key_Down:
	case Qt::Key_Home:
	case Qt::Key_End:
	case Qt::Key_PageUp:
	case Qt::Key_PageDown:
	case Qt::Key_Delete:
	case Qt::Key_Insert:
		return true;
	default:
		return false;
	}
}

void SendQtKey(EditState &s, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
	QKeyEvent press(QEvent::KeyPress, key, modifiers);
	Reinject(s.Widget(), &press);
}

void KeyDown(HWND hwnd, EditState &s, WPARAM vk)
{
	const bool multi = s.multi != nullptr;
	const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
	if(multi && InsideDialog(hwnd))
	{
		const HWND parent = ::GetParent(hwnd);
		if(vk == VK_TAB)
		{
			::SendMessage(parent, WM_NEXTDLGCTL, shift, 0);
			return;
		}
		if(vk == VK_RETURN && !(::GetWindowLongPtr(hwnd, GWL_STYLE) & ES_WANTRETURN))
		{
			const LRESULT def = ::SendMessage(parent, DM_GETDEFID, 0, 0);
			const UINT id = HIWORD(def) == DC_HASDEFID ? LOWORD(def) : IDOK;
			::SendMessage(parent, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), reinterpret_cast<LPARAM>(::GetDlgItem(parent, static_cast<int>(id))));
			return;
		}
		if(vk == VK_ESCAPE)
		{
			::PostMessage(parent, WM_CLOSE, 0, 0);
			return;
		}
	}
	if(g_currentKey != nullptr && IsEditingKey(g_currentKey->key()))
	{
		QKeyEvent copy(QEvent::KeyPress, g_currentKey->key(), g_currentKey->modifiers(), QString(), g_currentKey->isAutoRepeat());
		Reinject(s.Widget(), &copy);
	}
}

void Char(HWND hwnd, EditState &s, WCHAR ch)
{
	const LONG_PTR style = ::GetWindowLongPtr(hwnd, GWL_STYLE);
	const bool readOnly = (style & ES_READONLY) != 0;
	switch(ch)
	{
	case 0x01: // Ctrl+A
		SetSelection(s, 0, -1);
		return;
	case 0x03: // Ctrl+C
		::SendMessage(hwnd, WM_COPY, 0, 0);
		return;
	case 0x16: // Ctrl+V
		if(!readOnly)
			::SendMessage(hwnd, WM_PASTE, 0, 0);
		return;
	case 0x18: // Ctrl+X
		if(!readOnly)
			::SendMessage(hwnd, WM_CUT, 0, 0);
		return;
	case 0x1A: // Ctrl+Z
		if(!readOnly)
			::SendMessage(hwnd, WM_UNDO, 0, 0);
		return;
	case '\b':
		if(!readOnly)
			SendQtKey(s, Qt::Key_Backspace, (::GetKeyState(VK_CONTROL) & 0x8000) ? Qt::ControlModifier : Qt::NoModifier);
		return;
	case 0x7F: // Ctrl+Backspace
		if(!readOnly)
			SendQtKey(s, Qt::Key_Backspace, Qt::ControlModifier);
		return;
	case '\r':
		if(s.multi && !readOnly && ((style & ES_WANTRETURN) || !InsideDialog(hwnd)))
			ReplaceSelection(s, QStringLiteral("\r\n"));
		return;
	case '\t':
		if(s.multi && !readOnly && !InsideDialog(hwnd))
			ReplaceSelection(s, QStringLiteral("\t"));
		return;
	default:
		break;
	}
	if(ch < 0x20 || readOnly)
		return;
	QChar c(static_cast<char16_t>(ch));
	if((style & ES_NUMBER) && !c.isDigit())
	{
		::MessageBeep(0);
		return;
	}
	if(style & ES_UPPERCASE)
		c = c.toUpper();
	else if(style & ES_LOWERCASE)
		c = c.toLower();
	int a, b;
	GetSelection(s, a, b);
	if(WinText(s).size() - (b - a) + 1 > s.limit)
	{
		NotifyParent(hwnd, EN_MAXTEXT);
		return;
	}
	ReplaceSelection(s, QString(c));
}

// Key events, the focus, colours and the context menu of the Qt widget go
// through the edit window.
class EditFilter : public QObject
{
public:
	EditFilter(HWND hwnd, QObject *parent) : QObject(parent), hwnd(hwnd) { }

protected:
	bool eventFilter(QObject *watched, QEvent *e) override
	{
		if(g_reinjecting)
			return false;
		Win32Widget *host = FromHandle(hwnd);
		EditState *s = Control<EditState>(hwnd);
		if(host == nullptr || s == nullptr)
			return false;
		switch(e->type())
		{
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
			{
				auto *key = static_cast<QKeyEvent *>(e);
				QKeyEvent *previous = g_currentKey;
				g_currentKey = key;
				host->DeliverKey(key);
				g_currentKey = previous;
				return true;
			}
		case QEvent::ShortcutOverride:
			e->accept();
			return true;
		case QEvent::FocusIn:
			// A popup (a combo box's list) takes Qt's focus but not Windows'.
			if(watched == s->Widget() && static_cast<QFocusEvent *>(e)->reason() != Qt::PopupFocusReason)
				host->HandleFocusIn();
			return false;
		case QEvent::FocusOut:
			if(watched == s->Widget() && static_cast<QFocusEvent *>(e)->reason() != Qt::PopupFocusReason)
				host->HandleFocusOut();
			return false;
		case QEvent::Paint:
			UpdateColors(hwnd, *s);
			return false;
		case QEvent::ContextMenu:
			{
				auto *menu = static_cast<QContextMenuEvent *>(e);
				const QPoint global = menu->reason() == QContextMenuEvent::Keyboard ? QPoint(-1, -1) : menu->globalPos();
				pendingMenu = menu;
				menuWidget = static_cast<QWidget *>(watched);
				::SendMessage(hwnd, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(global.x(), global.y()));
				pendingMenu = nullptr;
				return true;
			}
		default:
			return false;
		}
	}

public:
	HWND hwnd;
	QContextMenuEvent *pendingMenu = nullptr;
	QWidget *menuWidget = nullptr;
};

void CreateWidget(HWND hwnd, const CREATESTRUCT *cs)
{
	WindowData *d = Data(hwnd);
	Win32Widget *w = d->widget;
	auto state = std::make_unique<EditState>();
	EditState &s = *state;
	const DWORD style = d->style;
	const bool framed = (style & WS_BORDER) || (d->exStyle & WS_EX_CLIENTEDGE);
	// The Qt widget draws the frame and scroll bars.
	d->scroll[SB_HORZ].shown = d->scroll[SB_VERT].shown = false;
	QWidget *widget;
	if(style & ES_MULTILINE)
	{
		s.multi = new QPlainTextEdit(w);
		s.multi->setFrameShape(framed ? QFrame::StyledPanel : QFrame::NoFrame);
		s.multi->setLineWrapMode((style & (WS_HSCROLL | ES_AUTOHSCROLL)) ? QPlainTextEdit::NoWrap : QPlainTextEdit::WidgetWidth);
		s.multi->setVerticalScrollBarPolicy((style & WS_VSCROLL) ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
		s.multi->setHorizontalScrollBarPolicy((style & WS_HSCROLL) ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
		s.multi->setTabChangesFocus(false);
		s.multi->setReadOnly(style & ES_READONLY);
		if(style & ES_READONLY)
			s.multi->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		s.multi->setContextMenuPolicy(Qt::DefaultContextMenu);
		QObject::connect(s.multi, &QPlainTextEdit::textChanged, s.multi, [hwnd]() { TextChanged(hwnd); });
		QObject::connect(s.multi->verticalScrollBar(), &QScrollBar::valueChanged, s.multi, [hwnd]() { NotifyParent(hwnd, EN_VSCROLL); });
		QObject::connect(s.multi->horizontalScrollBar(), &QScrollBar::valueChanged, s.multi, [hwnd]() { NotifyParent(hwnd, EN_HSCROLL); });
		widget = s.multi;
	} else
	{
		s.line = new QLineEdit(w);
		s.line->setFrame(framed);
		s.line->setReadOnly(style & ES_READONLY);
		s.line->setMaxLength(s.limit);
		if(style & ES_PASSWORD)
		{
			s.line->setEchoMode(QLineEdit::Password);
			s.passwordChar = L'*';
		}
		if(style & ES_CENTER)
			s.line->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		else if(style & ES_RIGHT)
			s.line->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		QObject::connect(s.line, &QLineEdit::textChanged, s.line, [hwnd]() { TextChanged(hwnd); });
		widget = s.line;
	}
	auto *filter = new EditFilter(hwnd, nullptr);
	s.filter = filter;
	widget->installEventFilter(filter);
	if(s.multi)
		s.multi->viewport()->installEventFilter(filter);
	widget->setFocusPolicy(Qt::StrongFocus);
	w->setFocusProxy(widget);
	w->setFocusPolicy(Qt::StrongFocus);
	if(cs != nullptr && cs->lpszName != nullptr && cs->lpszName[0] != 0)
	{
		s.notify = false;
		QString text = ToQString(cs->lpszName);
		if(s.multi)
			s.multi->setPlainText(text.replace(QLatin1String("\r\n"), QLatin1String("\n")));
		else
			s.line->setText(text);
		s.notify = true;
	}
	if(s.multi)
		s.multi->document()->setModified(false);
	else
		s.line->setModified(false);
	d->control = std::move(state);
	widget->setGeometry(ClientRect(w));
	widget->show();
}

void SetText(HWND hwnd, EditState &s, LPCWSTR text)
{
	QString t = ToQString(text ? text : L"");
	// Multi-line edits send no EN_CHANGE for WM_SETTEXT, single-line ones do.
	s.notify = s.line != nullptr;
	if(s.multi)
	{
		s.multi->setPlainText(t.replace(QLatin1String("\r\n"), QLatin1String("\n")));
		s.multi->document()->setModified(false);
	} else
	{
		t.remove(QLatin1Char('\r'));
		t.replace(QLatin1Char('\n'), QLatin1Char(' '));
		if(s.line->text() != t)
			s.line->setText(t);
		else
			TextChanged(hwnd);
		s.line->setModified(false);
	}
	if(EditState *again = Control<EditState>(hwnd))
		again->notify = true;
	if(WindowData *d = Data(hwnd))
		d->text = text ? text : L"";
}

LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	EditState *s = Control<EditState>(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		CreateWidget(hwnd, reinterpret_cast<const CREATESTRUCT *>(lp));
		return TRUE;
	case WM_NCCALCSIZE:
		// No non-client area: the Qt widget draws its frame.
		return 0;
	case WM_NCPAINT:
		return 0;
	case WM_SIZE:
		if(s != nullptr)
			s->Widget()->setGeometry(ClientRect(FromHandle(hwnd)));
		return 0;
	case WM_ERASEBKGND:
		return 1;
	case WM_GETDLGCODE:
		{
			LRESULT code = DLGC_WANTCHARS | DLGC_HASSETSEL | DLGC_WANTARROWS;
			if(IsMultiline(hwnd))
				code |= DLGC_WANTALLKEYS;
			if(lp != 0)
			{
				const MSG *m = reinterpret_cast<const MSG *>(lp);
				if(m->message == WM_KEYDOWN && m->wParam == VK_ESCAPE && !IsMultiline(hwnd))
					code &= ~DLGC_WANTALLKEYS;
			}
			return code;
		}
	case WM_SETTEXT:
		if(s != nullptr)
		{
			SetText(hwnd, *s, reinterpret_cast<LPCWSTR>(lp));
			return TRUE;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_GETTEXT:
		if(s != nullptr)
		{
			auto *buf = reinterpret_cast<LPWSTR>(lp);
			if(buf == nullptr || wp == 0)
				return 0;
			const std::wstring text = WinText(*s).toStdWString();
			const std::size_t n = std::min<std::size_t>(text.size(), wp - 1);
			std::copy_n(text.c_str(), n, buf);
			buf[n] = 0;
			return static_cast<LRESULT>(n);
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_GETTEXTLENGTH:
		return s != nullptr ? static_cast<LRESULT>(WinText(*s).toStdWString().size()) : 0;
	case WM_SETFONT:
		::DefWindowProc(hwnd, msg, wp, 0);
		if(s != nullptr)
			s->Widget()->setFont(FontOf(reinterpret_cast<HFONT>(wp)));
		return 0;
	case WM_ENABLE:
		if(s != nullptr)
			s->Widget()->update();
		return 0;
	case WM_SETFOCUS:
		NotifyParent(hwnd, EN_SETFOCUS);
		return 0;
	case WM_KILLFOCUS:
		NotifyParent(hwnd, EN_KILLFOCUS);
		return 0;
	case WM_KEYDOWN:
		if(s != nullptr)
			KeyDown(hwnd, *s, wp);
		return 0;
	case WM_CHAR:
		if(s != nullptr)
			Char(hwnd, *s, static_cast<WCHAR>(wp));
		return 0;
	case WM_KEYUP:
	case WM_SYSCHAR:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CONTEXTMENU:
		if(s != nullptr)
		{
			auto *filter = static_cast<EditFilter *>(s->filter);
			if(filter->pendingMenu != nullptr)
			{
				Reinject(filter->menuWidget, filter->pendingMenu);
			} else
			{
				const QPoint global = GET_X_LPARAM(lp) == -1 ? s->Widget()->mapToGlobal(QPoint(0, 0)) : QPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
				QWidget *target = s->multi ? s->multi->viewport() : s->Widget();
				QContextMenuEvent e(QContextMenuEvent::Mouse, target->mapFromGlobal(global), global);
				Reinject(target, &e);
			}
		}
		return 0;
	case WM_CUT:
		if(s != nullptr)
			s->line ? s->line->cut() : s->multi->cut();
		return 0;
	case WM_COPY:
		if(s != nullptr)
			s->line ? s->line->copy() : s->multi->copy();
		return 0;
	case WM_PASTE:
		if(s != nullptr && !(::GetWindowLongPtr(hwnd, GWL_STYLE) & ES_READONLY))
			ReplaceSelection(*s, QApplication::clipboard()->text());
		return 0;
	case WM_CLEAR:
		if(s != nullptr && !(::GetWindowLongPtr(hwnd, GWL_STYLE) & ES_READONLY))
			ReplaceSelection(*s, QString());
		return 0;
	case WM_UNDO:
	case EM_UNDO:
		if(s != nullptr)
			s->line ? s->line->undo() : s->multi->undo();
		return TRUE;
	case EM_CANUNDO:
		return s != nullptr && (s->line ? s->line->isUndoAvailable() : s->multi->document()->isUndoAvailable());
	case EM_EMPTYUNDOBUFFER:
		if(s != nullptr && s->multi)
			s->multi->document()->clearUndoRedoStacks();
		else if(s != nullptr)
		{
			// QLineEdit keeps its history until the text is set.
			const QString t = s->line->text();
			const int cursor = s->line->cursorPosition();
			const bool notify = s->notify;
			s->notify = false;
			s->line->setText(t);
			s->line->setCursorPosition(cursor);
			s->notify = notify;
		}
		return 0;
	case EM_GETSEL:
		if(s != nullptr)
		{
			int a, b;
			GetSelection(*s, a, b);
			if(wp)
				*reinterpret_cast<DWORD *>(wp) = static_cast<DWORD>(a);
			if(lp)
				*reinterpret_cast<DWORD *>(lp) = static_cast<DWORD>(b);
			return MAKELONG(std::min(a, 0xFFFF), std::min(b, 0xFFFF));
		}
		return 0;
	case EM_SETSEL:
		if(s != nullptr)
			SetSelection(*s, static_cast<int>(wp), static_cast<int>(lp));
		return 0;
	case EM_REPLACESEL:
		if(s != nullptr)
			ReplaceSelection(*s, ToQString(lp ? reinterpret_cast<LPCWSTR>(lp) : L""));
		return 0;
	case EM_LIMITTEXT:
		if(s != nullptr)
		{
			s->limit = wp == 0 ? (s->multi ? 0x7FFFFFFE : 0x7FFFFFFE) : static_cast<int>(std::min<WPARAM>(wp, 0x7FFFFFFE));
			if(s->line)
				s->line->setMaxLength(std::min(s->limit, 0x7FFFFFFE));
		}
		return 0;
	case EM_GETLIMITTEXT:
		return s != nullptr ? s->limit : 0;
	case EM_SETREADONLY:
		if(s != nullptr)
		{
			LONG_PTR style = ::GetWindowLongPtr(hwnd, GWL_STYLE);
			style = wp ? (style | ES_READONLY) : (style & ~LONG_PTR(ES_READONLY));
			if(WindowData *d = Data(hwnd))
				d->style = static_cast<DWORD>(style) & ~WS_VISIBLE;
			if(s->line)
				s->line->setReadOnly(wp != 0);
			else
				s->multi->setReadOnly(wp != 0);
			s->Widget()->update();
		}
		return TRUE;
	case EM_GETMODIFY:
		return s != nullptr && (s->line ? s->line->isModified() : s->multi->document()->isModified());
	case EM_SETMODIFY:
		if(s != nullptr)
			s->line ? s->line->setModified(wp != 0) : s->multi->document()->setModified(wp != 0);
		return 0;
	case EM_GETLINECOUNT:
		return s != nullptr ? LineCount(*s) : 1;
	case EM_LINEINDEX:
		return s != nullptr ? LineIndex(*s, static_cast<int>(wp)) : 0;
	case EM_LINELENGTH:
		return s != nullptr ? LineLength(*s, static_cast<int>(wp)) : 0;
	case EM_LINEFROMCHAR:
		return s != nullptr ? LineFromChar(*s, static_cast<int>(wp)) : 0;
	case EM_GETLINE:
		if(s != nullptr && lp != 0)
		{
			// The buffer starts with its size in characters.
			auto *buf = reinterpret_cast<LPWSTR>(lp);
			const WORD max = *reinterpret_cast<const WORD *>(buf);
			QString text;
			if(s->multi)
			{
				const QTextBlock b = Line(*s, static_cast<int>(wp));
				if(!b.isValid())
					return 0;
				text = b.text();
			} else
			{
				text = s->line->text();
			}
			const std::wstring w = text.toStdWString();
			const std::size_t n = std::min<std::size_t>(w.size(), max);
			std::copy_n(w.c_str(), n, buf);
			return static_cast<LRESULT>(n);
		}
		return 0;
	case EM_GETFIRSTVISIBLELINE:
		return s != nullptr ? (s->multi ? FirstVisibleLine(*s) : 0) : 0;
	case EM_LINESCROLL:
		if(s != nullptr && s->multi)
		{
			s->multi->verticalScrollBar()->setValue(s->multi->verticalScrollBar()->value() + static_cast<int>(lp));
			s->multi->horizontalScrollBar()->setValue(s->multi->horizontalScrollBar()->value() + static_cast<int>(wp) * QFontMetrics(s->multi->font()).averageCharWidth());
			return TRUE;
		}
		return FALSE;
	case EM_SCROLL:
		if(s != nullptr && s->multi)
		{
			QScrollBar *bar = s->multi->verticalScrollBar();
			const int before = bar->value();
			switch(wp)
			{
			case SB_LINEUP: bar->triggerAction(QAbstractSlider::SliderSingleStepSub); break;
			case SB_LINEDOWN: bar->triggerAction(QAbstractSlider::SliderSingleStepAdd); break;
			case SB_PAGEUP: bar->triggerAction(QAbstractSlider::SliderPageStepSub); break;
			case SB_PAGEDOWN: bar->triggerAction(QAbstractSlider::SliderPageStepAdd); break;
			default: return FALSE;
			}
			return MAKELONG(static_cast<WORD>(bar->value() - before), TRUE);
		}
		return FALSE;
	case EM_SCROLLCARET:
		if(s != nullptr)
		{
			if(s->multi)
				s->multi->ensureCursorVisible();
			else
				s->line->setCursorPosition(s->line->cursorPosition());
		}
		return TRUE;
	case EM_SETPASSWORDCHAR:
		if(s != nullptr && s->line)
		{
			s->passwordChar = static_cast<WCHAR>(wp);
			s->line->setEchoMode(wp ? QLineEdit::Password : QLineEdit::Normal);
		}
		return 0;
	case EM_GETPASSWORDCHAR:
		return s != nullptr ? s->passwordChar : 0;
	case EM_SETMARGINS:
		if(s != nullptr)
		{
			QMargins m = s->line ? s->line->textMargins() : QMargins();
			if(wp & EC_LEFTMARGIN)
				m.setLeft(LOWORD(lp) == EC_USEFONTINFO ? 0 : LOWORD(lp));
			if(wp & EC_RIGHTMARGIN)
				m.setRight(HIWORD(lp) == EC_USEFONTINFO ? 0 : HIWORD(lp));
			if(s->line)
				s->line->setTextMargins(m);
			else
				s->multi->document()->setDocumentMargin(std::max(m.left(), m.right()));
		}
		return 0;
	case EM_GETMARGINS:
		if(s != nullptr && s->line)
			return MAKELONG(s->line->textMargins().left(), s->line->textMargins().right());
		return 0;
	case EM_SETCUEBANNER:
		if(s != nullptr && s->line)
			s->line->setPlaceholderText(ToQString(lp ? reinterpret_cast<LPCWSTR>(lp) : L""));
		return TRUE;
	case EM_GETCUEBANNER:
		if(s != nullptr && s->line && wp != 0 && lp > 0)
		{
			const std::wstring w = s->line->placeholderText().toStdWString();
			const std::size_t n = std::min<std::size_t>(w.size(), static_cast<std::size_t>(lp) - 1);
			std::copy_n(w.c_str(), n, reinterpret_cast<LPWSTR>(wp));
			reinterpret_cast<LPWSTR>(wp)[n] = 0;
			return TRUE;
		}
		return FALSE;
	case EM_SETTABSTOPS:
		if(s != nullptr && s->multi)
		{
			// In dialog template units (a quarter of the average character).
			const int units = wp == 0 ? 32 : *reinterpret_cast<const int *>(lp);
			s->multi->setTabStopDistance(QFontMetricsF(s->multi->font()).averageCharWidth() * units / 4.0);
		}
		return TRUE;
	case EM_GETRECT:
		if(lp != 0)
			::GetClientRect(hwnd, reinterpret_cast<RECT *>(lp));
		return 0;
	case EM_SETRECT:
	case EM_SETRECTNP:
	case EM_FMTLINES:
		return 0;
	case EM_POSFROMCHAR:
		if(s != nullptr)
		{
			QRect r;
			if(s->multi)
			{
				QTextCursor c(s->multi->document());
				c.setPosition(ToQt(*s, static_cast<int>(wp)));
				r = s->multi->cursorRect(c).translated(s->multi->viewport()->pos());
			} else
			{
				// QLineEdit only reports where its caret is.
				const QFontMetrics fm(s->line->font());
				r = QRect(fm.horizontalAdvance(s->line->text().left(static_cast<int>(wp))) + 2, 2, 1, fm.height());
			}
			return MAKELONG(static_cast<WORD>(r.left()), static_cast<WORD>(r.top()));
		}
		return -1;
	case EM_CHARFROMPOS:
		if(s != nullptr)
		{
			const QPoint pt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
			if(s->multi)
			{
				const QTextCursor c = s->multi->cursorForPosition(s->multi->viewport()->mapFrom(s->multi, pt));
				return MAKELONG(static_cast<WORD>(ToWin(*s, c.position())), static_cast<WORD>(c.block().blockNumber()));
			}
			return MAKELONG(static_cast<WORD>(s->line->cursorPositionAt(pt)), 0);
		}
		return 0;
	case WM_NCDESTROY:
		if(WindowData *d = Data(hwnd))
		{
			if(s != nullptr)
				s->Widget()->removeEventFilter(s->filter);
			d->control.reset();
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

} // namespace

void RegisterEditClass()
{
	WNDCLASS wc{};
	wc.style = CS_DBLCLKS | CS_PARENTDC;
	wc.lpfnWndProc = EditProc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_IBEAM);
	wc.lpszClassName = L"Edit";
	::RegisterClass(&wc);
}

} // namespace linopenmpt::qtwin
