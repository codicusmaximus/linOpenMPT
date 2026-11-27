/*
 * Ctrl_com.cpp
 * ------------
 * Purpose: Song comments tab, upper panel.
 * Notes  : mfcless twin of upstream's mptrack/Ctrl_com.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Ctrl_com.h"
#include "Globals.h"
#include "HighDPISupport.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "view_com.h"
#include "../soundlib/mod_specifications.h"
#include "mpt/format/join.hpp"
#include "mpt/string/utility.hpp"


//#define MPT_COMMENTS_LONG_LINES_WRAP
//#define MPT_COMMENTS_LONG_LINES_TRUNCATE


#define MPT_COMMENTS_MARGIN 4



OPENMPT_NAMESPACE_BEGIN


void CCtrlComments::RegisterMessageMap()
{
	CModControlDlg::RegisterMessageMap();
	AddNotify(IDC_EDIT_COMMENTS, mfcless::Notify::EditUpdate, &CCtrlComments::OnCommentsUpdated);
	AddNotify(IDC_EDIT_COMMENTS, mfcless::Notify::EditChange, &CCtrlComments::OnCommentsChanged);
}

void CCtrlComments::DoDataExchange(DataExchange &dx)
{
	CModControlDlg::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CCtrlComments)
	ExchangeControl(dx, IDC_EDIT_COMMENTS, m_EditComments);
	//}}AFX_DATA_MAP
}


CCtrlComments::CCtrlComments(CModControlView &parent, CModDoc &document) : CModControlDlg(parent, document)
{
}


const mfcless::ViewClass *CCtrlComments::GetAssociatedViewClass()
{
	return (&CViewComments::classCViewComments);
}


void CCtrlComments::OnActivatePage(intptr_t)
{
	// Don't stop generating VU meter messages
	m_modDoc.SetNotifications(Notification::Default);
	m_modDoc.SetFollowWnd(m_hWnd);
	m_EditComments->setFocus();
}


void CCtrlComments::OnDeactivatePage()
{
	CModControlDlg::OnDeactivatePage();
}


bool CCtrlComments::OnInitDialog()
{
	CModControlDlg::OnInitDialog();
	// Initialize comments
	uint32 margin = HighDPISupport::ScalePixels(MPT_COMMENTS_MARGIN, m_EditComments->m_hWnd);
	m_EditComments->SetMargins(margin, margin);
	UpdateView(CommentHint().ModType());
	m_EditComments->setFocus();
	m_EditComments->FmtLines(false);
	m_initialized = true;
	return false;
}


Setting<int32> &CCtrlComments::GetSplitPosRef() { return TrackerSettings::Instance().glCommentsWindowHeight; }


void CCtrlComments::RecalcLayout()
{
	mfcless::Rect rcClient, rect;
	int cx0, cy0;
	
	if ((!m_hWnd) || (!m_EditComments->m_hWnd)) return;
	GetClientRect(&rcClient);
	m_EditComments->GetWindowRect(&rect);
	ScreenToClient(&rect);
	cx0 = rect.Width();
	cy0 = rect.Height();
	rect.bottom = rcClient.bottom - 3;
	rect.right = rcClient.right - rect.left;
	if ((rect.right > rect.left) && (rect.bottom > rect.top))
	{
		int cx = rect.Width(), cy = rect.Height();
		if(m_sndFile.GetModSpecifications().commentLineLengthMax != 0)
		{
			const int cxBorder = HighDPISupport::GetSystemMetrics(SM_CXBORDER, GetDPI());
			const int cxVScroll = HighDPISupport::GetSystemMetrics(SM_CXVSCROLL, GetDPI());
			int cxmax = cxBorder + m_sndFile.GetModSpecifications().commentLineLengthMax * charWidth + cxVScroll + cxBorder + HighDPISupport::ScalePixels(MPT_COMMENTS_MARGIN + MPT_COMMENTS_MARGIN - 1, m_EditComments->m_hWnd);
			if(cx != cxmax && cxmax != 0)
				cx = cxmax;
			//SetWindowLong(m_EditComments->m_hWnd, GWL_STYLE, GetWindowLong(m_EditComments->m_hWnd, GWL_STYLE) & ~WS_HSCROLL);
		} else
		{
			//SetWindowLong(m_EditComments->m_hWnd, GWL_STYLE, GetWindowLong(m_EditComments->m_hWnd, GWL_STYLE) | WS_HSCROLL);
		}
		if(cx != cx0 || cy != cy0)
			m_EditComments->SetWindowPos(NULL, 0,0, cx, cy, SWP_NOMOVE|SWP_NOZORDER|SWP_DRAWFRAME);
	}
}


void CCtrlComments::OnDPIChanged()
{
	UpdateView(UpdateHint().MPTOptions(), nullptr);
	CModControlDlg::OnDPIChanged();
}


void CCtrlComments::UpdateView(UpdateHint hint, QObject *pHint)
{
	CommentHint commentHint = hint.ToType<CommentHint>();
	if(pHint == this || !commentHint.GetType()[HINT_MODCOMMENTS | HINT_MPTOPTIONS | HINT_MODTYPE])
		return;
	if(IsLocked())
		return;
	LockControls();

	QFont &hFont = CMainFrame::GetMainFrame()->GetCommentsFont();
	static FontSetting previousFont;
	static int previousFontSize = 0;
	FontSetting font = TrackerSettings::Instance().commentsFont;
	// Point size to pixels
	int32 fontSize = -MulDiv(font.size, GetDPI(), 720);
	if(previousFont != font || previousFontSize != fontSize)
	{
		previousFont = font;
		previousFontSize = fontSize;
		hFont.DeleteObject();
		hFont.CreateFont(fontSize, 0, 0, 0, font.flags[FontSetting::Bold] ? FW_BOLD : FW_NORMAL,
			font.flags[FontSetting::Italic] ? true :false, false, false,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
			FIXED_PITCH | FF_MODERN, mpt::ToUnicode(font.name));
	}
	m_EditComments->SendMessage(WM_SETFONT, reinterpret_cast<uintptr_t>(hFont.m_hObject));
	mfcless::DC *pDC = m_EditComments->GetDC();
	pDC->SelectObject(hFont);
	TEXTMETRIC tm;
	pDC->GetTextMetrics(&tm);
	charWidth = tm.tmAveCharWidth;
	m_EditComments->ReleaseDC(pDC);

	RecalcLayout();

	m_EditComments->setUpdatesEnabled(false);

	std::string text = m_sndFile.m_songMessage.GetFormatted(SongMessage::leCRLF);
	for(std::size_t i = 0; i < text.length(); ++i)
	{
		// replace control characters
		char c = text[i];
		if(c > '\0' && c < ' ' && c != '\r' && c != '\n')
		{
			c = ' ';
		}
		text[i] = c;
	}
	mpt::ustring new_text = mpt::ToUnicode(m_sndFile.GetCharsetInternal(), text);
	mpt::ustring old_text;
	m_EditComments->GetWindowText(old_text);
	if(new_text != old_text)
	{
		m_EditComments->SetWindowText(new_text);
	}

	if(commentHint.GetType() & HINT_MODTYPE)
	{
		m_EditComments->setReadOnly(!m_sndFile.GetModSpecifications().hasComments);
	}

	m_EditComments->setUpdatesEnabled(true);
	UnlockControls();
}


void CCtrlComments::OnCommentsUpdated()
{

#if defined(MPT_COMMENTS_LONG_LINES_TRUNCATE) || defined(MPT_COMMENTS_LONG_LINES_WRAP)

	if(m_Reformatting)
	{
		return;
	}

	if(!m_sndFile.GetModSpecifications().hasComments)
	{
		return;
	}
	if(m_sndFile.GetModSpecifications().commentLineLengthMax == 0)
	{
		return;
	}

	m_Reformatting = true;
	const std::size_t maxline = m_sndFile.GetModSpecifications().commentLineLengthMax;
	int beg = 0;
	int end = 0;
	m_EditComments->GetSel(beg, end);
	mpt::ustring text;
	m_EditComments->GetWindowText(text);
	std::string lines_new;
	lines_new.reserve(text.GetLength());
	bool modified = false;
	std::size_t pos = 0;

#if defined(MPT_COMMENTS_LONG_LINES_WRAP)

	std::string lines = text.GetString();
	std::size_t line_length = 0;
	for(std::size_t i = 0; i < lines.length(); ++i)
	{
		if(lines[i] == '\r')
		{
			// nothing
		} else if (lines[i] == '\n')
		{
			line_length = 0;
		} else
		{
			line_length += 1;
		}
		if(line_length > maxline)
		{
			modified = true;
			lines_new.push_back('\r');
			lines_new.push_back('\n');
			if(beg >= 0)
			{
				if(beg >= pos)
				{
					beg += 2;
				}
			}
			if(end >= 0)
			{
				if(end >= pos)
				{
					end += 2;
				}
			}
			pos += 2;
			line_length = 1;
		}
		lines_new.push_back(lines[i]);
		pos++;
	}

#elif defined(MPT_COMMENTS_LONG_LINES_TRUNCATE)

	std::vector<std::string> lines = mpt::split(std::string(text.GetString()), std::string("\r\n"));
	for(std::size_t i = 0; i < lines.size(); ++i)
	{
		if(i > 0)
		{
			pos += 2;
		}
		if(lines[i].length() > maxline)
		{
			modified = true;
			pos += maxline;
			for(std::size_t n = 0; n < lines[i].length() - maxline; ++n)
			{
				if(beg >= 0)
				{
					if(beg > pos)
					{
						beg--;
					}
				}
				if(end >= 0)
				{
					if(end > pos)
					{
						end--;
					}
				}
			}
			lines[i] = lines[i].substr(0, maxline);
		} else
		{
			pos += lines[i].length();
		}
	}
	lines_new = mpt::join_format(lines, std::string("\r\n"));

#endif

	if(modified)
	{
		text = lines_new.c_str();
		m_EditComments->SetWindowText(text);
		m_EditComments->SetSel(beg, end);
	}
	m_Reformatting = false;

#endif

}


void CCtrlComments::OnCommentsChanged()
{
	if(m_nLockCount)
		return;
	if(!m_initialized || !m_EditComments->m_hWnd || !m_EditComments->GetModify())
		return;
	mpt::ustring text;
	m_EditComments->GetWindowText(text);
	m_EditComments->SetModify(false);
	if(m_sndFile.m_songMessage.SetFormatted(mpt::ToCharset(m_sndFile.GetCharsetInternal(), text), SongMessage::leCRLF))
	{
		m_modDoc.SetModified();
		m_modDoc.UpdateAllViews(nullptr, CommentHint(), this);
	}
}


bool CCtrlComments::PreTranslateMessage(mfcless::Message *pMsg)
{
	if(pMsg->message == mfcless::msg::KeyDown && pMsg->wParam == 'A' && GetKeyState(mfcless::vk::CONTROL) < 0)
	{
		// Ctrl-A is not handled by multiline edit boxes
		if(::GetFocus() == m_EditComments->m_hWnd)
			m_EditComments->SetSel(0, -1);
	} else if(pMsg->message == mfcless::msg::KeyDown && pMsg->wParam == mfcless::vk::TAB && CInputHandler::GetModifierMask() == ModNone)
	{
		int selStart, selEnd;
		m_EditComments->GetSel(selStart, selEnd);
		int posInLine = (selStart - m_EditComments->LineIndex(m_EditComments->LineFromChar(selStart)));
		mpt::ustring tabs(_T(' '), 4 - (posInLine % 4));
		m_EditComments->ReplaceSel(tabs, true);
		m_EditComments->SetSel(-1, -1, true);
		return true;
	}
	return CModControlDlg::PreTranslateMessage(pMsg);
}


OPENMPT_NAMESPACE_END
