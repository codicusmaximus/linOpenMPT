/*
 * Keyboard.cpp
 * ------------
 * Purpose: Key codes from Qt key events, and the key state.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/Keyboard.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>

#include <algorithm>
#include <array>

namespace mfcless
{

namespace
{

// The US layout: the reference key names and characters use.
struct KeyEntry
{
	uint32_t scan;
	bool extended;
	uint32_t vk;
	const char16_t *name;
	char16_t normal;
	char16_t shifted;
};

const KeyEntry g_keys[] = {
	{0x01, false, vk::ESCAPE, u"Esc", 0x1B, 0x1B},
	{0x02, false, '1', u"1", u'1', u'!'},
	{0x03, false, '2', u"2", u'2', u'@'},
	{0x04, false, '3', u"3", u'3', u'#'},
	{0x05, false, '4', u"4", u'4', u'$'},
	{0x06, false, '5', u"5", u'5', u'%'},
	{0x07, false, '6', u"6", u'6', u'^'},
	{0x08, false, '7', u"7", u'7', u'&'},
	{0x09, false, '8', u"8", u'8', u'*'},
	{0x0A, false, '9', u"9", u'9', u'('},
	{0x0B, false, '0', u"0", u'0', u')'},
	{0x0C, false, vk::OEM_MINUS, u"-", u'-', u'_'},
	{0x0D, false, vk::OEM_PLUS, u"=", u'=', u'+'},
	{0x0E, false, vk::BACK, u"Backspace", u'\b', u'\b'},
	{0x0F, false, vk::TAB, u"Tab", u'\t', u'\t'},
	{0x10, false, 'Q', u"Q", u'q', u'Q'},
	{0x11, false, 'W', u"W", u'w', u'W'},
	{0x12, false, 'E', u"E", u'e', u'E'},
	{0x13, false, 'R', u"R", u'r', u'R'},
	{0x14, false, 'T', u"T", u't', u'T'},
	{0x15, false, 'Y', u"Y", u'y', u'Y'},
	{0x16, false, 'U', u"U", u'u', u'U'},
	{0x17, false, 'I', u"I", u'i', u'I'},
	{0x18, false, 'O', u"O", u'o', u'O'},
	{0x19, false, 'P', u"P", u'p', u'P'},
	{0x1A, false, vk::OEM_4, u"[", u'[', u'{'},
	{0x1B, false, vk::OEM_6, u"]", u']', u'}'},
	{0x1C, false, vk::RETURN, u"Enter", u'\r', u'\r'},
	{0x1C, true, vk::RETURN, u"Num Enter", u'\r', u'\r'},
	{0x1D, false, vk::CONTROL, u"Ctrl", 0, 0},
	{0x1D, true, vk::CONTROL, u"Right Ctrl", 0, 0},
	{0x1E, false, 'A', u"A", u'a', u'A'},
	{0x1F, false, 'S', u"S", u's', u'S'},
	{0x20, false, 'D', u"D", u'd', u'D'},
	{0x21, false, 'F', u"F", u'f', u'F'},
	{0x22, false, 'G', u"G", u'g', u'G'},
	{0x23, false, 'H', u"H", u'h', u'H'},
	{0x24, false, 'J', u"J", u'j', u'J'},
	{0x25, false, 'K', u"K", u'k', u'K'},
	{0x26, false, 'L', u"L", u'l', u'L'},
	{0x27, false, vk::OEM_1, u";", u';', u':'},
	{0x28, false, vk::OEM_7, u"'", u'\'', u'"'},
	{0x29, false, vk::OEM_3, u"`", u'`', u'~'},
	{0x2A, false, vk::SHIFT, u"Shift", 0, 0},
	{0x2B, false, vk::OEM_5, u"\\", u'\\', u'|'},
	{0x2C, false, 'Z', u"Z", u'z', u'Z'},
	{0x2D, false, 'X', u"X", u'x', u'X'},
	{0x2E, false, 'C', u"C", u'c', u'C'},
	{0x2F, false, 'V', u"V", u'v', u'V'},
	{0x30, false, 'B', u"B", u'b', u'B'},
	{0x31, false, 'N', u"N", u'n', u'N'},
	{0x32, false, 'M', u"M", u'm', u'M'},
	{0x33, false, vk::OEM_COMMA, u",", u',', u'<'},
	{0x34, false, vk::OEM_PERIOD, u".", u'.', u'>'},
	{0x35, false, vk::OEM_2, u"/", u'/', u'?'},
	{0x35, true, vk::DIVIDE, u"Num /", u'/', u'/'},
	{0x36, false, vk::SHIFT, u"Right Shift", 0, 0},
	{0x37, false, vk::MULTIPLY, u"Num *", u'*', u'*'},
	{0x37, true, vk::SNAPSHOT, u"Prnt Scrn", 0, 0},
	{0x38, false, vk::MENU, u"Alt", 0, 0},
	{0x38, true, vk::MENU, u"Right Alt", 0, 0},
	{0x39, false, vk::SPACE, u"Space", u' ', u' '},
	{0x3A, false, vk::CAPITAL, u"Caps Lock", 0, 0},
	{0x3B, false, vk::F1, u"F1", 0, 0},
	{0x3C, false, vk::F2, u"F2", 0, 0},
	{0x3D, false, vk::F3, u"F3", 0, 0},
	{0x3E, false, vk::F4, u"F4", 0, 0},
	{0x3F, false, vk::F5, u"F5", 0, 0},
	{0x40, false, vk::F6, u"F6", 0, 0},
	{0x41, false, vk::F7, u"F7", 0, 0},
	{0x42, false, vk::F8, u"F8", 0, 0},
	{0x43, false, vk::F9, u"F9", 0, 0},
	{0x44, false, vk::F10, u"F10", 0, 0},
	{0x45, false, vk::PAUSE, u"Pause", 0, 0},
	{0x45, true, vk::NUMLOCK, u"Num Lock", 0, 0},
	{0x46, false, vk::SCROLL, u"Scroll Lock", 0, 0},
	{0x47, false, vk::NUMPAD7, u"Num 7", u'7', u'7'},
	{0x47, true, vk::HOME, u"Home", 0, 0},
	{0x48, false, vk::NUMPAD8, u"Num 8", u'8', u'8'},
	{0x48, true, vk::UP, u"Up", 0, 0},
	{0x49, false, vk::NUMPAD9, u"Num 9", u'9', u'9'},
	{0x49, true, vk::PRIOR, u"Page Up", 0, 0},
	{0x4A, false, vk::SUBTRACT, u"Num -", u'-', u'-'},
	{0x4B, false, vk::NUMPAD4, u"Num 4", u'4', u'4'},
	{0x4B, true, vk::LEFT, u"Left", 0, 0},
	{0x4C, false, vk::NUMPAD5, u"Num 5", u'5', u'5'},
	{0x4D, false, vk::NUMPAD6, u"Num 6", u'6', u'6'},
	{0x4D, true, vk::RIGHT, u"Right", 0, 0},
	{0x4E, false, vk::ADD, u"Num +", u'+', u'+'},
	{0x4F, false, vk::NUMPAD1, u"Num 1", u'1', u'1'},
	{0x4F, true, vk::END, u"End", 0, 0},
	{0x50, false, vk::NUMPAD2, u"Num 2", u'2', u'2'},
	{0x50, true, vk::DOWN, u"Down", 0, 0},
	{0x51, false, vk::NUMPAD3, u"Num 3", u'3', u'3'},
	{0x51, true, vk::NEXT, u"Page Down", 0, 0},
	{0x52, false, vk::NUMPAD0, u"Num 0", u'0', u'0'},
	{0x52, true, vk::INSERT, u"Insert", 0, 0},
	{0x53, false, vk::DECIMAL, u"Num Del", u'.', u'.'},
	{0x53, true, vk::DELETE, u"Delete", 0, 0},
	{0x56, false, vk::OEM_102, u"\\", u'\\', u'|'},
	{0x57, false, vk::F11, u"F11", 0, 0},
	{0x58, false, vk::F12, u"F12", 0, 0},
	{0x5B, true, vk::LWIN, u"Left Windows", 0, 0},
	{0x5C, true, vk::RWIN, u"Right Windows", 0, 0},
	{0x5D, true, vk::APPS, u"Application", 0, 0},
	{0x64, false, vk::F13, u"F13", 0, 0},
	{0x65, false, vk::F14, u"F14", 0, 0},
	{0x66, false, vk::F15, u"F15", 0, 0},
	{0x67, false, vk::F16, u"F16", 0, 0},
	{0x68, false, vk::F17, u"F17", 0, 0},
	{0x69, false, vk::F18, u"F18", 0, 0},
	{0x6A, false, vk::F19, u"F19", 0, 0},
	{0x6B, false, vk::F20, u"F20", 0, 0},
	{0x6C, false, vk::F21, u"F21", 0, 0},
	{0x6D, false, vk::F22, u"F22", 0, 0},
	{0x6E, false, vk::F23, u"F23", 0, 0},
	{0x76, false, vk::F24, u"F24", 0, 0},
};

const KeyEntry *FindScan(uint32_t scan, bool extended)
{
	for(const KeyEntry &k : g_keys)
	{
		if(k.scan == scan && k.extended == extended)
			return &k;
	}
	return nullptr;
}

const KeyEntry *FindVK(uint32_t code)
{
	// Prefer the main-block key (e.g. Enter, not Num Enter).
	for(const KeyEntry &k : g_keys)
	{
		if(k.vk == code && !k.extended)
			return &k;
	}
	for(const KeyEntry &k : g_keys)
	{
		if(k.vk == code)
			return &k;
	}
	return nullptr;
}

const KeyEntry *FindVK(uint32_t code, bool extended)
{
	for(const KeyEntry &k : g_keys)
	{
		if(k.vk == code && k.extended == extended)
			return &k;
	}
	return FindVK(code);
}

// evdev codes beyond the shared main block, as Windows scan codes.
struct EvdevExtra
{
	quint32 evdev;
	uint32_t scan;
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

bool ScanFromNative(quint32 native, uint32_t &scan, bool &extended)
{
	if(!HasEvdevScanCodes() || native < 8)
		return false;
	const quint32 evdev = native - 8;
	if(evdev == 69)
	{
		scan = 0x45;  // Num Lock
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

uint32_t KeypadVK(int qtKey)
{
	if(qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
		return vk::NUMPAD0 + (qtKey - Qt::Key_0);
	switch(qtKey)
	{
	case Qt::Key_Asterisk: return vk::MULTIPLY;
	case Qt::Key_Plus: return vk::ADD;
	case Qt::Key_Minus: return vk::SUBTRACT;
	case Qt::Key_Period:
	case Qt::Key_Comma: return vk::DECIMAL;
	case Qt::Key_Slash: return vk::DIVIDE;
	case Qt::Key_Clear: return vk::CLEAR;
	default: return 0;
	}
}

char16_t Upper(char16_t c)
{
	return (c >= u'a' && c <= u'z') ? static_cast<char16_t>(c - u'a' + u'A') : c;
}

// For characters, the US key that types them.
uint32_t VKForChar(char16_t c, bool *shift = nullptr)
{
	const char16_t upper = Upper(c);
	if(upper >= u'A' && upper <= u'Z')
	{
		if(shift)
			*shift = (c == upper);
		return upper;
	}
	for(const KeyEntry &k : g_keys)
	{
		if(k.extended || k.normal == 0)
			continue;
		if(k.vk >= vk::NUMPAD0 && k.vk <= vk::DIVIDE)
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

uint32_t VKFromQtKey(int qtKey)
{
	if(qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
		return 'A' + (qtKey - Qt::Key_A);
	if(qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
		return '0' + (qtKey - Qt::Key_0);
	if(qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
		return vk::F1 + (qtKey - Qt::Key_F1);
	switch(qtKey)
	{
	case Qt::Key_Escape: return vk::ESCAPE;
	case Qt::Key_Tab:
	case Qt::Key_Backtab: return vk::TAB;
	case Qt::Key_Backspace: return vk::BACK;
	case Qt::Key_Return:
	case Qt::Key_Enter: return vk::RETURN;
	case Qt::Key_Insert: return vk::INSERT;
	case Qt::Key_Delete: return vk::DELETE;
	case Qt::Key_Pause: return vk::PAUSE;
	case Qt::Key_Print: return vk::SNAPSHOT;
	case Qt::Key_Clear: return vk::CLEAR;
	case Qt::Key_Home: return vk::HOME;
	case Qt::Key_End: return vk::END;
	case Qt::Key_Left: return vk::LEFT;
	case Qt::Key_Up: return vk::UP;
	case Qt::Key_Right: return vk::RIGHT;
	case Qt::Key_Down: return vk::DOWN;
	case Qt::Key_PageUp: return vk::PRIOR;
	case Qt::Key_PageDown: return vk::NEXT;
	case Qt::Key_Shift: return vk::SHIFT;
	case Qt::Key_Control: return vk::CONTROL;
	case Qt::Key_Meta: return vk::LWIN;
	case Qt::Key_Alt:
	case Qt::Key_AltGr: return vk::MENU;
	case Qt::Key_CapsLock: return vk::CAPITAL;
	case Qt::Key_NumLock: return vk::NUMLOCK;
	case Qt::Key_ScrollLock: return vk::SCROLL;
	case Qt::Key_Menu: return vk::APPS;
	case Qt::Key_Space: return vk::SPACE;
	case Qt::Key_Back: return vk::BROWSER_BACK;
	case Qt::Key_Forward: return vk::BROWSER_FORWARD;
	default:
		break;
	}
	if(qtKey > 0 && qtKey < 0x10000)
		return VKForChar(static_cast<char16_t>(qtKey));
	return 0;
}

// Bit 7 down, bit 0 toggled.
std::array<uint8_t, 256> g_keyState{};

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
			if(!(keypad && !info.extended && k->vk >= vk::NUMPAD0 && k->vk <= vk::DECIMAL))
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
		case vk::INSERT: case vk::DELETE: case vk::HOME: case vk::END: case vk::PRIOR: case vk::NEXT:
		case vk::LEFT: case vk::RIGHT: case vk::UP: case vk::DOWN:
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

Message KeyMessage(QKeyEvent *e, QObject *receiver)
{
	Message m;
	m.hwnd = receiver;
	m.event = e;
	const bool release = (e->type() == QEvent::KeyRelease);
	if(release && e->isAutoRepeat())
		return m;
	const KeyInfo info = KeyFromQt(e);
	uint32_t key = info.vk;
	if(key == 0 && !e->text().isEmpty())
	{
		// A key without a key code, typing a character (input methods, remote
		// desktops): the key that types that character.
		const int code = VkFromChar(e->text().at(0).unicode());
		if(code != -1)
			key = code & 0xFF;
	}
	const bool alt = (e->modifiers() & Qt::AltModifier) && !(e->modifiers() & Qt::ControlModifier);
	uint32_t flags = (info.scanCode & 0xFF) | (info.extended ? 0x100 : 0) | (alt ? 0x2000 : 0);
	if(release)
		flags |= 0x8000 | 0x4000;
	else if(e->isAutoRepeat())
		flags |= 0x4000;
	// F10 and Alt combinations are system keys on Windows.
	const bool sys = alt || key == vk::F10 || key == vk::MENU;
	m.message = release ? (sys ? msg::SysKeyUp : msg::KeyUp) : (sys ? msg::SysKeyDown : msg::KeyDown);
	m.wParam = key;
	const uint32_t repeat = static_cast<uint32_t>(std::max(1, e->count()));
	m.lParam = static_cast<intptr_t>(static_cast<uint32_t>(repeat & 0xFFFF) | (flags << 16));
	return m;
}

void SyncModifierState(Qt::KeyboardModifiers modifiers)
{
	// A modifier pressed or released while another application had the
	// keyboard (or in synthesized events) is only seen in the modifiers.
	const struct { uint32_t vk; Qt::KeyboardModifier mod; uint32_t scan; } keys[] = {
		{vk::SHIFT, Qt::ShiftModifier, 0x2A},
		{vk::CONTROL, Qt::ControlModifier, 0x1D},
		{vk::MENU, Qt::AltModifier, 0x38},
	};
	for(const auto &k : keys)
	{
		const bool down = (g_keyState[k.vk] & 0x80) != 0;
		const bool want = (modifiers & k.mod) != 0;
		if(down != want)
			SetKeyState(k.vk, want, k.scan, false);
	}
}

void SetKeyState(uint32_t code, bool down, uint32_t scanCode, bool extended)
{
	code &= 0xFF;
	if(down)
	{
		if(!(g_keyState[code] & 0x80))
			g_keyState[code] ^= 0x01;  // toggles on each press
		g_keyState[code] |= 0x80;
	} else
	{
		g_keyState[code] &= ~0x80;
	}
	// Left / right variants.
	uint32_t side = 0;
	switch(code)
	{
	case vk::SHIFT: side = (scanCode == 0x36) ? vk::RSHIFT : vk::LSHIFT; break;
	case vk::CONTROL: side = extended ? vk::RCONTROL : vk::LCONTROL; break;
	case vk::MENU: side = extended ? vk::RMENU : vk::LMENU; break;
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
			const uint32_t l = (code == vk::SHIFT) ? vk::LSHIFT : (code == vk::CONTROL) ? vk::LCONTROL : vk::LMENU;
			if((g_keyState[l] | g_keyState[l + 1]) & 0x80)
				g_keyState[code] |= 0x80;
		}
	}
}

void ResetKeyState()
{
	for(auto &b : g_keyState)
		b &= 0x01;
}

int16_t KeyState(uint32_t code)
{
	const uint8_t s = g_keyState[code & 0xFF];
	return static_cast<int16_t>(((s & 0x80) ? 0x8000 : 0) | (s & 0x01));
}

bool IsKeyDownNow(uint32_t code)
{
	bool down = (g_keyState[code & 0xFF] & 0x80) != 0;
	const Qt::MouseButtons buttons = QGuiApplication::mouseButtons();
	const Qt::KeyboardModifiers modifiers = QGuiApplication::queryKeyboardModifiers();
	switch(code)
	{
	case vk::LBUTTON: down = (buttons & Qt::LeftButton) != 0; break;
	case vk::RBUTTON: down = (buttons & Qt::RightButton) != 0; break;
	case vk::MBUTTON: down = (buttons & Qt::MiddleButton) != 0; break;
	case vk::SHIFT: down = (modifiers & Qt::ShiftModifier) != 0; break;
	case vk::CONTROL: down = (modifiers & Qt::ControlModifier) != 0; break;
	case vk::MENU: down = (modifiers & Qt::AltModifier) != 0; break;
	default: break;
	}
	return down;
}

void GetKeyboardState(uint8_t *keyState)
{
	std::copy(g_keyState.begin(), g_keyState.end(), keyState);
}

uint32_t MouseKeyFlags(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
	uint32_t flags = 0;
	if(buttons & Qt::LeftButton)
		flags |= MouseKey::Left;
	if(buttons & Qt::RightButton)
		flags |= MouseKey::Right;
	if(buttons & Qt::MiddleButton)
		flags |= MouseKey::Middle;
	if(buttons & Qt::BackButton)
		flags |= MouseKey::X1;
	if(buttons & Qt::ForwardButton)
		flags |= MouseKey::X2;
	if(modifiers & Qt::ShiftModifier)
		flags |= MouseKey::Shift;
	if(modifiers & Qt::ControlModifier)
		flags |= MouseKey::Control;
	return flags;
}

uint32_t VkToScan(uint32_t code)
{
	switch(code)
	{
	case vk::LSHIFT: return 0x2A;
	case vk::RSHIFT: return 0x36;
	case vk::LCONTROL: case vk::RCONTROL: return 0x1D;
	case vk::LMENU: case vk::RMENU: return 0x38;
	default: break;
	}
	const KeyEntry *k = FindVK(code);
	return k ? k->scan : 0;
}

uint32_t ScanToVk(uint32_t extendedScan, bool distinguishSides)
{
	const KeyEntry *k = FindScan(extendedScan & 0xFF, (extendedScan & 0xE000) != 0);
	if(k == nullptr)
		return 0;
	if(distinguishSides)
	{
		if(k->vk == vk::SHIFT)
			return k->scan == 0x36 ? vk::RSHIFT : vk::LSHIFT;
		if(k->vk == vk::CONTROL)
			return k->extended ? vk::RCONTROL : vk::LCONTROL;
		if(k->vk == vk::MENU)
			return k->extended ? vk::RMENU : vk::LMENU;
	}
	return k->vk;
}

int VkFromChar(char16_t ch)
{
	bool shift = false;
	const uint32_t code = VKForChar(ch, &shift);
	if(code == 0)
		return -1;
	return static_cast<int>(code | (shift ? 0x100 : 0));
}

char16_t CharFromVk(uint32_t code, bool shift, bool capsLock)
{
	const KeyEntry *k = FindVK(code);
	if(k == nullptr || k->normal == 0)
		return 0;
	char16_t c = shift ? k->shifted : k->normal;
	if(capsLock && code >= 'A' && code <= 'Z')
		c = shift ? k->normal : k->shifted;
	return c;
}

QString KeyName(uint32_t code, bool extended)
{
	switch(code)
	{
	case vk::LSHIFT: return QStringLiteral("Shift");
	case vk::RSHIFT: return QStringLiteral("Right Shift");
	case vk::LCONTROL: return QStringLiteral("Ctrl");
	case vk::RCONTROL: return QStringLiteral("Right Ctrl");
	case vk::LMENU: return QStringLiteral("Alt");
	case vk::RMENU: return QStringLiteral("Right Alt");
	default: break;
	}
	const KeyEntry *k = FindVK(code, extended);
	if(k == nullptr)
		return {};
	return QString::fromUtf16(k->name);
}

} // namespace mfcless
