// Files dropped from the desktop (WM_DROPFILES and the shell's DragQuery*
// functions), and the outline MFC's control bar dragging draws on the screen.
#include "QtWin.h"

#include <QtCore/QMimeData>
#include <QtCore/QUrl>
#include <QtGui/QDropEvent>
#include <QtGui/QPainter>

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace linopenmpt::qtwin
{

namespace
{

// What an HDROP is here.
struct DropData
{
	std::vector<std::wstring> files;
	POINT point{}; // client coordinates of the window it was dropped on
	bool inClient = true;
};

std::vector<std::wstring> LocalFiles(const QMimeData *mime)
{
	std::vector<std::wstring> files;
	if(mime == nullptr || !mime->hasUrls())
		return files;
	for(const QUrl &url : mime->urls())
	{
		if(url.isLocalFile())
			files.push_back(ToWString(url.toLocalFile()));
	}
	return files;
}

std::map<HWND, DropTargetCallbacks> &DropTargets()
{
	static std::map<HWND, DropTargetCallbacks> targets;
	return targets;
}

POINT ClientPoint(Win32Widget *w, QPoint pos)
{
	const QRect client = ClientRect(w);
	return POINT{pos.x() - client.left(), pos.y() - client.top()};
}

// A drag over a window with a drop target: OLE's calls instead of
// WM_DROPFILES.
bool HandleOleDrop(Win32Widget *w, QEvent *e, DropTargetCallbacks &target)
{
	switch(e->type())
	{
	case QEvent::DragEnter:
	case QEvent::DragMove:
	{
		auto *drag = static_cast<QDragMoveEvent *>(e);
		const auto files = LocalFiles(drag->mimeData());
		DWORD effect = 0;
		if(!files.empty() && target.over)
			effect = target.over(files, static_cast<DWORD>(MouseKeyFlags(drag->buttons(), drag->modifiers())), ClientPoint(w, drag->position().toPoint()), e->type() == QEvent::DragEnter);
		if(effect != 0)
		{
			drag->setDropAction((effect & 2) ? Qt::MoveAction : ((effect & 4) ? Qt::LinkAction : Qt::CopyAction));
			drag->accept();
		} else if(e->type() == QEvent::DragEnter && !files.empty())
		{
			// Not here yet, but keep the moves coming: OLE asks the target
			// at every move, and the item under the mouse decides.
			drag->accept();
		} else
		{
			drag->ignore();
		}
		return true;
	}
	case QEvent::DragLeave:
		if(target.leave)
			target.leave();
		return true;
	case QEvent::Drop:
	{
		auto *drop = static_cast<QDropEvent *>(e);
		const auto files = LocalFiles(drop->mimeData());
		const DWORD effect = drop->dropAction() == Qt::MoveAction ? 2 : (drop->dropAction() == Qt::LinkAction ? 4 : 1);
		if(!files.empty() && target.drop && target.drop(files, effect, ClientPoint(w, drop->position().toPoint())))
			drop->acceptProposedAction();
		else
			drop->ignore();
		return true;
	}
	default:
		return false;
	}
}

} // namespace

void RegisterDropTarget(HWND hwnd, DropTargetCallbacks callbacks)
{
	DropTargets()[hwnd] = std::move(callbacks);
	if(Win32Widget *w = FromHandle(hwnd))
		w->setAcceptDrops(true);
}

void RevokeDropTarget(HWND hwnd)
{
	DropTargets().erase(hwnd);
	if(Win32Widget *w = FromHandle(hwnd))
		w->setAcceptDrops((w->data.exStyle & WS_EX_ACCEPTFILES) != 0);
}

HDROP CreateDrop(const std::vector<std::wstring> &files, POINT point, bool inClient)
{
	auto *data = new DropData;
	data->files = files;
	data->point = point;
	data->inClient = inClient;
	return reinterpret_cast<HDROP>(data);
}

bool HandleFileDropEvent(Win32Widget *w, QEvent *e)
{
	{
		auto it = DropTargets().find(w->hwnd());
		if(it != DropTargets().end())
		{
			// A copy: the target may revoke itself while it runs.
			DropTargetCallbacks target = it->second;
			return HandleOleDrop(w, e, target);
		}
	}
	switch(e->type())
	{
	case QEvent::DragEnter:
	case QEvent::DragMove:
	{
		auto *drag = static_cast<QDragMoveEvent *>(e);
		if((w->data.exStyle & WS_EX_ACCEPTFILES) && !LocalFiles(drag->mimeData()).empty())
		{
			drag->setDropAction(Qt::CopyAction);
			drag->accept();
		} else
		{
			drag->ignore();
		}
		return true;
	}
	case QEvent::DragLeave:
		return true;
	case QEvent::Drop:
	{
		auto *drop = static_cast<QDropEvent *>(e);
		auto files = LocalFiles(drop->mimeData());
		if(!(w->data.exStyle & WS_EX_ACCEPTFILES) || files.empty())
		{
			drop->ignore();
			return true;
		}
		drop->setDropAction(Qt::CopyAction);
		drop->accept();
		auto *data = new DropData;
		data->files = std::move(files);
		const QPoint pos = drop->position().toPoint();
		const QRect client = ClientRect(w);
		data->inClient = client.contains(pos);
		data->point = POINT{pos.x() - client.left(), pos.y() - client.top()};
		// The window procedure frees it with DragFinish().
		::SendMessage(w->hwnd(), WM_DROPFILES, reinterpret_cast<WPARAM>(data), 0);
		return true;
	}
	default:
		return false;
	}
}

// ---- the drag outline ----------------------------------------------------------------

namespace
{

// One side of the outline: a top-level strip that takes no input.
class DragStrip : public QWidget
{
public:
	DragStrip()
		: QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus)
	{
		setAttribute(Qt::WA_ShowWithoutActivating);
		setAttribute(Qt::WA_TransparentForMouseEvents);
	}
	bool dither = false;

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		if(dither)
		{
			// MFC's halftone brush: every other pixel.
			p.fillRect(rect(), Qt::white);
			p.fillRect(rect(), QBrush(Qt::black, Qt::Dense4Pattern));
		} else
		{
			p.fillRect(rect(), QColor(64, 64, 64));
		}
	}
};

struct DragFrame
{
	std::array<std::unique_ptr<DragStrip>, 4> strips;
	RECT rect{};
	bool shown = false;
};

DragFrame &Frame()
{
	static DragFrame frame;
	return frame;
}

} // namespace

} // namespace linopenmpt::qtwin

using namespace linopenmpt::qtwin;

void LinOpenMPTShowDragFrame(const RECT *rect, SIZE thickness, BOOL dither)
{
	DragFrame &f = Frame();
	if(rect == nullptr || ::IsRectEmpty(rect) || thickness.cx <= 0 || thickness.cy <= 0)
	{
		for(auto &strip : f.strips)
		{
			if(strip)
				strip->hide();
		}
		f.shown = false;
		f.rect = RECT{};
		return;
	}
	const int x = rect->left, y = rect->top, w = rect->right - rect->left, h = rect->bottom - rect->top;
	const int tx = std::min<int>(thickness.cx, w), ty = std::min<int>(thickness.cy, h);
	const QRect sides[4] = {
		QRect(x, y, w, ty),
		QRect(x, y + h - ty, w, ty),
		QRect(x, y + ty, tx, std::max(h - 2 * ty, 0)),
		QRect(x + w - tx, y + ty, tx, std::max(h - 2 * ty, 0)),
	};
	for(int i = 0; i < 4; i++)
	{
		auto &strip = f.strips[static_cast<std::size_t>(i)];
		if(!strip)
			strip = std::make_unique<DragStrip>();
		strip->dither = dither != FALSE;
		if(sides[i].isEmpty())
		{
			strip->hide();
			continue;
		}
		strip->setGeometry(sides[i]);
		strip->show();
		strip->update();
	}
	f.rect = *rect;
	f.shown = true;
}

BOOL LinOpenMPTGetDragFrame(RECT *rect)
{
	const DragFrame &f = Frame();
	if(rect != nullptr)
		*rect = f.shown ? f.rect : RECT{};
	return f.shown;
}

void DragAcceptFiles(HWND hwnd, BOOL accept)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return;
	if(accept)
		w->data.exStyle |= WS_EX_ACCEPTFILES;
	else
		w->data.exStyle &= ~WS_EX_ACCEPTFILES;
	w->setAcceptDrops(accept != FALSE);
}

UINT DragQueryFileW(HDROP drop, UINT index, LPWSTR file, UINT size)
{
	const auto *data = reinterpret_cast<const DropData *>(drop);
	if(data == nullptr)
		return 0;
	if(index == 0xFFFFFFFF)
		return static_cast<UINT>(data->files.size());
	if(index >= data->files.size())
		return 0;
	const std::wstring &name = data->files[index];
	if(file == nullptr || size == 0)
		return static_cast<UINT>(name.size()); // the length, without the terminator
	const std::size_t count = std::min<std::size_t>(name.size(), size - 1);
	std::copy_n(name.data(), count, file);
	file[count] = 0;
	return static_cast<UINT>(count);
}

UINT DragQueryFile(HDROP drop, UINT index, LPTSTR file, UINT size)
{
	return DragQueryFileW(drop, index, file, size);
}

BOOL DragQueryPoint(HDROP drop, LPPOINT point)
{
	const auto *data = reinterpret_cast<const DropData *>(drop);
	if(data == nullptr || point == nullptr)
		return FALSE;
	*point = data->point;
	return data->inClient;
}

void DragFinish(HDROP drop)
{
	delete reinterpret_cast<DropData *>(drop);
}
