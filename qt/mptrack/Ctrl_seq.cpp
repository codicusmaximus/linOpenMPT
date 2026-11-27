/*
 * Ctrl_seq.cpp
 * ------------
 * Purpose: Order list for the pattern editor upper panel.
 * Notes  : mfcless twin of upstream's mptrack/Ctrl_seq.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Ctrl_pat.h"
#include "mfcless/Widgets.h"
#include <QtCore/QSignalBlocker>
#include <QtGui/QFontMetrics>
#include <QtWidgets/QScrollBar>
#include "Globals.h"
#include "HighDPISupport.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "PatternClipboard.h"
#include "Reporting.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../soundlib/mod_specifications.h"
#include "../common/mptStringBuffer.h"


OPENMPT_NAMESPACE_BEGIN

enum SequenceAction : SEQUENCEINDEX
{
	kAddSequence = MAX_SEQUENCES,
	kDuplicateSequence,
	kDeleteSequence,
	kSplitSequence,

	kMaxSequenceActions
};

static bool IsSelectionKeyPressed() { return CMainFrame::GetInputHandler()->SelectionPressed(); }


//////////////////////////////////////////////////////////////
// COrderList

void COrderList::RegisterMessageMap()
{
	mfcless::Window<QAbstractScrollArea>::RegisterMessageMap();
	// ON_WM_PAINT() (mfcless::Window)
	// ON_WM_ERASEBKGND(): by hand (Qt event override)
	// ON_WM_MOUSEMOVE() (mfcless::Window)
	// ON_WM_LBUTTONDOWN() (mfcless::Window)
	// ON_WM_LBUTTONDBLCLK() (mfcless::Window)
	// ON_WM_LBUTTONUP() (mfcless::Window)
	// ON_WM_RBUTTONUP() (mfcless::Window)
	// ON_WM_MBUTTONDOWN() (mfcless::Window)
	// ON_WM_SETFOCUS() (mfcless::Window)
	// ON_WM_KILLFOCUS() (mfcless::Window)
	AddScroll(Qt::Horizontal, &COrderList::OnHScroll);
	// ON_WM_SIZE() (mfcless::Window)
	AddCommand(ID_ORDERLIST_INSERT, &COrderList::OnInsertOrder);
	AddCommand(ID_ORDERLIST_INSERT_SEPARATOR, &COrderList::OnInsertSeparatorPattern);
	AddCommand(ID_ORDERLIST_DELETE, &COrderList::OnDeleteOrder);
	AddCommand(ID_ORDERLIST_RENDER, &COrderList::OnRenderOrder);
	AddCommand(ID_ORDERLIST_EDIT_COPY, &COrderList::OnEditCopy);
	AddCommand(ID_ORDERLIST_EDIT_CUT, &COrderList::OnEditCut);
	AddCommand(ID_ORDERLIST_EDIT_COPY_ORDERS, &COrderList::OnEditCopyOrders);
	AddCommand(ID_PATTERN_PROPERTIES, &COrderList::OnPatternProperties);
	AddCommand(ID_PLAYER_PLAY, &COrderList::OnPlayerPlay);
	AddCommand(ID_PLAYER_PAUSE, &COrderList::OnPlayerPause);
	AddCommand(ID_PLAYER_PLAYFROMSTART, &COrderList::OnPlayerPlayFromStart);
	AddCommand(IDC_PATTERN_PLAYFROMSTART, &COrderList::OnPatternPlayFromStart);
	AddCommand(ID_ORDERLIST_NEW, &COrderList::OnCreateNewPattern);
	AddCommand(ID_ORDERLIST_COPY, &COrderList::OnDuplicatePattern);
	AddCommand(ID_ORDERLIST_MERGE, &COrderList::OnMergePatterns);
	AddCommand(ID_PATTERNCOPY, &COrderList::OnPatternCopy);
	AddCommand(ID_PATTERNPASTE, &COrderList::OnPatternPaste);
	AddCommand(ID_SETRESTARTPOS, &COrderList::OnSetRestartPos);
	AddCommand(ID_ORDERLIST_LOCKPLAYBACK, &COrderList::OnLockPlayback);
	AddCommand(ID_ORDERLIST_UNLOCKPLAYBACK, &COrderList::OnUnlockPlayback);
	AddCommand(ID_QUEUE_AT_PATTERN_END, &COrderList::OnQueueAtPatternEnd);
	AddCommand(ID_QUEUE_AT_MEASURE_END, &COrderList::OnQueueAtMeasureEnd);
	AddCommand(ID_QUEUE_AT_BEAT_END, &COrderList::OnQueueAtBeatEnd);
	AddCommand(ID_QUEUE_AT_ROW_END, &COrderList::OnQueueAtRowEnd);
	AddMessage(WM_MOD_DRAGONDROPPING, &COrderList::OnDragonDropping);
	AddMessage(WM_MOD_KEYCOMMAND, &COrderList::OnCustomKeyMsg);
	// WM_DPICHANGED_AFTERPARENT: CCtrlPatterns::OnDPIChanged calls OnDPIChanged.
	// TTN_NEEDTEXT: GetToolTipText.
	AddCommandRange(ID_SEQUENCE_ITEM, ID_SEQUENCE_ITEM + kMaxSequenceActions - 1, &COrderList::OnSelectSequence);
}


COrderList::COrderList(CCtrlPatterns &parent, CModDoc &document)
	: m_modDoc(document)
	, m_pParent(parent)
	, m_nOrderlistMargins(TrackerSettings::Instance().orderlistMargins)
{
	EnableActiveAccessibility();
}


bool COrderList::EnsureEditable(ORDERINDEX ord)
{
	auto &sndFile = m_modDoc.GetSoundFile();
	if(ord >= Order().size())
	{
		if(ord < sndFile.GetModSpecifications().ordersMax)
		{
			try
			{
				Order().resize(ord + 1);
			} catch(mpt::out_of_memory e)
			{
				mpt::delete_out_of_memory(e);
				return false;
			}
		} else
		{
			return false;
		}
	}
	return true;
}


ModSequence &COrderList::Order() { return m_modDoc.GetSoundFile().Order(); }
const ModSequence &COrderList::Order() const { return m_modDoc.GetSoundFile().Order(); }


void COrderList::SetScrollPos(int pos)
{
	QSignalBlocker block(horizontalScrollBar());
	horizontalScrollBar()->setValue(pos);
}


int COrderList::GetScrollPos(bool getTrackPos)
{
	return getTrackPos ? horizontalScrollBar()->sliderPosition() : horizontalScrollBar()->value();
}


// The scroll range as Windows has it: the last position is max + page - 1.
void COrderList::GetScrollRange(int &smin, int &smax)
{
	smin = horizontalScrollBar()->minimum();
	smax = horizontalScrollBar()->maximum() + std::max(horizontalScrollBar()->pageStep() - 1, 0);
}


bool COrderList::IsOrderInMargins(int order, int startOrder)
{
	const ORDERINDEX nMargins = GetMargins();
	return ((startOrder != 0 && order - startOrder < nMargins) ||
		order - startOrder >= GetLength() - nMargins);
}


void COrderList::EnsureVisible(ORDERINDEX order)
{
	// nothing needs to be done
	if(!IsOrderInMargins(order, m_nXScroll) || order == ORDERINDEX_INVALID)
		return;

	if(order < m_nXScroll)
	{
		if(order < GetMargins())
			m_nXScroll = 0;
		else
			m_nXScroll = order - GetMargins();
	} else
	{
		m_nXScroll = order + 2 * GetMargins() - 1;
		if(m_nXScroll < GetLength())
			m_nXScroll = 0;
		else
			m_nXScroll -= GetLength();
	}
}


bool COrderList::IsPlaying() const
{
	return (CMainFrame::GetMainFrame()->GetModPlaying() == &m_modDoc);
}


ORDERINDEX COrderList::GetOrderFromPoint(const mfcless::Point &pt) const
{
	if(m_cxFont)
		return mpt::saturate_cast<ORDERINDEX>(m_nXScroll + pt.x / m_cxFont);
	return 0;
}


mfcless::Rect COrderList::GetRectFromOrder(ORDERINDEX ord) const
{
	return mfcless::Rect{mfcless::Point{(ord - m_nXScroll) * m_cxFont, 0}, mfcless::Size{m_cxFont, m_cyFont}};
}


bool COrderList::Init(const mfcless::Rect &rect)
{
	// A child window with a thin sunken edge (WS_EX_STATICEDGE) and a
	// horizontal scroll bar.
	setParent(&m_pParent);
	setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	setFocusPolicy(Qt::StrongFocus);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	horizontalScrollBar()->setSingleStep(1);
	mfcless::ConnectNotifications(horizontalScrollBar(), IDC_ORDERLIST, this);
	setGeometry(QRect(rect));
	m_hFont = CMainFrame::GetGUIFont();
	setFont(m_hFont);
	m_pParent.AddDynamicItem(IDC_ORDERLIST, this);
	SetScrollPos(0);
	SetCurSel(0);
	show();
	return true;
}


void COrderList::UpdateScrollInfo()
{
	mfcless::Rect rcClient;

	GetClientRect(&rcClient);
	if((m_cxFont > 0) && (rcClient.right > 0))
	{
		int nMax = Order().GetLengthTailTrimmed() + std::max(ORDERINDEX(1), GetMargins()) - 1;

		const int nPage = rcClient.right / m_cxFont;
		if(nMax <= nPage)
			nMax = nPage + 1;
		// Windows' range is 0..nMax with a page of nPage; Qt's maximum is the
		// last position the page can start at.
		QScrollBar *bar = horizontalScrollBar();
		const int qtMax = std::max(nMax - std::max(nPage - 1, 0), 0);
		if(bar->maximum() != qtMax || bar->pageStep() != nPage)
		{
			QSignalBlocker block(bar);
			bar->setPageStep(nPage);
			bar->setRange(0, qtMax);
		}
	}
}


int COrderList::GetFontWidth()
{
	if(m_cxFont <= 0)
	{
		return QFontMetrics(m_hFont).horizontalAdvance(QStringLiteral("000+"));
	}
	return m_cxFont;
}


void COrderList::InvalidateSelection()
{
	ORDERINDEX ordLo = m_nScrollPos, count = 1;
	static ORDERINDEX m_nScrollPos2Old = m_nScrollPos2nd;
	if(m_nScrollPos2Old != ORDERINDEX_INVALID)
	{
		// there were multiple orders selected - remove them all
		ORDERINDEX ordHi = m_nScrollPos;
		if(m_nScrollPos2Old < m_nScrollPos)
			ordLo = m_nScrollPos2Old;
		else
			ordHi = m_nScrollPos2Old;
		count = ordHi - ordLo + 1;
	}
	m_nScrollPos2Old = m_nScrollPos2nd;
	mfcless::Rect rcClient, rect;
	GetClientRect(&rcClient);
	rect.left = rcClient.left + (ordLo - m_nXScroll) * m_cxFont;
	rect.top = rcClient.top;
	rect.right = rect.left + m_cxFont * count;
	rect.bottom = rcClient.bottom;
	rect &= rcClient;
	if(rect.right > rect.left)
		InvalidateRect(rect, false);
	if(m_playPos != ORDERINDEX_INVALID)
	{
		rect.left = rcClient.left + (m_playPos - m_nXScroll) * m_cxFont;
		rect.top = rcClient.top;
		rect.right = rect.left + m_cxFont;
		rect &= rcClient;
		if(rect.right > rect.left)
			InvalidateRect(rect, false);
		m_playPos = ORDERINDEX_INVALID;
	}
}


ORDERINDEX COrderList::GetLength()
{
	mfcless::Rect rcClient;
	GetClientRect(&rcClient);
	if(m_cxFont > 0)
		return mpt::saturate_cast<ORDERINDEX>(rcClient.right / m_cxFont);
	else
	{
		const int fontWidth = GetFontWidth();
		return (fontWidth > 0) ? mpt::saturate_cast<ORDERINDEX>(rcClient.right / fontWidth) : 0;
	}
}


OrdSelection COrderList::GetCurSel(bool ignoreSelection) const
{
	// returns the currently selected order(s)
	OrdSelection result;
	result.firstOrd = result.lastOrd = m_nScrollPos;
	// ignoreSelection: true if only first selection marker is important.
	if(!ignoreSelection && m_nScrollPos2nd != ORDERINDEX_INVALID)
	{
		if(m_nScrollPos2nd < m_nScrollPos)  // ord2 < ord1
			result.firstOrd = m_nScrollPos2nd;
		else
			result.lastOrd = m_nScrollPos2nd;
	}
	ORDERINDEX lastIndex = std::max(Order().GetLengthTailTrimmed(), m_modDoc.GetSoundFile().GetModSpecifications().ordersMax) - 1u;
	LimitMax(result.firstOrd, lastIndex);
	LimitMax(result.lastOrd, lastIndex);
	return result;
}


void COrderList::SetSelection(ORDERINDEX firstOrd, ORDERINDEX lastOrd)
{
	SetCurSel(firstOrd, true, false, true);
	SetCurSel(lastOrd != ORDERINDEX_INVALID ? lastOrd : firstOrd, false, true, true);
}


bool COrderList::SetCurSel(ORDERINDEX sel, bool setPlayPos, bool shiftClick, bool ignoreCurSel)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	CSoundFile &sndFile = m_modDoc.GetSoundFile();
	ORDERINDEX &ord = shiftClick ? m_nScrollPos2nd : m_nScrollPos;
	const ORDERINDEX lastIndex = std::max(Order().GetLength(), sndFile.GetModSpecifications().ordersMax) - 1u;

	if((sel < 0) || (sel > lastIndex) || (!pMainFrm))
		return false;
	if(!ignoreCurSel && sel == ord && (sel == sndFile.m_PlayState.m_nCurrentOrder))
		return true;
	const ORDERINDEX shownLength = GetLength();
	InvalidateSelection();
	ord = sel;
	if(!EnsureEditable(ord))
		return false;

	if(!m_bScrolling)
	{
		const ORDERINDEX margins = GetMargins(GetMarginsMax(shownLength));
		if(ord < m_nXScroll + margins)
		{
			// Must move first shown sequence item to left in order to show the new active order.
			m_nXScroll = (ord > margins) ? (ord - margins) : 0;
			SetScrollPos(m_nXScroll);
			Invalidate(false);
		} else
		{
			ORDERINDEX maxsel = shownLength;
			if(maxsel)
				maxsel--;
			if(ord - m_nXScroll >= maxsel - margins)
			{
				// Must move first shown sequence item to right in order to show the new active order.
				m_nXScroll = ord - (maxsel - margins);
				SetScrollPos(m_nXScroll);
				Invalidate(false);
			}
		}
	}
	InvalidateSelection();
	PATTERNINDEX n = Order()[m_nScrollPos];
	if(setPlayPos && !shiftClick && sndFile.Patterns.IsValidPat(n))
	{
		const bool isPlaying = IsPlaying();
		bool changedPos = false;

		if(isPlaying && sndFile.m_PlayState.m_flags[SONG_PATTERNLOOP])
		{
			pMainFrm->ResetNotificationBuffer();

			// Update channel parameters and play time
			CriticalSection cs;
			m_modDoc.SetElapsedTime(m_nScrollPos, 0, !sndFile.m_PlayState.m_flags[SONG_PAUSED | SONG_STEP]);

			changedPos = true;
		} else if(m_pParent.GetFollowSong())
		{
			FlagSet<PlayFlags> pausedFlags = sndFile.m_PlayState.m_flags & (SONG_PAUSED | SONG_STEP | SONG_PATTERNLOOP);
			// Update channel parameters and play time
			CriticalSection cs;
			sndFile.SetCurrentOrder(m_nScrollPos);
			m_modDoc.SetElapsedTime(m_nScrollPos, 0, !sndFile.m_PlayState.m_flags[SONG_PAUSED | SONG_STEP]);
			sndFile.m_PlayState.m_flags.set(pausedFlags);

			if(isPlaying)
				pMainFrm->ResetNotificationBuffer();
			changedPos = true;
		}

		if(changedPos && Order().IsPositionLocked(m_nScrollPos))
		{
			// Users wants to go somewhere else, so let them do that.
			OnUnlockPlayback();
		}

		m_pParent.SetCurrentPattern(n);
	} else if(setPlayPos && !shiftClick && n != PATTERNINDEX_SKIP && n != PATTERNINDEX_INVALID)
	{
		m_pParent.SetCurrentPattern(n);
	}
	UpdateInfoText();
	if(m_nScrollPos == m_nScrollPos2nd)
		m_nScrollPos2nd = ORDERINDEX_INVALID;
	return true;
}


PATTERNINDEX COrderList::GetCurrentPattern() const
{
	const ModSequence &order = Order();
	if(m_nScrollPos < order.size())
	{
		return order[m_nScrollPos];
	}
	return 0;
}


bool COrderList::PreTranslateMessage(mfcless::Message *pMsg)
{
	//handle Patterns View context keys that we want to take effect in the orderlist.
	if((pMsg->message == mfcless::msg::SysKeyUp)   || (pMsg->message == mfcless::msg::KeyUp) || 
	   (pMsg->message == mfcless::msg::SysKeyDown) || (pMsg->message == mfcless::msg::KeyDown))
	{
		CInputHandler *ih = CMainFrame::GetInputHandler();
		const auto event = ih->Translate(*pMsg);

		if(ih->KeyEvent(kCtxCtrlOrderlist, event, this) != kcNull)
			return true;  // Mapped to a command, no need to pass message on.

		//HACK: masquerade as kCtxViewPatternsNote context until we implement appropriate
		//      command propagation to kCtxCtrlOrderlist context.

		if(ih->KeyEvent(kCtxViewPatternsNote, event, this) != kcNull)
			return true;  // Mapped to a command, no need to pass message on.

		if(ih->KeyEvent(kCtxAllContexts, event, this) != kcNull)
			return true;  // Mapped to a command, no need to pass message on.
	}

	return WindowBase::PreTranslateMessage(pMsg);
}


intptr_t COrderList::OnCustomKeyMsg(uintptr_t wParam, intptr_t lParam)
{
	const bool isPlaying = IsPlaying();
	switch(wParam)
	{
	case kcContextMenu:
		{
			const auto selection = GetCurSel();
			auto pt = (GetRectFromOrder(selection.firstOrd) | GetRectFromOrder(selection.lastOrd)).CenterPoint();
			mfcless::Rect clientRect;
			GetClientRect(clientRect);
			if(!clientRect.PtInRect(pt))
				pt = clientRect.CenterPoint();
			OnRButtonUp(0, pt);
		}
		return wParam;

	case kcEditCopy:
		OnEditCopy(); return wParam;
	case kcEditCut:
		OnEditCut(); return wParam;
	case kcEditPaste:
		OnPatternPaste(); return wParam;
	case kcOrderlistEditCopyOrders:
		OnEditCopyOrders(); return wParam;

	// Orderlist navigation
	case kcOrderlistNavigateLeftSelect:
	case kcOrderlistNavigateLeft:
		SetCurSelTo2ndSel(wParam == kcOrderlistNavigateLeftSelect); SetCurSel(m_nScrollPos - 1, wParam == kcOrderlistNavigateLeft || !isPlaying); return wParam;
	case kcOrderlistNavigateRightSelect:
	case kcOrderlistNavigateRight:
		SetCurSelTo2ndSel(wParam == kcOrderlistNavigateRightSelect); SetCurSel(m_nScrollPos + 1, wParam == kcOrderlistNavigateRight || !isPlaying); return wParam;
	case kcOrderlistNavigateFirstSelect:
	case kcOrderlistNavigateFirst:
		SetCurSelTo2ndSel(wParam == kcOrderlistNavigateFirstSelect); SetCurSel(0, wParam == kcOrderlistNavigateFirst || !isPlaying); return wParam;
	case kcEditSelectAll:
		SetCurSel(0, !isPlaying);
		[[fallthrough]];
	case kcOrderlistNavigateLastSelect:
	case kcOrderlistNavigateLast:
		{
			SetCurSelTo2ndSel(wParam == kcOrderlistNavigateLastSelect || wParam == kcEditSelectAll);
			ORDERINDEX nLast = Order().GetLengthTailTrimmed();
			if(nLast > 0) nLast--;
			SetCurSel(nLast, wParam == kcOrderlistNavigateLast || !isPlaying);
		}
		return wParam;

	// Orderlist edit
	case kcOrderlistEditDelete:
		OnDeleteOrder(); return wParam;
	case kcOrderlistEditInsert:
		OnInsertOrder(); return wParam;
	case kcOrderlistEditInsertSeparator:
		OnInsertSeparatorPattern(); return wParam;
	case kcOrderlistSwitchToPatternView:
		OnSwitchToView(); return wParam;
	case kcOrderlistEditPattern:
		OnLButtonDblClk(0, mfcless::Point(0, 0)); OnSwitchToView(); return wParam;

	// Enter pattern number
	case kcOrderlistPat0:
	case kcOrderlistPat1:
	case kcOrderlistPat2:
	case kcOrderlistPat3:
	case kcOrderlistPat4:
	case kcOrderlistPat5:
	case kcOrderlistPat6:
	case kcOrderlistPat7:
	case kcOrderlistPat8:
	case kcOrderlistPat9:
		EnterPatternNum(static_cast<uint32>(wParam) - kcOrderlistPat0); return wParam;
	case kcOrderlistPatMinus:
		EnterPatternNum(10); return wParam;
	case kcOrderlistPatPlus:
		EnterPatternNum(11); return wParam;
	case kcOrderlistPatIgnore:
		EnterPatternNum(12); return wParam;
	case kcOrderlistPatInvalid:
		EnterPatternNum(13); return wParam;

	// kCtxViewPatternsNote messages
	case kcSwitchToOrderList:
		OnSwitchToView();
		return wParam;
	case kcChangeLoopStatus:
		m_pParent.OnModCtrlMsg(CTRLMSG_PAT_LOOP, -1); return wParam;
	case kcToggleFollowSong:
		m_pParent.OnModCtrlMsg(CTRLMSG_PAT_FOLLOWSONG, 1); return wParam;

	case kcChannelUnmuteAll:
	case kcUnmuteAllChnOnPatTransition:
		return m_pParent.SendMessage(WM_MOD_KEYCOMMAND, wParam, lParam);

	case kcOrderlistLockPlayback:
		OnLockPlayback(); return wParam;
	case kcOrderlistUnlockPlayback:
		OnUnlockPlayback(); return wParam;

	case kcOrderlistQueueAtPatternEnd:
		QueuePattern(GetCurSel(true).firstOrd, OrderTransitionMode::AtPatternEnd); return wParam;
	case kcOrderlistQueueAtMeasureEnd:
		QueuePattern(GetCurSel(true).firstOrd, OrderTransitionMode::AtMeasureEnd); return wParam;
	case kcOrderlistQueueAtBeatEnd:
		QueuePattern(GetCurSel(true).firstOrd, OrderTransitionMode::AtBeatEnd); return wParam;
	case kcOrderlistQueueAtRowEnd:
		QueuePattern(GetCurSel(true).firstOrd, OrderTransitionMode::AtRowEnd); return wParam;

	case kcDuplicatePattern:
		OnDuplicatePattern(); return wParam;
	case kcMergePatterns:
		OnMergePatterns(); return wParam;
	case kcNewPattern:
		OnCreateNewPattern(); return wParam;

	case kcOrderlistStreamExport:
		OnRenderOrder(); return wParam;
	}

	return kcNull;
}


// Helper function to enter pattern index into the orderlist.
// Call with param 0...9 (enter digit), 10 (decrease) or 11 (increase).
void COrderList::EnterPatternNum(int enterNum)
{
	CSoundFile &sndFile = m_modDoc.GetSoundFile();

	if(!EnsureEditable(m_nScrollPos))
		return;

	PATTERNINDEX curIndex = Order()[m_nScrollPos];
	const PATTERNINDEX maxIndex = std::max(PATTERNINDEX(1), sndFile.Patterns.GetNumPatterns()) - 1;
	const PATTERNINDEX firstInvalid = sndFile.GetModSpecifications().hasIgnoreIndex ? PATTERNINDEX_SKIP : PATTERNINDEX_INVALID;

	if(enterNum >= 0 && enterNum <= 9)  // enter 0...9
	{
		if(curIndex >= sndFile.Patterns.Size())
			curIndex = 0;

		curIndex = curIndex * 10 + static_cast<PATTERNINDEX>(enterNum);
		static_assert(MAX_PATTERNS < 10000);
		if((curIndex >= 1000) && (curIndex > maxIndex)) curIndex %= 1000;
		if((curIndex >= 100) && (curIndex > maxIndex)) curIndex %= 100;
		if((curIndex >= 10) && (curIndex > maxIndex)) curIndex %= 10;
	} else if(enterNum == 10) // decrease pattern index
	{
		if(curIndex == 0)
		{
			curIndex = PATTERNINDEX_INVALID;
		} else if(curIndex > maxIndex && curIndex <= firstInvalid)
		{
			curIndex = maxIndex;
		} else
		{
			do
			{
				curIndex--;
			} while(curIndex > 0 && curIndex < firstInvalid && !sndFile.Patterns.IsValidPat(curIndex));
		}
	} else if(enterNum == 11)  // increase pattern index
	{
		if(curIndex >= PATTERNINDEX_INVALID)
		{
			curIndex = 0;
		} else if(curIndex >= maxIndex && curIndex < firstInvalid)
		{
			curIndex = firstInvalid;
		} else
		{
			do
			{
				curIndex++;
			} while(curIndex <= maxIndex && !sndFile.Patterns.IsValidPat(curIndex));
		}
	} else if(enterNum == 12)  // ignore index (+++)
	{
		if(sndFile.GetModSpecifications().hasIgnoreIndex)
			curIndex = PATTERNINDEX_SKIP;
	} else if(enterNum == 13)  // invalid index (---)
	{
		curIndex = PATTERNINDEX_INVALID;
	}
	// apply
	if(curIndex != Order()[m_nScrollPos])
	{
		Order()[m_nScrollPos] = curIndex;
		m_modDoc.SetModified();
		m_modDoc.UpdateAllViews(nullptr, SequenceHint().Data(), this);
		InvalidateSelection();
		m_pParent.SetCurrentPattern(curIndex);
	}
}


void COrderList::OnEditCut()
{
	OnEditCopy();
	OnDeleteOrder();
}


void COrderList::OnCopy(bool onlyOrders)
{
	const OrdSelection ordsel = GetCurSel();
	mfcless::BeginWaitCursor();
	PatternClipboard::Copy(m_modDoc.GetSoundFile(), ordsel.firstOrd, ordsel.lastOrd, onlyOrders);
	PatternClipboardDialog::UpdateList();
	mfcless::EndWaitCursor();
}


void COrderList::UpdateView(UpdateHint hint, QObject *pObj)
{
	if(pObj != this && hint.ToType<SequenceHint>().GetType()[HINT_MODTYPE | HINT_MODSEQUENCE])
	{
		Invalidate(false);
		UpdateInfoText();
	}
	if(hint.GetType()[HINT_MPTOPTIONS])
	{
		m_nOrderlistMargins = TrackerSettings::Instance().orderlistMargins;
	}
}


void COrderList::OnSwitchToView()
{
	m_pParent.PostViewMessage(VIEWMSG_SETFOCUS);
}


void COrderList::UpdateInfoText()
{
	if(mfcless::GetFocus() != this)
		return;

	CSoundFile &sndFile = m_modDoc.GetSoundFile();
	const auto &order = Order();

	const ORDERINDEX length = order.GetLengthTailTrimmed();
	mpt::ustring s;
	if(TrackerSettings::Instance().patternSetup & PatternSetup::RowAndOrderNumbersHex)
		s = mfcless::PrintF(UL_("Position %02Xh of %02Xh"), m_nScrollPos, length);
	else
		s = mfcless::PrintF(UL_("Position %u of %u (%02Xh of %02Xh)"), m_nScrollPos, length, m_nScrollPos, length);

	if(order.IsValidPat(m_nScrollPos))
	{
		if(const auto patName = sndFile.Patterns[order[m_nScrollPos]].GetName(); !patName.empty())
			s += UL_(": ") + mpt::ToUnicode(sndFile.GetCharsetInternal(), patName);
	}
	CMainFrame::GetMainFrame()->SetInfoText(s);

	CMainFrame::GetMainFrame()->NotifyAccessibilityUpdate(*this);
}


// Accessible description for screen readers
QString COrderList::GetAccessibleName()
{
	CSoundFile &sndFile = m_modDoc.GetSoundFile();
	const auto &order = Order();

	mpt::ustring s;
	const bool singleSel = m_nScrollPos2nd == ORDERINDEX_INVALID || m_nScrollPos2nd == m_nScrollPos;
	const auto firstOrd = singleSel ? m_nScrollPos : std::min(m_nScrollPos, m_nScrollPos2nd), lastOrd = singleSel ? m_nScrollPos : std::max(m_nScrollPos, m_nScrollPos2nd);
	if(singleSel)
		s = MPT_UFORMAT("Order {}, ")(FormatOrderRow(m_nScrollPos));
	else
		s = MPT_UFORMAT("Order selection {} to {}: ")(FormatOrderRow(firstOrd), FormatOrderRow(lastOrd));
	bool first = true;
	for(ORDERINDEX o = firstOrd; o <= lastOrd; o++)
	{
		if(!first)
			s += UL_(", ");
		first = false;
		PATTERNINDEX pat = order[o];
		if(pat == PATTERNINDEX_SKIP)
			s += UL_(" Skip");
		else if(pat == PATTERNINDEX_INVALID)
			s += UL_(" Stop");
		else
			s += MPT_UFORMAT("Pattern {}")(pat);
		if(sndFile.Patterns.IsValidPat(pat))
		{
			if(const auto patName = sndFile.Patterns[pat].GetName(); !patName.empty())
				s += UL_(" (") + mpt::ToUnicode(sndFile.GetCharsetInternal(), patName) + UL_(")");
		}
	}
	
	return mfcless::Text(s).value;
}


/////////////////////////////////////////////////////////////////
// COrderList messages

void COrderList::OnDPIChanged()
{
	m_cxFont = m_cyFont = 0;
	m_hFont = CMainFrame::GetGUIFont();
	setFont(m_hFont);
	Invalidate(false);
}


void COrderList::OnPaint()
{
	mpt::ustring s;
	mfcless::PaintDC dc(this);
	dc.SelectObject(m_hFont);
	const auto separatorColor = mfcless::SystemColor(mfcless::SysColor::Window) ^ 0x808080;
	const auto colorText = mfcless::SystemColor(mfcless::SysColor::WindowText), colorInvalid = mfcless::SystemColor(mfcless::SysColor::GrayText), colorTextSel = mfcless::SystemColor(mfcless::SysColor::HighlightText);
	const auto windowColor = mfcless::SystemColor(mfcless::SysColor::Window), highlightColor = mfcless::SystemColor(mfcless::SysColor::Highlight), faceColor = mfcless::SystemColor(mfcless::SysColor::ButtonFace);

	dc.SelectObject(mfcless::Pen(mfcless::pen::Solid, 1, separatorColor));

	// First time?
	if(m_cxFont <= 0 || m_cyFont <= 0)
	{
		mfcless::Size sz = dc.GetTextExtent(UL_("000+"));
		m_cxFont = sz.cx;
		m_cyFont = sz.cy;
	}

	if(m_cxFont > 0 && m_cyFont > 0)
	{
		mfcless::Rect rcClient;
		GetClientRect(&rcClient);
		mfcless::Rect rect = rcClient;

		UpdateScrollInfo();
		dc.SetBkMode(mfcless::bk::Transparent);
		const OrdSelection selection = GetCurSel();

		const int lineWidth1 = HighDPISupport::ScalePixels(1, this);
		const int lineWidth2 = HighDPISupport::ScalePixels(2, this);
		const bool isFocussed = (mfcless::GetFocus() == this);

		const auto &order = Order();
		CSoundFile &sndFile = m_modDoc.GetSoundFile();
		ORDERINDEX maxEntries = sndFile.GetModSpecifications().ordersMax;
		if(order.size() > maxEntries)
		{
			// Only computed if potentially needed.
			maxEntries = std::max(maxEntries, order.GetLengthTailTrimmed());
		}

		// Scrolling the shown orders(the showns rectangles)?
		for(size_t pos = m_nXScroll; rect.left < rcClient.right; pos++, rect.left += m_cxFont)
		{
			const ORDERINDEX ord = mpt::saturate_cast<ORDERINDEX>(pos);
			dc.SetTextColor(colorText);
			const bool inSelection = (ord >= selection.firstOrd && ord <= selection.lastOrd);
			const bool highLight = (isFocussed && inSelection);
			if((rect.right = rect.left + m_cxFont) > rcClient.right)
				rect.right = rcClient.right;
			rect.right--;

			uint32 background = windowColor;  // Normal, unselected item.
			if(highLight)
				background = highlightColor;  // Currently selected order item
			else if(order.IsPositionLocked(ord))
				background = faceColor;  // "Playback lock" indicator - grey out all order items which aren't played.
			dc.FillSolidRect(rect, background);

			// Drawing the shown pattern-indicator or drag position.
			if(ord == (m_bDragging ? m_nDropPos : m_nScrollPos))
			{
				dc.SetBkColor(mfcless::Rgb(0, 0, 0));
				dc.SetTextColor(mfcless::Rgb(255, 255, 255));
				rect.InflateRect(-1, -1);
				dc.DrawFocusRect(&rect);
				rect.InflateRect(1, 1);
			}
			dc.MoveTo(rect.right, rect.top);
			dc.LineTo(rect.right, rect.bottom);

			// Drawing the 'ctrl-transition' indicator
			if(ord == sndFile.m_PlayState.m_nSeqOverride && sndFile.m_PlayState.m_nSeqOverride != ORDERINDEX_INVALID)
			{
				dc.FillSolidRect(mfcless::Rect{rect.left + 4, rect.bottom - 4 - lineWidth1, rect.right - 4, rect.bottom - 4}, separatorColor);
			}

			// Drawing 'playing'-indicator.
			if(ord == sndFile.GetCurrentOrder() && CMainFrame::GetMainFrame()->IsPlaying())
			{
				dc.FillSolidRect(mfcless::Rect{rect.left + 4, rect.top + 2, rect.right - 4, rect.top + 2 + lineWidth1}, separatorColor);
				m_playPos = ord;
			}

			// Drawing drop indicator
			if(m_bDragging && ord == m_nDropPos && !inSelection)
			{
				const bool dropLeft = (m_nDropPos < selection.firstOrd) || TrackerSettings::Instance().orderListOldDropBehaviour;
				dc.FillSolidRect(mfcless::Rect{dropLeft ? (rect.left + 2) : (rect.right - 2 - lineWidth2), rect.top + 2, dropLeft ? (rect.left + 2 + lineWidth2) : (rect.right - 2), rect.bottom - 2}, separatorColor);
			}

			s.clear();
			const PATTERNINDEX pat = (ord < order.size()) ? order[ord] : PATTERNINDEX_INVALID;
			if(ord < maxEntries && (rect.left + m_cxFont - 4) <= rcClient.right)
			{
				if(pat == PATTERNINDEX_INVALID)
					s = UL_("---");
				else if(pat == PATTERNINDEX_SKIP)
					s = UL_("+++");
				else
					s = mfcless::PrintF(UL_("%u"), pat);
			}

			uint32 textCol;
			if(highLight)
				textCol = colorTextSel;  // Highlighted pattern
			else if(sndFile.Patterns.IsValidPat(pat))
				textCol = colorText;  // Normal pattern
			else
				textCol = colorInvalid;  // Non-existent pattern
			dc.SetTextColor(textCol);
			dc.DrawText(s, -1, &rect, mfcless::dt::SingleLine | mfcless::dt::Center | mfcless::dt::VCenter);
		}
	}
}


void COrderList::OnSetFocus(QWidget *pWnd)
{
	Window::OnSetFocus(pWnd);
	InvalidateSelection();
	UpdateInfoText();
}


void COrderList::OnKillFocus(QWidget *pWnd)
{
	Window::OnKillFocus(pWnd);
	InvalidateSelection();
}


void COrderList::OnLButtonDown(uint32 nFlags, mfcless::Point pt)
{
	mfcless::Rect rect;
	GetClientRect(&rect);
	if(pt.y < rect.bottom)
	{
		setFocus();

		const ORDERINDEX ord = GetOrderFromPoint(pt);
		if((nFlags & mfcless::MouseKey::Control) != 0)
		{
			// Queue pattern
			QueuePattern(ord, CMainFrame::GetInputHandler()->ModifierKeysToTransitionMode());
		} else
		{
			// mark pattern (+skip to)
			const int oldXScroll = m_nXScroll;

			OrdSelection selection = GetCurSel();

			// check if cursor is in selection - if it is, only react on MouseUp as the user might want to drag those orders
			if(m_nScrollPos2nd == ORDERINDEX_INVALID || ord < selection.firstOrd || ord > selection.lastOrd)
			{
				m_nScrollPos2nd = ORDERINDEX_INVALID;
				SetCurSel(ord, true, IsSelectionKeyPressed());
			}
			m_bDragging = !IsOrderInMargins(m_nScrollPos, oldXScroll) || !IsOrderInMargins(m_nScrollPos2nd, oldXScroll);

			m_nMouseDownPos = ord;
			if(m_bDragging)
			{
				m_nDragOrder = m_nDropPos = GetCurSel(true).firstOrd;
				SetCapture();
			}
		}
	} else
	{
		Window::OnLButtonDown(nFlags, pt);
	}
}


void COrderList::OnLButtonUp(uint32 nFlags, mfcless::Point pt)
{
	mfcless::Rect rect;
	GetClientRect(&rect);

	// Copy or move orders?
	const bool copyOrders = IsSelectionKeyPressed();

	if(m_bDragging)
	{
		m_bDragging = false;
		ReleaseCapture();
		if(rect.PtInRect(pt))
		{
			ORDERINDEX n = GetOrderFromPoint(pt);
			const OrdSelection selection = GetCurSel();
			if(n != ORDERINDEX_INVALID && n == m_nDropPos && (n < selection.firstOrd || n > selection.lastOrd))
			{
				const bool multiSelection = (selection.firstOrd != selection.lastOrd);
				const bool moveBack = m_nDropPos < m_nDragOrder;
				ORDERINDEX moveCount = (selection.lastOrd - selection.firstOrd), movePos = selection.firstOrd;

				if(!moveBack && !TrackerSettings::Instance().orderListOldDropBehaviour)
					m_nDropPos++;

				bool modified = false;
				for(int i = 0; i <= moveCount; i++)
				{
					if(!m_modDoc.MoveOrder(movePos, m_nDropPos, true, copyOrders))
						break;
					modified = true;
					if(moveBack != copyOrders && multiSelection)
					{
						movePos++;
						m_nDropPos++;
					}
					if(moveBack && copyOrders && multiSelection)
					{
						movePos += 2;
						m_nDropPos++;
					}
				}

				if(multiSelection)
				{
					// adjust selection
					m_nScrollPos2nd = m_nDropPos - 1;
					m_nDropPos -= moveCount + (moveBack ? 0 : 1);
					SetCurSel((moveBack && !copyOrders) ? m_nDropPos - 1 : m_nDropPos);
				} else
				{
					SetCurSel((m_nDragOrder < m_nDropPos && !copyOrders) ? m_nDropPos - 1 : m_nDropPos);
				}
				// Did we actually change anything?
				if(modified)
					m_modDoc.SetModified();
			} else
			{
				if(pt.y < rect.bottom && n == m_nMouseDownPos && !copyOrders)
				{
					// Remove selection if we didn't drag anything but multiselect was active
					m_nScrollPos2nd = ORDERINDEX_INVALID;
					setFocus();
					SetCurSel(n);
				}
			}
		}
		Invalidate(false);
	} else
	{
		Window::OnLButtonUp(nFlags, pt);
	}
}


void COrderList::OnMouseMove(uint32 nFlags, mfcless::Point pt)
{
	if((m_bDragging) && (m_cxFont))
	{
		mfcless::Rect rect;

		GetClientRect(&rect);
		ORDERINDEX n = ORDERINDEX_INVALID;
		if(rect.PtInRect(pt))
		{
			CSoundFile &sndFile = m_modDoc.GetSoundFile();
			n = GetOrderFromPoint(pt);
			if(n >= Order().size() && n >= sndFile.GetModSpecifications().ordersMax)
				n = ORDERINDEX_INVALID;
		}
		if(n != m_nDropPos)
		{
			if(n != ORDERINDEX_INVALID)
			{
				m_nMouseDownPos = ORDERINDEX_INVALID;
				m_nDropPos = n;
				Invalidate(false);
				setCursor(CMainFrame::curDragging);
			} else
			{
				m_nDropPos = ORDERINDEX_INVALID;
				setCursor(CMainFrame::curNoDrop);
			}
		}
	} else
	{
		Window::OnMouseMove(nFlags, pt);
	}
}


void COrderList::OnSelectSequence(uint32 nid)
{
	SelectSequence(static_cast<SEQUENCEINDEX>(nid - ID_SEQUENCE_ITEM));
}


void COrderList::OnRButtonUp(uint32 nFlags, mfcless::Point pt)
{
	mfcless::Rect rect;
	GetClientRect(&rect);
	if(m_bDragging)
	{
		m_nDropPos = ORDERINDEX_INVALID;
		OnLButtonUp(nFlags, pt);
	}
	if(pt.y >= rect.bottom)
		return;

	m_menuOrder = GetOrderFromPoint(pt);
	bool multiSelection = (m_nScrollPos2nd != ORDERINDEX_INVALID);

	if(!multiSelection)
		SetCurSel(m_menuOrder, false, false, false);
	setFocus();
	mfcless::Menu hMenu = mfcless::Menu::CreatePopup();
	if(!hMenu)
		return;

	CSoundFile &sndFile = m_modDoc.GetSoundFile();

	// Check if at least one pattern in the current selection exists
	bool patExists = false;
	OrdSelection selection = GetCurSel();
	LimitMax(selection.lastOrd, Order().GetLastIndex());
	for(ORDERINDEX ord = selection.firstOrd; ord <= selection.lastOrd && !patExists; ord++)
	{
		patExists = Order().IsValidPat(ord);
	}

	const uint32 greyed = patExists ? 0 : mfcless::menu::Grayed;

	CInputHandler *ih = CMainFrame::GetInputHandler();

	if(multiSelection)
	{
		// Several patterns are selected.
		hMenu.AppendMenu(mfcless::menu::String, ID_ORDERLIST_INSERT, ih->GetKeyTextFromCommand(kcOrderlistEditInsert, UL_("&Insert Patterns")));
		hMenu.AppendMenu(mfcless::menu::String, ID_ORDERLIST_DELETE, ih->GetKeyTextFromCommand(kcOrderlistEditDelete, UL_("&Remove Patterns")));
		hMenu.AppendMenu(mfcless::menu::Separator, NULL, UL_(""));
		hMenu.AppendMenu(mfcless::menu::String, ID_ORDERLIST_EDIT_COPY, ih->GetKeyTextFromCommand(kcEditCopy, UL_("&Copy Patterns")));
		hMenu.AppendMenu(mfcless::menu::String, ID_ORDERLIST_EDIT_COPY_ORDERS, ih->GetKeyTextFromCommand(kcOrderlistEditCopyOrders, UL_("&Copy Orders")));
		hMenu.AppendMenu(mfcless::menu::String, ID_ORDERLIST_EDIT_CUT, ih->GetKeyTextFromCommand(kcEditCut, UL_("&C&ut Patterns")));
		hMenu.AppendMenu(mfcless::menu::String | greyed, ID_PATTERNPASTE, ih->GetKeyTextFromCommand(kcEditPaste, UL_("P&aste Patterns")));
		hMenu.AppendMenu(mfcless::menu::Separator, NULL, UL_(""));
		hMenu.AppendMenu(mfcless::menu::String | greyed, ID_ORDERLIST_COPY, ih->GetKeyTextFromCommand(kcDuplicatePattern, UL_("&Duplicate Patterns")));
		hMenu.AppendMenu(mfcless::menu::String | greyed, ID_ORDERLIST_MERGE, ih->GetKeyTextFromCommand(kcMergePatterns, UL_("&Merge Patterns")));
	} else
	{
		// Only one pattern is selected
		hMenu.AppendMenu(mfcless::menu::String, ID_ORDERLIST_INSERT, ih->GetKeyTextFromCommand(kcOrderlistEditInsert, UL_("&Insert Pattern")));
		if(sndFile.GetModSpecifications().hasIgnoreIndex)
		{
			hMenu.AppendMenu(mfcless::menu::String, ID_ORDERLIST_INSERT_SEPARATOR, ih->GetKeyTextFromCommand(kcOrderlistEditInsertSeparator, UL_("&Insert Separator")));
		}
		hMenu.AppendMenu(mfcless::menu::String, ID_ORDERLIST_DELETE, ih->GetKeyTextFromCommand(kcOrderlistEditDelete, UL_("&Remove Pattern")));
		hMenu.AppendMenu(mfcless::menu::Separator, NULL, UL_(""));
		hMenu.AppendMenu(mfcless::menu::String, ID_ORDERLIST_NEW, ih->GetKeyTextFromCommand(kcNewPattern, UL_("Create &New Pattern")));
		hMenu.AppendMenu(mfcless::menu::String | greyed, ID_ORDERLIST_COPY, ih->GetKeyTextFromCommand(kcDuplicatePattern, UL_("&Duplicate Pattern")));
		hMenu.AppendMenu(mfcless::menu::String | greyed, ID_PATTERNCOPY, UL_("&Copy Pattern"));
		hMenu.AppendMenu(mfcless::menu::String, ID_PATTERNPASTE, ih->GetKeyTextFromCommand(kcEditPaste, UL_("P&aste Pattern")));
		const bool hasPatternProperties = sndFile.GetModSpecifications().patternRowsMin != sndFile.GetModSpecifications().patternRowsMax;
		if(hasPatternProperties || sndFile.GetModSpecifications().hasRestartPos)
		{
			hMenu.AppendMenu(mfcless::menu::Separator, NULL, UL_(""));
			if(hasPatternProperties)
				hMenu.AppendMenu(mfcless::menu::String | greyed, ID_PATTERN_PROPERTIES, UL_("&Pattern Properties..."));
			if(sndFile.GetModSpecifications().hasRestartPos)
				hMenu.AppendMenu(mfcless::menu::String | greyed | ((Order().GetRestartPos() == m_nScrollPos) ? mfcless::menu::Checked : 0), ID_SETRESTARTPOS, UL_("R&estart Position"));
		}
		if(sndFile.GetModSpecifications().sequencesMax > 1)
		{
			hMenu.AppendMenu(mfcless::menu::Separator, NULL, UL_(""));

			mfcless::Menu menuSequence = mfcless::Menu::CreatePopup();
			hMenu.AppendMenu(mfcless::menu::Popup, menuSequence, UL_("&Sequences"));

			const SEQUENCEINDEX numSequences = sndFile.Order.GetNumSequences();
			for(SEQUENCEINDEX i = 0; i < numSequences; i++)
			{
				mpt::ustring str;
				if(sndFile.Order(i).GetName().empty())
					str = MPT_UFORMAT("Sequence {}")(i + 1);
				else
					str = MPT_UFORMAT("{}: {}")(i + 1, mpt::ToUnicode(sndFile.Order(i).GetName()));
				const uint32 flags = (sndFile.Order.GetCurrentSequenceIndex() == i) ? mfcless::menu::String | mfcless::menu::Checked : mfcless::menu::String;
				menuSequence.AppendMenu(flags, ID_SEQUENCE_ITEM + i, str);
			}
			if(sndFile.Order.GetNumSequences() < sndFile.GetModSpecifications().sequencesMax)
			{
				menuSequence.AppendMenu(mfcless::menu::String, ID_SEQUENCE_ITEM + kDuplicateSequence, UL_("&Duplicate current sequence"));
				menuSequence.AppendMenu(mfcless::menu::String, ID_SEQUENCE_ITEM + kAddSequence, UL_("&Create empty sequence"));
			}
			if(sndFile.Order.GetNumSequences() > 1)
				menuSequence.AppendMenu(mfcless::menu::String, ID_SEQUENCE_ITEM + kDeleteSequence, UL_("D&elete current sequence"));
			else
				menuSequence.AppendMenu(mfcless::menu::String, ID_SEQUENCE_ITEM + kSplitSequence, UL_("&Split sub songs into sequences"));
		}
	}
	hMenu.AppendMenu(mfcless::menu::Separator, NULL, UL_(""));
	hMenu.AppendMenu(((selection.firstOrd == sndFile.m_lockOrderStart && selection.lastOrd == sndFile.m_lockOrderEnd) ? (mfcless::menu::String | mfcless::menu::Checked) : mfcless::menu::String), ID_ORDERLIST_LOCKPLAYBACK, ih->GetKeyTextFromCommand(kcOrderlistLockPlayback, UL_("&Lock Playback to Selection")));
	hMenu.AppendMenu((sndFile.m_lockOrderStart == ORDERINDEX_INVALID ? (mfcless::menu::String | mfcless::menu::Grayed) : mfcless::menu::String), ID_ORDERLIST_UNLOCKPLAYBACK, ih->GetKeyTextFromCommand(kcOrderlistUnlockPlayback, UL_("&Unlock Playback")));
	if(!multiSelection)
	{
		mfcless::Menu menuQueue = mfcless::Menu::CreatePopup();
		menuQueue.AppendMenu(mfcless::menu::String, ID_QUEUE_AT_PATTERN_END, ih->GetKeyTextFromCommand(kcOrderlistQueueAtPatternEnd, UL_("Transition at end of current &pattern")));
		menuQueue.AppendMenu(mfcless::menu::String, ID_QUEUE_AT_MEASURE_END, ih->GetKeyTextFromCommand(kcOrderlistQueueAtMeasureEnd, UL_("Transition at end of current &measure")));
		menuQueue.AppendMenu(mfcless::menu::String, ID_QUEUE_AT_BEAT_END, ih->GetKeyTextFromCommand(kcOrderlistQueueAtBeatEnd, UL_("Transition at end of current &beat")));
		menuQueue.AppendMenu(mfcless::menu::String, ID_QUEUE_AT_ROW_END, ih->GetKeyTextFromCommand(kcOrderlistQueueAtRowEnd, UL_("Transition at end of current &row")));
		hMenu.AppendMenu(mfcless::menu::Popup, menuQueue, UL_("&Queue Pattern"));
	}

	hMenu.AppendMenu(mfcless::menu::Separator, NULL, UL_(""));
	hMenu.AppendMenu(mfcless::menu::String | greyed, ID_ORDERLIST_RENDER, ih->GetKeyTextFromCommand(kcOrderlistStreamExport, UL_("Stream E&xport")));

	ClientToScreen(&pt);
	hMenu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, pt.x, pt.y, this);
	hMenu.DestroyMenu();
}


void COrderList::OnLButtonDblClk(uint32, mfcless::Point)
{
	auto &sndFile = m_modDoc.GetSoundFile();
	m_nScrollPos2nd = ORDERINDEX_INVALID;
	setFocus();
	if(!EnsureEditable(m_nScrollPos))
		return;
	PATTERNINDEX pat = Order()[m_nScrollPos];
	if(sndFile.Patterns.IsValidPat(pat))
		m_pParent.SetCurrentPattern(pat);
	else if(pat != PATTERNINDEX_SKIP)
		OnCreateNewPattern();
}


void COrderList::OnMButtonDown(uint32 nFlags, mfcless::Point pt)
{
	MPT_UNREFERENCED_PARAMETER(nFlags);
	QueuePattern(GetOrderFromPoint(pt), CMainFrame::GetInputHandler()->ModifierKeysToTransitionMode());
}


void COrderList::OnHScroll(mfcless::ScrollCode nSBCode, int /*nPos*/, QWidget *)
{
	uint32 nNewPos = m_nXScroll;
	int smin, smaxInt;

	GetScrollRange(smin, smaxInt);
	const uint32 smax = static_cast<uint32>(std::max(smaxInt, 0));
	m_bScrolling = true;
	switch(nSBCode)
	{
	case mfcless::ScrollCode::LineUp:		if (nNewPos) nNewPos--; break;
	case mfcless::ScrollCode::LineDown:		if (nNewPos < smax) nNewPos++; break;
	case mfcless::ScrollCode::PageUp:		if (nNewPos > 4) nNewPos -= 4; else nNewPos = 0; break;
	case mfcless::ScrollCode::PageDown:		if (nNewPos + 4 < smax) nNewPos += 4; else nNewPos = smax; break;
	case mfcless::ScrollCode::ThumbPosition:
	case mfcless::ScrollCode::ThumbTrack:		nNewPos = GetScrollPos(true); break;
	case mfcless::ScrollCode::Top:			nNewPos = 0; break;
	case mfcless::ScrollCode::Bottom:			nNewPos = smax; break;
	case mfcless::ScrollCode::EndScroll:		m_bScrolling = false; break;
	}
	if (nNewPos > smax) nNewPos = smax;
	if (nNewPos != m_nXScroll)
	{
		m_nXScroll = static_cast<ORDERINDEX>(nNewPos);
		SetScrollPos(m_nXScroll);
		Invalidate(false);
	}
}


void COrderList::OnSize(uint32 nType, int cx, int cy)
{
	int nPos;
	int smin, smax;

	Window::OnSize(nType, cx, cy);
	UpdateScrollInfo();
	GetScrollRange(smin, smax);
	nPos = GetScrollPos();
	if(nPos > smax)
		nPos = smax;
	if(m_nXScroll != nPos)
	{
		m_nXScroll = mpt::saturate_cast<ORDERINDEX>(nPos);
		SetScrollPos(m_nXScroll);
		Invalidate(false);
	}
}


void COrderList::OnInsertOrder()
{
	// insert the same order(s) after the currently selected order(s)
	ModSequence &order = Order();

	const OrdSelection selection = GetCurSel();
	const ORDERINDEX insertCount = order.insert(selection.lastOrd + 1, selection.lastOrd - selection.firstOrd + 1);
	if(!insertCount)
		return;

	std::copy(order.begin() + selection.firstOrd, order.begin() + selection.firstOrd + insertCount, order.begin() + selection.lastOrd + 1);

	InsertUpdatePlaystate(selection.firstOrd, selection.lastOrd);

	m_nScrollPos = std::min(ORDERINDEX(selection.lastOrd + 1), order.GetLastIndex());
	if(insertCount > 1)
		m_nScrollPos2nd = std::min(ORDERINDEX(m_nScrollPos + insertCount - 1), order.GetLastIndex());
	else
		m_nScrollPos2nd = ORDERINDEX_INVALID;

	InvalidateSelection();
	EnsureVisible(m_nScrollPos2nd);
	// first inserted order has higher priority than the last one
	EnsureVisible(m_nScrollPos);

	Invalidate(false);
	m_modDoc.SetModified();
	m_modDoc.UpdateAllViews(nullptr, SequenceHint().Data(), this);
	UpdateInfoText();
}


void COrderList::OnInsertSeparatorPattern()
{
	// Insert a separator pattern after the current pattern, don't move order list cursor
	ModSequence &order = Order();

	const OrdSelection selection = GetCurSel(true);
	ORDERINDEX insertPos = selection.firstOrd;

	if(!EnsureEditable(insertPos))
		return;
	if(order[insertPos] != PATTERNINDEX_INVALID)
	{
		// If we're not inserting at a stop (---) index, we move on by one position.
		insertPos++;
		order.insert(insertPos, 1, PATTERNINDEX_SKIP);
	} else
	{
		order[insertPos] = PATTERNINDEX_SKIP;
	}

	InsertUpdatePlaystate(insertPos, insertPos);

	Invalidate(false);
	m_modDoc.SetModified();
	m_modDoc.UpdateAllViews(nullptr, SequenceHint().Data(), this);
}


void COrderList::OnRenderOrder()
{
	OrdSelection selection = GetCurSel();
	m_modDoc.OnFileWaveConvert(selection.firstOrd, selection.lastOrd);
}


void COrderList::OnDeleteOrder()
{
	OrdSelection selection = GetCurSel();
	// remove selection
	m_nScrollPos2nd = ORDERINDEX_INVALID;

	Order().Remove(selection.firstOrd, selection.lastOrd);

	m_modDoc.SetModified();
	Invalidate(false);
	m_modDoc.UpdateAllViews(nullptr, SequenceHint().Data(), this);

	DeleteUpdatePlaystate(selection.firstOrd, selection.lastOrd);

	SetCurSel(selection.firstOrd, true, false, true);
}


void COrderList::OnPatternProperties()
{
	ModSequence &order = Order();
	if(order.IsValidPat(m_menuOrder))
		m_pParent.PostViewMessage(VIEWMSG_PATTERNPROPERTIES, order[m_menuOrder]);
}


void COrderList::OnPlayerPlay()
{
	mfcless::PostCommand(&m_pParent, ID_PLAYER_PLAY);
}


void COrderList::OnPlayerPause()
{
	mfcless::PostCommand(&m_pParent, ID_PLAYER_PAUSE);
}


void COrderList::OnPlayerPlayFromStart()
{
	mfcless::PostCommand(&m_pParent, ID_PLAYER_PLAYFROMSTART);
}


void COrderList::OnPatternPlayFromStart()
{
	mfcless::PostCommand(&m_pParent, IDC_PATTERN_PLAYFROMSTART);
}


void COrderList::OnCreateNewPattern()
{
	mfcless::PostCommand(&m_pParent, ID_ORDERLIST_NEW);
}


void COrderList::OnDuplicatePattern()
{
	mfcless::PostCommand(&m_pParent, ID_ORDERLIST_COPY);
}


void COrderList::OnMergePatterns()
{
	mfcless::PostCommand(&m_pParent, ID_ORDERLIST_MERGE);
}


void COrderList::OnPatternCopy()
{
	PATTERNINDEX pat = PATTERNINDEX_INVALID;
	OrdSelection selection = GetCurSel();
	if(selection.firstOrd < Order().size())
		pat = Order()[selection.firstOrd];
	m_pParent.PostViewMessage(VIEWMSG_COPYPATTERN, pat);
}


void COrderList::OnPatternPaste()
{
	mfcless::PostCommand(&m_pParent, ID_PATTERNPASTE);
}


void COrderList::OnSetRestartPos()
{
	CSoundFile &sndFile = m_modDoc.GetSoundFile();
	bool modified = false;
	if(m_menuOrder == Order().GetRestartPos())
	{
		// Unset position
		modified = (m_menuOrder != 0);
		Order().SetRestartPos(0);
	} else if(sndFile.GetModSpecifications().hasRestartPos)
	{
		// Set new position
		modified = true;
		Order().SetRestartPos(m_menuOrder);
	}
	if(modified)
	{
		m_modDoc.SetModified();
		m_modDoc.UpdateAllViews(nullptr, SequenceHint().RestartPos(), this);
	}
}


intptr_t COrderList::OnDragonDropping(uintptr_t doDrop, intptr_t lParam)
{
	const DRAGONDROP *pDropInfo = (const DRAGONDROP *)lParam;
	mfcless::Point pt;

	if((!pDropInfo) || (&m_modDoc.GetSoundFile() != pDropInfo->sndFile) || (!m_cxFont))
		return false;
	bool canDrop = false;
	switch(pDropInfo->dropType)
	{
	case DRAGONDROP_ORDER:
		if(pDropInfo->dropItem >= Order().size())
			break;
		[[fallthrough]];
	case DRAGONDROP_PATTERN:
		canDrop = true;
		break;
	default:
		break;
	}
	if(!canDrop || !doDrop)
		return canDrop;
	mfcless::GetCursorPos(&pt);
	ScreenToClient(&pt);
	if(pt.x < 0)
		pt.x = 0;
	ORDERINDEX posDest = mpt::saturate_cast<ORDERINDEX>(m_nXScroll + (pt.x / m_cxFont));
	if(posDest >= Order().size())
		return false;
	switch(pDropInfo->dropType)
	{
	case DRAGONDROP_PATTERN:
		Order()[posDest] = static_cast<PATTERNINDEX>(pDropInfo->dropItem);
		break;
	case DRAGONDROP_ORDER:
		Order()[posDest] = Order()[pDropInfo->dropItem];
		break;
	default:
		break;
	}
	if(canDrop)
	{
		Invalidate(false);
		m_modDoc.SetModified();
		m_modDoc.UpdateAllViews(nullptr, SequenceHint().Data(), this);
		SetCurSel(posDest, true);
	}
	return canDrop;
}


ORDERINDEX COrderList::SetMargins(int i)
{
	m_nOrderlistMargins = i;
	return GetMargins();
}


void COrderList::SelectSequence(const SEQUENCEINDEX seq)
{
	CriticalSection cs;

	CMainFrame::GetMainFrame()->ResetNotificationBuffer();
	CSoundFile &sndFile = m_modDoc.GetSoundFile();
	const bool editSequence = seq >= sndFile.Order.GetNumSequences();
	if(seq == kSplitSequence)
	{
		if(!sndFile.Order.CanSplitSubsongs())
		{
			Reporting::Information(U_("No sub songs have been found in this sequence."));
			return;
		}
		if(Reporting::Confirm(U_("The order list contains separator items.\nDo you want to split the sequence at the separators into multiple song sequences?")) != cnfYes)
			return;
		if(!sndFile.Order.SplitSubsongsToMultipleSequences())
			return;
	} else if(seq == kDeleteSequence)
	{
		SEQUENCEINDEX curSeq = sndFile.Order.GetCurrentSequenceIndex();
		mpt::ustring str = MPT_UFORMAT("Remove sequence {}: {}?")(curSeq + 1, mpt::ToUnicode(Order().GetName()));
		if(Reporting::Confirm(str) == cnfYes)
			sndFile.Order.RemoveSequence(curSeq);
		else
			return;
	} else if(seq == kAddSequence || seq == kDuplicateSequence)
	{
		const bool duplicate = (seq == kDuplicateSequence);
		const SEQUENCEINDEX newIndex = sndFile.Order.GetCurrentSequenceIndex() + 1u;
		std::vector<SEQUENCEINDEX> newOrder(sndFile.Order.GetNumSequences());
		std::iota(newOrder.begin(), newOrder.end(), SEQUENCEINDEX(0));
		newOrder.insert(newOrder.begin() + newIndex, duplicate ? sndFile.Order.GetCurrentSequenceIndex() : SEQUENCEINDEX_INVALID);
		if(m_modDoc.ReArrangeSequences(newOrder))
		{
			sndFile.Order.SetSequence(newIndex);
			if(const auto name = sndFile.Order().GetName(); duplicate && !name.empty())
				sndFile.Order().SetName(name + U_(" (Copy)"));
		}
	} else if(seq == sndFile.Order.GetCurrentSequenceIndex())
		return;
	else if(seq < sndFile.Order.GetNumSequences())
		sndFile.Order.SetSequence(seq);
	ORDERINDEX posCandidate = Order().GetLengthTailTrimmed() - 1;
	SetCurSel(std::min(m_nScrollPos, posCandidate), true, false, true);
	m_pParent.SetCurrentPattern(Order()[m_nScrollPos]);

	UpdateScrollInfo();
	// This won't make sense anymore in the new sequence.
	OnUnlockPlayback();

	cs.Leave();

	if(editSequence)
	{
		m_modDoc.SetModified();
		m_modDoc.UpdateAllViews(nullptr, SequenceHint(SEQUENCEINDEX_INVALID).Names().Data(), nullptr);
	} else
	{
		m_modDoc.UpdateAllViews(nullptr, SequenceHint(SEQUENCEINDEX_INVALID).Data(), nullptr);
	}
}


void COrderList::QueuePattern(ORDERINDEX order, OrderTransitionMode transitionMode)
{
	CSoundFile &sndFile = m_modDoc.GetSoundFile();
	const ORDERINDEX length = Order().GetLength();

	// If this is not a playable order item, find the next valid item.
	while(order < length && (Order()[order] == PATTERNINDEX_SKIP || Order()[order] == PATTERNINDEX_INVALID))
	{
		order++;
	}

	if(order < length)
	{
		CriticalSection cs;
		if(sndFile.m_PlayState.m_nSeqOverride == order)
		{
			// This item is already queued: Dequeue it.
			sndFile.m_PlayState.m_nSeqOverride = ORDERINDEX_INVALID;
		} else
		{
			if(Order().IsPositionLocked(order))
			{
				// Users wants to go somewhere else, so let them do that.
				OnUnlockPlayback();
			}
			sndFile.m_PlayState.m_seqOverrideMode = transitionMode;
			sndFile.m_PlayState.m_nSeqOverride = order;
		}
		Invalidate(false);
	}
}


void COrderList::OnLockPlayback()
{
	CSoundFile &sndFile = m_modDoc.GetSoundFile();

	OrdSelection selection = GetCurSel();
	if(selection.firstOrd == sndFile.m_lockOrderStart && selection.lastOrd == sndFile.m_lockOrderEnd)
	{
		OnUnlockPlayback();
	} else
	{
		sndFile.m_lockOrderStart = selection.firstOrd;
		sndFile.m_lockOrderEnd = selection.lastOrd;
		Invalidate(false);
	}
}


void COrderList::OnUnlockPlayback()
{
	CSoundFile &sndFile = m_modDoc.GetSoundFile();
	sndFile.m_lockOrderStart = sndFile.m_lockOrderEnd = ORDERINDEX_INVALID;
	Invalidate(false);
}


void COrderList::InsertUpdatePlaystate(ORDERINDEX first, ORDERINDEX last)
{
	auto &sndFile = m_modDoc.GetSoundFile();
	Util::InsertItem(first, last, sndFile.m_PlayState.m_nNextOrder);
	if(sndFile.m_PlayState.m_nSeqOverride != ORDERINDEX_INVALID)
		Util::InsertItem(first, last, sndFile.m_PlayState.m_nSeqOverride);
	// Adjust order lock position
	if(sndFile.m_lockOrderStart != ORDERINDEX_INVALID)
		Util::InsertRange(first, last, sndFile.m_lockOrderStart, sndFile.m_lockOrderEnd);
}


void COrderList::DeleteUpdatePlaystate(ORDERINDEX first, ORDERINDEX last)
{
	auto &sndFile = m_modDoc.GetSoundFile();
	Util::DeleteItem(first, last, sndFile.m_PlayState.m_nNextOrder);
	if(sndFile.m_PlayState.m_nSeqOverride != ORDERINDEX_INVALID)
		Util::DeleteItem(first, last, sndFile.m_PlayState.m_nSeqOverride);
	// Adjust order lock position
	if(sndFile.m_lockOrderStart != ORDERINDEX_INVALID)
		Util::DeleteRange(first, last, sndFile.m_lockOrderStart, sndFile.m_lockOrderEnd);
}


intptr_t COrderList::OnToolHitTest(mfcless::Point point, mfcless::ToolInfo *pTI) const
{
	mfcless::Rect rect;
	GetClientRect(&rect);

	pTI->hwnd = const_cast<COrderList *>(this);
	pTI->uId = GetOrderFromPoint(point);
	// One tool per order box, so the tip changes as the mouse moves on.
	pTI->rect = GetRectFromOrder(static_cast<ORDERINDEX>(pTI->uId));
	return pTI->uId;
}


QString COrderList::GetToolTipText(uintptr_t id)
{
	mpt::ustring text;
	const CSoundFile &sndFile = m_modDoc.GetSoundFile();
	const ModSequence &order = Order();
	const ORDERINDEX ord = mpt::saturate_cast<ORDERINDEX>(id), ordLen = order.GetLengthTailTrimmed();
	text = mfcless::PrintF(UL_("Position %u of %u [%02Xh of %02Xh]"), ord, ordLen, ord, ordLen);
	if(order.IsValidPat(ord))
	{
		PATTERNINDEX pat = order[ord];
		const std::string name = sndFile.Patterns[pat].GetName();
		if(!name.empty())
			text += UL_("\n") + mpt::ToUnicode(sndFile.GetCharsetInternal(), name);  // multiline tooltip
	}
	return mfcless::Text(text).value;
}


OPENMPT_NAMESPACE_END
