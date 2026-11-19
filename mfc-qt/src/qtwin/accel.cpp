// Accelerator tables: CreateAcceleratorTable, LoadAccelerators (RT_ACCELERATOR
// resources) and TranslateAccelerator, which turns a matching WM_KEYDOWN /
// WM_SYSKEYDOWN / WM_CHAR into WM_COMMAND (HIWORD 1) for the window.
//
// OpenMPT routes keys through its own input handler and has no accelerator
// resources, but MFC's frames translate through m_hAccelTable, so this keeps
// that path working for anything that sets one.

#include "QtWin.h"

#include <algorithm>
#include <set>
#include <vector>

namespace
{

struct AccelTable
{
	std::vector<ACCEL> entries;
};

std::set<AccelTable *> &Tables()
{
	static std::set<AccelTable *> tables;
	return tables;
}

AccelTable *Table(HACCEL h)
{
	auto *t = reinterpret_cast<AccelTable *>(h);
	return Tables().count(t) ? t : nullptr;
}

HACCEL Make(std::vector<ACCEL> entries)
{
	auto *t = new AccelTable{std::move(entries)};
	Tables().insert(t);
	return reinterpret_cast<HACCEL>(t);
}

bool Down(int vk)
{
	return (::GetKeyState(vk) & 0x8000) != 0;
}

} // namespace

HACCEL CreateAcceleratorTable(LPACCEL accel, int count)
{
	if(accel == nullptr || count <= 0)
		return nullptr;
	return Make(std::vector<ACCEL>(accel, accel + count));
}

BOOL DestroyAcceleratorTable(HACCEL h)
{
	AccelTable *t = Table(h);
	if(t == nullptr)
		return FALSE;
	Tables().erase(t);
	delete t;
	return TRUE;
}

int CopyAcceleratorTable(HACCEL h, LPACCEL accel, int count)
{
	AccelTable *t = Table(h);
	if(t == nullptr)
		return 0;
	const int size = static_cast<int>(t->entries.size());
	if(accel == nullptr)
		return size;
	const int n = std::min(size, count);
	std::copy(t->entries.begin(), t->entries.begin() + n, accel);
	return n;
}

HACCEL LoadAccelerators(HINSTANCE instance, LPCTSTR name)
{
	// The resource is an array of ACCELTABLEENTRY {WORD fFlags, wAnsi, wId,
	// padding}; the last entry has 0x80 in fFlags.
	HRSRC res = ::FindResource(instance, name, RT_ACCELERATOR);
	if(res == nullptr)
		return nullptr;
	HGLOBAL mem = ::LoadResource(instance, res);
	const auto *p = static_cast<const WORD *>(::LockResource(mem));
	const DWORD size = ::SizeofResource(instance, res);
	if(p == nullptr)
		return nullptr;
	std::vector<ACCEL> entries;
	for(DWORD i = 0; (i + 1) * 8 <= size; i++)
	{
		const WORD flags = p[i * 4];
		entries.push_back({static_cast<BYTE>(flags & 0x7F), p[i * 4 + 1], p[i * 4 + 2]});
		if(flags & 0x80)
			break;
	}
	return entries.empty() ? nullptr : Make(std::move(entries));
}

int TranslateAccelerator(HWND hwnd, HACCEL h, LPMSG msg)
{
	AccelTable *t = Table(h);
	if(t == nullptr || msg == nullptr || hwnd == nullptr)
		return 0;
	const bool keyDown = msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN;
	const bool character = msg->message == WM_CHAR || msg->message == WM_SYSCHAR;
	if(!keyDown && !character)
		return 0;
	const bool alt = msg->message == WM_SYSKEYDOWN || msg->message == WM_SYSCHAR || Down(VK_MENU);
	for(const ACCEL &a : t->entries)
	{
		bool match;
		if(a.fVirt & FVIRTKEY)
		{
			match = keyDown && a.key == static_cast<WORD>(msg->wParam)
				&& ((a.fVirt & FSHIFT) != 0) == Down(VK_SHIFT)
				&& ((a.fVirt & FCONTROL) != 0) == Down(VK_CONTROL)
				&& ((a.fVirt & FALT) != 0) == alt;
		} else
		{
			// Character accelerators match the translated character; FALT
			// still has to agree with the Alt state.
			match = character && a.key == static_cast<WORD>(msg->wParam) && ((a.fVirt & FALT) != 0) == alt;
		}
		if(!match)
			continue;
		// Windows skips the command while the window is disabled (and while a
		// menu would show the item disabled, which callers check themselves).
		if(!::IsWindowEnabled(hwnd))
			return 1;
		::SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(a.cmd, 1), 0);
		return 1;
	}
	return 0;
}
