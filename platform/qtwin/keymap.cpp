// linOpenMPT platform layer: keyboard and mouse state for Win32 on Qt.
//
// Qt key events become Windows virtual-key codes and scan codes:
//  - letters follow the keyboard layout (Qt's key), as Windows' VK_A..VK_Z
//    do;
//  - every other key is identified by its position, so digits, punctuation
//    and the note keys of the pattern editor sit where they would on a
//    Windows keyboard. On Linux (X11 and Wayland) Qt's native scan code is
//    the evdev code plus 8, and evdev codes of the main block equal Windows'
//    set-1 scan codes;
//  - keypad keys follow Num Lock, through Qt's key.
// Without native scan codes (offscreen tests, other platforms), Qt's key
// alone decides.
#include "QtWin.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>

#include <array>
#include <cwctype>

namespace linopenmpt::qtwin
{

namespace
{

// The US layout: the reference Windows key names and characters use.
struct KeyEntry
{
	UINT scan;
	bool extended;
	UINT vk;
	const wchar_t *name;
	wchar_t normal;
	wchar_t shifted;
};

const KeyEntry g_keys[] = {
	{0x01, false, VK_ESCAPE, L"Esc", 0x1B, 0x1B},
	{0x02, false, '1', L"1", L'1', L'!'},
	{0x03, false, '2', L"2", L'2', L'@'},
	{0x04, false, '3', L"3", L'3', L'#'},
	{0x05, false, '4', L"4", L'4', L'$'},
	{0x06, false, '5', L"5", L'5', L'%'},
	{0x07, false, '6', L"6", L'6', L'^'},
	{0x08, false, '7', L"7", L'7', L'&'},
	{0x09, false, '8', L"8", L'8', L'*'},
	{0x0A, false, '9', L"9", L'9', L'('},
	{0x0B, false, '0', L"0", L'0', L')'},
	{0x0C, false, VK_OEM_MINUS, L"-", L'-', L'_'},
	{0x0D, false, VK_OEM_PLUS, L"=", L'=', L'+'},
	{0x0E, false, VK_BACK, L"Backspace", L'\b', L'\b'},
	{0x0F, false, VK_TAB, L"Tab", L'\t', L'\t'},
	{0x10, false, 'Q', L"Q", L'q', L'Q'},
	{0x11, false, 'W', L"W", L'w', L'W'},
	{0x12, false, 'E', L"E", L'e', L'E'},
	{0x13, false, 'R', L"R", L'r', L'R'},
	{0x14, false, 'T', L"T", L't', L'T'},
	{0x15, false, 'Y', L"Y", L'y', L'Y'},
	{0x16, false, 'U', L"U", L'u', L'U'},
	{0x17, false, 'I', L"I", L'i', L'I'},
	{0x18, false, 'O', L"O", L'o', L'O'},
	{0x19, false, 'P', L"P", L'p', L'P'},
	{0x1A, false, VK_OEM_4, L"[", L'[', L'{'},
	{0x1B, false, VK_OEM_6, L"]", L']', L'}'},
	{0x1C, false, VK_RETURN, L"Enter", L'\r', L'\r'},
	{0x1C, true, VK_RETURN, L"Num Enter", L'\r', L'\r'},
	{0x1D, false, VK_CONTROL, L"Ctrl", 0, 0},
	{0x1D, true, VK_CONTROL, L"Right Ctrl", 0, 0},
	{0x1E, false, 'A', L"A", L'a', L'A'},
	{0x1F, false, 'S', L"S", L's', L'S'},
	{0x20, false, 'D', L"D", L'd', L'D'},
	{0x21, false, 'F', L"F", L'f', L'F'},
	{0x22, false, 'G', L"G", L'g', L'G'},
	{0x23, false, 'H', L"H", L'h', L'H'},
	{0x24, false, 'J', L"J", L'j', L'J'},
	{0x25, false, 'K', L"K", L'k', L'K'},
	{0x26, false, 'L', L"L", L'l', L'L'},
	{0x27, false, VK_OEM_1, L";", L';', L':'},
	{0x28, false, VK_OEM_7, L"'", L'\'', L'"'},
	{0x29, false, VK_OEM_3, L"`", L'`', L'~'},
	{0x2A, false, VK_SHIFT, L"Shift", 0, 0},
	{0x2B, false, VK_OEM_5, L"\\", L'\\', L'|'},
	{0x2C, false, 'Z', L"Z", L'z', L'Z'},
	{0x2D, false, 'X', L"X", L'x', L'X'},
	{0x2E, false, 'C', L"C", L'c', L'C'},
	{0x2F, false, 'V', L"V", L'v', L'V'},
	{0x30, false, 'B', L"B", L'b', L'B'},
	{0x31, false, 'N', L"N", L'n', L'N'},
	{0x32, false, 'M', L"M", L'm', L'M'},
	{0x33, false, VK_OEM_COMMA, L",", L',', L'<'},
	{0x34, false, VK_OEM_PERIOD, L".", L'.', L'>'},
	{0x35, false, VK_OEM_2, L"/", L'/', L'?'},
	{0x35, true, VK_DIVIDE, L"Num /", L'/', L'/'},
	{0x36, false, VK_SHIFT, L"Right Shift", 0, 0},
	{0x37, false, VK_MULTIPLY, L"Num *", L'*', L'*'},
	{0x37, true, VK_SNAPSHOT, L"Prnt Scrn", 0, 0},
	{0x38, false, VK_MENU, L"Alt", 0, 0},
	{0x38, true, VK_MENU, L"Right Alt", 0, 0},
	{0x39, false, VK_SPACE, L"Space", L' ', L' '},
	{0x3A, false, VK_CAPITAL, L"Caps Lock", 0, 0},
	{0x3B, false, VK_F1, L"F1", 0, 0},
	{0x3C, false, VK_F2, L"F2", 0, 0},
	{0x3D, false, VK_F3, L"F3", 0, 0},
	{0x3E, false, VK_F4, L"F4", 0, 0},
	{0x3F, false, VK_F5, L"F5", 0, 0},
	{0x40, false, VK_F6, L"F6", 0, 0},
	{0x41, false, VK_F7, L"F7", 0, 0},
	{0x42, false, VK_F8, L"F8", 0, 0},
	{0x43, false, VK_F9, L"F9", 0, 0},
	{0x44, false, VK_F10, L"F10", 0, 0},
	{0x45, false, VK_PAUSE, L"Pause", 0, 0},
	{0x45, true, VK_NUMLOCK, L"Num Lock", 0, 0},
	{0x46, false, VK_SCROLL, L"Scroll Lock", 0, 0},
	{0x47, false, VK_NUMPAD7, L"Num 7", L'7', L'7'},
	{0x47, true, VK_HOME, L"Home", 0, 0},
	{0x48, false, VK_NUMPAD8, L"Num 8", L'8', L'8'},
	{0x48, true, VK_UP, L"Up", 0, 0},
	{0x49, false, VK_NUMPAD9, L"Num 9", L'9', L'9'},
	{0x49, true, VK_PRIOR, L"Page Up", 0, 0},
	{0x4A, false, VK_SUBTRACT, L"Num -", L'-', L'-'},
	{0x4B, false, VK_NUMPAD4, L"Num 4", L'4', L'4'},
	{0x4B, true, VK_LEFT, L"Left", 0, 0},
	{0x4C, false, VK_NUMPAD5, L"Num 5", L'5', L'5'},
	{0x4D, false, VK_NUMPAD6, L"Num 6", L'6', L'6'},
	{0x4D, true, VK_RIGHT, L"Right", 0, 0},
	{0x4E, false, VK_ADD, L"Num +", L'+', L'+'},
	{0x4F, false, VK_NUMPAD1, L"Num 1", L'1', L'1'},
	{0x4F, true, VK_END, L"End", 0, 0},
	{0x50, false, VK_NUMPAD2, L"Num 2", L'2', L'2'},
	{0x50, true, VK_DOWN, L"Down", 0, 0},
	{0x51, false, VK_NUMPAD3, L"Num 3", L'3', L'3'},
	{0x51, true, VK_NEXT, L"Page Down", 0, 0},
	{0x52, false, VK_NUMPAD0, L"Num 0", L'0', L'0'},
	{0x52, true, VK_INSERT, L"Insert", 0, 0},
	{0x53, false, VK_DECIMAL, L"Num Del", L'.', L'.'},
	{0x53, true, VK_DELETE, L"Delete", 0, 0},
	{0x56, false, VK_OEM_102, L"\\", L'\\', L'|'},
	{0x57, false, VK_F11, L"F11", 0, 0},
	{0x58, false, VK_F12, L"F12", 0, 0},
	{0x5B, true, VK_LWIN, L"Left Windows", 0, 0},
	{0x5C, true, VK_RWIN, L"Right Windows", 0, 0},
	{0x5D, true, VK_APPS, L"Application", 0, 0},
	{0x64, false, VK_F13, L"F13", 0, 0},
	{0x65, false, VK_F14, L"F14", 0, 0},
	{0x66, false, VK_F15, L"F15", 0, 0},
	{0x67, false, VK_F16, L"F16", 0, 0},
	{0x68, false, VK_F17, L"F17", 0, 0},
	{0x69, false, VK_F18, L"F18", 0, 0},
	{0x6A, false, VK_F19, L"F19", 0, 0},
	{0x6B, false, VK_F20, L"F20", 0, 0},
	{0x6C, false, VK_F21, L"F21", 0, 0},
	{0x6D, false, VK_F22, L"F22", 0, 0},
	{0x6E, false, VK_F23, L"F23", 0, 0},
	{0x76, false, VK_F24, L"F24", 0, 0},
};

const KeyEntry *FindScan(UINT scan, bool extended)
{
	for(const KeyEntry &k : g_keys)
	{
		if(k.scan == scan && k.extended == extended)
			return &k;
	}
	return nullptr;
}

const KeyEntry *FindVK(UINT vk)
{
	// Prefer the main-block key (e.g. Enter, not Num Enter).
	for(const KeyEntry &k : g_keys)
	{
		if(k.vk == vk && !k.extended)
			return &k;
	}
	for(const KeyEntry &k : g_keys)
	{
		if(k.vk == vk)
			return &k;
	}
	return nullptr;
}

// evdev codes beyond the shared main block, as Windows scan codes.
struct EvdevExtra
{
	quint32 evdev;
	UINT scan;
	bool extended;
};
const EvdevExtra g_evdevExtra[] = {
	{96, 0x1C, true}, // KP Enter
	{97, 0x1D, true}, // Right Ctrl
	{98, 0x35, true}, // KP /
	{99, 0x37, true}, // SysRq / Print
	{100, 0x38, true}, // Right Alt
	{102, 0x47, true}, // Home
	{103, 0x48, true}, // Up
	{104, 0x49, true}, // Page Up
	{105, 0x4B, true}, // Left
	{106, 0x4D, true}, // Right
	{107, 0x4F, true}, // End
	{108, 0x50, true}, // Down
	{109, 0x51, true}, // Page Down
	{110, 0x52, true}, // Insert
	{111, 0x53, true}, // Delete
	{119, 0x45, false}, // Pause
	{125, 0x5B, true}, // Left Meta
	{126, 0x5C, true}, // Right Meta
	{127, 0x5D, true}, // Compose / Menu
	{183, 0x64, false}, {184, 0x65, false}, {185, 0x66, false}, {186, 0x67, false},
	{187, 0x68, false}, {188, 0x69, false}, {189, 0x6A, false}, {190, 0x6B, false},
	{191, 0x6C, false}, {192, 0x6D, false}, {193, 0x6E, false}, {194, 0x76, false},
};

bool HasEvdevScanCodes()
{
	static const bool evdev = []
	{
		const QString platform = QGuiApplication::platformName();
		return platform == QLatin1String("xcb") || platform.startsWith(QLatin1String("wayland")) || platform == QLatin1String("eglfs") || platform == QLatin1String("linuxfb");
	}();
	return evdev;
}

bool ScanFromNative(quint32 native, UINT &scan, bool &extended)
{
	if(!HasEvdevScanCodes() || native < 8)
		return false;
	const quint32 evdev = native - 8;
	if(evdev == 69)
	{
		scan = 0x45; // Num Lock
		extended = true;
		return true;
	}
	if(evdev >= 1 && evdev <= 88)
	{
		scan = evdev;
		extended = false;
		return true;
	}
	for(const EvdevExtra &e : g_evdevExtra)
	{
		if(e.evdev == evdev)
		{
			scan = e.scan;
			extended = e.extended;
			return true;
		}
	}
	return false;
}

UINT KeypadVK(int qtKey)
{
	if(qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
		return VK_NUMPAD0 + (qtKey - Qt::Key_0);
	switch(qtKey)
	{
	case Qt::Key_Asterisk: return VK_MULTIPLY;
	case Qt::Key_Plus: return VK_ADD;
	case Qt::Key_Minus: return VK_SUBTRACT;
	case Qt::Key_Period:
	case Qt::Key_Comma: return VK_DECIMAL;
	case Qt::Key_Slash: return VK_DIVIDE;
	case Qt::Key_Clear: return VK_CLEAR;
	default: return 0;
	}
}

// For characters, the US key that types them.
UINT VKForChar(wchar_t c, bool *shift = nullptr)
{
	const wchar_t upper = static_cast<wchar_t>(std::towupper(c));
	if(upper >= L'A' && upper <= L'Z')
	{
		if(shift)
			*shift = (c == upper);
		return upper;
	}
	for(const KeyEntry &k : g_keys)
	{
		if(k.extended || k.normal == 0)
			continue;
		if(k.vk >= VK_NUMPAD0 && k.vk <= VK_DIVIDE)
			continue;
		if(k.normal == c || k.shifted == c)
		{
			if(shift)
				*shift = (k.normal != c);
			return k.vk;
		}
	}
	return 0;
}

UINT VKFromQtKey(int qtKey)
{
	if(qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
		return 'A' + (qtKey - Qt::Key_A);
	if(qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
		return '0' + (qtKey - Qt::Key_0);
	if(qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
		return VK_F1 + (qtKey - Qt::Key_F1);
	switch(qtKey)
	{
	case Qt::Key_Escape: return VK_ESCAPE;
	case Qt::Key_Tab:
	case Qt::Key_Backtab: return VK_TAB;
	case Qt::Key_Backspace: return VK_BACK;
	case Qt::Key_Return:
	case Qt::Key_Enter: return VK_RETURN;
	case Qt::Key_Insert: return VK_INSERT;
	case Qt::Key_Delete: return VK_DELETE;
	case Qt::Key_Pause: return VK_PAUSE;
	case Qt::Key_Print: return VK_SNAPSHOT;
	case Qt::Key_Clear: return VK_CLEAR;
	case Qt::Key_Home: return VK_HOME;
	case Qt::Key_End: return VK_END;
	case Qt::Key_Left: return VK_LEFT;
	case Qt::Key_Up: return VK_UP;
	case Qt::Key_Right: return VK_RIGHT;
	case Qt::Key_Down: return VK_DOWN;
	case Qt::Key_PageUp: return VK_PRIOR;
	case Qt::Key_PageDown: return VK_NEXT;
	case Qt::Key_Shift: return VK_SHIFT;
	case Qt::Key_Control: return VK_CONTROL;
	case Qt::Key_Meta: return VK_LWIN;
	case Qt::Key_Alt:
	case Qt::Key_AltGr: return VK_MENU;
	case Qt::Key_CapsLock: return VK_CAPITAL;
	case Qt::Key_NumLock: return VK_NUMLOCK;
	case Qt::Key_ScrollLock: return VK_SCROLL;
	case Qt::Key_Menu: return VK_APPS;
	case Qt::Key_Space: return VK_SPACE;
	default:
		break;
	}
	if(qtKey > 0 && qtKey < 0x10000)
		return VKForChar(static_cast<wchar_t>(qtKey));
	return 0;
}

// Key state as GetKeyboardState() returns it: bit 7 down, bit 0 toggled.
std::array<BYTE, 256> g_keyState{};

} // namespace

KeyInfo KeyFromQt(const QKeyEvent *e)
{
	KeyInfo info;
	const int qtKey = e->key();
	const bool keypad = (e->modifiers() & Qt::KeypadModifier) != 0;
	const bool haveScan = ScanFromNative(e->nativeScanCode(), info.scanCode, info.extended);
	if(keypad)
		info.vk = KeypadVK(qtKey);
	if(info.vk == 0 && qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
		info.vk = 'A' + (qtKey - Qt::Key_A);
	if(info.vk == 0 && haveScan)
	{
		if(const KeyEntry *k = FindScan(info.scanCode, info.extended))
		{
			// Keypad navigation keys (Num Lock off) keep Qt's meaning.
			if(!(keypad && !info.extended && k->vk >= VK_NUMPAD0 && k->vk <= VK_DECIMAL))
				info.vk = k->vk;
		}
	}
	if(info.vk == 0)
		info.vk = VKFromQtKey(qtKey);
	if(!haveScan && info.vk != 0)
	{
		if(const KeyEntry *k = FindVK(info.vk))
		{
			info.scanCode = k->scan;
			info.extended = k->extended;
		}
		switch(info.vk)
		{
		case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
		case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
			// The dedicated keys, not the keypad ones.
			if(!keypad)
				info.extended = true;
			break;
		default:
			break;
		}
	}
	return info;
}

void SetKeyState(UINT vk, bool down, UINT scanCode, bool extended)
{
	vk &= 0xFF;
	if(down)
	{
		if(!(g_keyState[vk] & 0x80))
			g_keyState[vk] ^= 0x01; // toggles on each press
		g_keyState[vk] |= 0x80;
	} else
	{
		g_keyState[vk] &= ~0x80;
	}
	// Left / right variants.
	UINT side = 0;
	switch(vk)
	{
	case VK_SHIFT: side = (scanCode == 0x36) ? VK_RSHIFT : VK_LSHIFT; break;
	case VK_CONTROL: side = extended ? VK_RCONTROL : VK_LCONTROL; break;
	case VK_MENU: side = extended ? VK_RMENU : VK_LMENU; break;
	default: break;
	}
	if(side)
	{
		if(down)
			g_keyState[side] |= 0x80;
		else
			g_keyState[side] &= ~0x80;
		if(!down)
		{
			// The generic key is up when both sides are.
			const UINT l = (vk == VK_SHIFT) ? VK_LSHIFT : (vk == VK_CONTROL) ? VK_LCONTROL : VK_LMENU;
			if((g_keyState[l] | g_keyState[l + 1]) & 0x80)
				g_keyState[vk] |= 0x80;
		}
	}
}

void ResetKeyState()
{
	for(auto &b : g_keyState)
		b &= 0x01;
}

WPARAM MouseKeyFlags(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
	WPARAM flags = 0;
	if(buttons & Qt::LeftButton)
		flags |= MK_LBUTTON;
	if(buttons & Qt::RightButton)
		flags |= MK_RBUTTON;
	if(buttons & Qt::MiddleButton)
		flags |= MK_MBUTTON;
	if(buttons & Qt::BackButton)
		flags |= MK_XBUTTON1;
	if(buttons & Qt::ForwardButton)
		flags |= MK_XBUTTON2;
	if(modifiers & Qt::ShiftModifier)
		flags |= MK_SHIFT;
	if(modifiers & Qt::ControlModifier)
		flags |= MK_CONTROL;
	return flags;
}

} // namespace linopenmpt::qtwin

using namespace linopenmpt::qtwin;

SHORT GetKeyState(int vKey)
{
	const BYTE s = g_keyState[vKey & 0xFF];
	return static_cast<SHORT>(((s & 0x80) ? 0x8000 : 0) | (s & 0x01));
}

SHORT GetAsyncKeyState(int vKey)
{
	// The physical state where Qt knows it.
	bool down = (g_keyState[vKey & 0xFF] & 0x80) != 0;
	const Qt::MouseButtons buttons = QGuiApplication::mouseButtons();
	const Qt::KeyboardModifiers modifiers = QGuiApplication::queryKeyboardModifiers();
	switch(vKey)
	{
	case VK_LBUTTON: down = (buttons & Qt::LeftButton) != 0; break;
	case VK_RBUTTON: down = (buttons & Qt::RightButton) != 0; break;
	case VK_MBUTTON: down = (buttons & Qt::MiddleButton) != 0; break;
	case VK_SHIFT: down = (modifiers & Qt::ShiftModifier) != 0; break;
	case VK_CONTROL: down = (modifiers & Qt::ControlModifier) != 0; break;
	case VK_MENU: down = (modifiers & Qt::AltModifier) != 0; break;
	default: break;
	}
	return static_cast<SHORT>(down ? 0x8000 : 0);
}

BOOL GetKeyboardState(PBYTE keyState)
{
	if(keyState == nullptr)
		return FALSE;
	std::copy(g_keyState.begin(), g_keyState.end(), keyState);
	return TRUE;
}

UINT MapVirtualKeyEx(UINT code, UINT mapType, HKL)
{
	switch(mapType)
	{
	case MAPVK_VK_TO_VSC:
		{
			if(code == VK_LSHIFT)
				return 0x2A;
			if(code == VK_RSHIFT)
				return 0x36;
			if(code == VK_LCONTROL || code == VK_RCONTROL)
				return 0x1D;
			if(code == VK_LMENU || code == VK_RMENU)
				return 0x38;
			const KeyEntry *k = FindVK(code);
			return k ? k->scan : 0;
		}
	case MAPVK_VSC_TO_VK:
	case MAPVK_VSC_TO_VK_EX:
		{
			const KeyEntry *k = FindScan(code & 0xFF, (code & 0xE000) != 0);
			if(k == nullptr)
				return 0;
			if(mapType == MAPVK_VSC_TO_VK_EX)
			{
				if(k->vk == VK_SHIFT)
					return k->scan == 0x36 ? VK_RSHIFT : VK_LSHIFT;
				if(k->vk == VK_CONTROL)
					return k->extended ? VK_RCONTROL : VK_LCONTROL;
				if(k->vk == VK_MENU)
					return k->extended ? VK_RMENU : VK_LMENU;
			}
			return k->vk;
		}
	case MAPVK_VK_TO_CHAR:
		{
			const KeyEntry *k = FindVK(code);
			if(k == nullptr || k->normal < 0x20)
				return 0;
			return static_cast<UINT>(std::towupper(k->normal));
		}
	default:
		return 0;
	}
}

UINT MapVirtualKey(UINT code, UINT mapType)
{
	return MapVirtualKeyEx(code, mapType, nullptr);
}

SHORT VkKeyScanW(WCHAR ch)
{
	bool shift = false;
	const UINT vk = VKForChar(ch, &shift);
	if(vk == 0)
		return -1;
	return static_cast<SHORT>(vk | (shift ? 0x100 : 0));
}

SHORT VkKeyScanEx(TCHAR ch, HKL)
{
	return VkKeyScanW(ch);
}

HKL GetKeyboardLayout(DWORD)
{
	// US English, which the key tables above describe.
	return reinterpret_cast<HKL>(static_cast<UINT_PTR>(0x04090409));
}

int GetKeyboardLayoutList(int count, HKL *list)
{
	if(count > 0 && list)
		list[0] = GetKeyboardLayout(0);
	return 1;
}

int GetKeyNameText(LONG lParam, LPTSTR buffer, int size)
{
	if(buffer == nullptr || size <= 0)
		return 0;
	const UINT scan = (static_cast<UINT>(lParam) >> 16) & 0xFF;
	const bool extended = (lParam & (1 << 24)) != 0;
	const bool dontCare = (lParam & (1 << 25)) != 0;
	const KeyEntry *k = FindScan(scan, extended);
	if(k == nullptr && extended)
		k = FindScan(scan, false);
	if(k == nullptr)
	{
		buffer[0] = 0;
		return 0;
	}
	const wchar_t *name = k->name;
	if(dontCare)
	{
		if(k->vk == VK_SHIFT)
			name = L"Shift";
		else if(k->vk == VK_CONTROL)
			name = L"Ctrl";
		else if(k->vk == VK_MENU)
			name = L"Alt";
	}
	const int n = std::min<int>(static_cast<int>(std::wcslen(name)), size - 1);
	std::wmemcpy(buffer, name, n);
	buffer[n] = 0;
	return n;
}

int ToUnicodeEx(UINT vk, UINT, const BYTE *keyState, LPWSTR buf, int bufSize, UINT, HKL)
{
	if(buf == nullptr || bufSize <= 0)
		return 0;
	const KeyEntry *k = FindVK(vk);
	if(k == nullptr || k->normal == 0)
		return 0;
	const bool shift = keyState && (keyState[VK_SHIFT] & 0x80);
	const bool caps = keyState && (keyState[VK_CAPITAL] & 0x01);
	wchar_t c = shift ? k->shifted : k->normal;
	if(caps && vk >= 'A' && vk <= 'Z')
		c = shift ? k->normal : k->shifted;
	buf[0] = c;
	if(bufSize > 1)
		buf[1] = 0;
	return 1;
}

UINT SendInput(UINT count, INPUT *, int)
{
	// Upstream only synthesizes input to keep Windows' Start menu and lock
	// key lights from reacting to its shortcuts; neither applies here.
	return count;
}
