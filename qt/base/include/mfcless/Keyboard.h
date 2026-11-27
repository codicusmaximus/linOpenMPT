/*
 * Keyboard.h
 * ----------
 * Purpose: Keyboard and mouse state in the terms upstream's key handling
 *          uses: virtual-key codes and scan codes as Windows numbers them, so
 *          that key maps (.mkb files) stay the same on every platform.
 * Notes  : Qt key events become key codes this way:
 *           - letters follow the keyboard layout (Qt's key);
 *           - every other key is identified by its position, so digits,
 *             punctuation and the pattern editor's note keys sit where they
 *             would on a Windows keyboard. On Linux (X11 and Wayland) Qt's
 *             native scan code is the evdev code plus 8, and evdev codes of
 *             the main block equal Windows' set-1 scan codes;
 *           - keypad keys follow Num Lock, through Qt's key.
 *          Without native scan codes (offscreen tests, other platforms), Qt's
 *          key alone decides.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include <QtCore/QString>
#include <QtCore/Qt>

#include <cstdint>

class QEvent;
class QKeyEvent;
class QObject;

namespace mfcless
{

// Virtual-key codes, by Windows' numbering (kept as data: key maps store them).
// Letters and digits are their upper-case ASCII codes.
namespace vk
{
	inline constexpr uint32_t
		LBUTTON = 0x01, RBUTTON = 0x02, CANCEL = 0x03, MBUTTON = 0x04, XBUTTON1 = 0x05, XBUTTON2 = 0x06,
		BACK = 0x08, TAB = 0x09, CLEAR = 0x0C, RETURN = 0x0D,
		SHIFT = 0x10, CONTROL = 0x11, MENU = 0x12, PAUSE = 0x13, CAPITAL = 0x14,
		ESCAPE = 0x1B, SPACE = 0x20, PRIOR = 0x21, NEXT = 0x22, END = 0x23, HOME = 0x24,
		LEFT = 0x25, UP = 0x26, RIGHT = 0x27, DOWN = 0x28, SNAPSHOT = 0x2C, INSERT = 0x2D, DELETE = 0x2E,
		A = 0x41, C = 0x43, N = 0x4E, V = 0x56, X = 0x58, Y = 0x59, Z = 0x5A,
		LWIN = 0x5B, RWIN = 0x5C, APPS = 0x5D,
		NUMPAD0 = 0x60, NUMPAD1 = 0x61, NUMPAD2 = 0x62, NUMPAD3 = 0x63, NUMPAD4 = 0x64,
		NUMPAD5 = 0x65, NUMPAD6 = 0x66, NUMPAD7 = 0x67, NUMPAD8 = 0x68, NUMPAD9 = 0x69,
		MULTIPLY = 0x6A, ADD = 0x6B, SEPARATOR = 0x6C, SUBTRACT = 0x6D, DECIMAL = 0x6E, DIVIDE = 0x6F,
		F1 = 0x70, F2 = 0x71, F3 = 0x72, F4 = 0x73, F5 = 0x74, F6 = 0x75, F7 = 0x76, F8 = 0x77,
		F9 = 0x78, F10 = 0x79, F11 = 0x7A, F12 = 0x7B, F13 = 0x7C, F14 = 0x7D, F15 = 0x7E, F16 = 0x7F,
		F17 = 0x80, F18 = 0x81, F19 = 0x82, F20 = 0x83, F21 = 0x84, F22 = 0x85, F23 = 0x86, F24 = 0x87,
		NUMLOCK = 0x90, SCROLL = 0x91,
		LSHIFT = 0xA0, RSHIFT = 0xA1, LCONTROL = 0xA2, RCONTROL = 0xA3, LMENU = 0xA4, RMENU = 0xA5,
		BROWSER_BACK = 0xA6, BROWSER_FORWARD = 0xA7,
		OEM_1 = 0xBA, OEM_PLUS = 0xBB, OEM_COMMA = 0xBC, OEM_MINUS = 0xBD, OEM_PERIOD = 0xBE, OEM_2 = 0xBF, OEM_3 = 0xC0,
		OEM_4 = 0xDB, OEM_5 = 0xDC, OEM_6 = 0xDD, OEM_7 = 0xDE, OEM_8 = 0xDF, OEM_102 = 0xE2,
		PACKET = 0xE7;
}

// The keys and buttons held during a mouse event (Windows' MK_ flags).
namespace MouseKey
{
	inline constexpr uint32_t Left = 0x01, Right = 0x02, Shift = 0x04, Control = 0x08, Middle = 0x10, X1 = 0x20, X2 = 0x40;
}

// Which extra button an OnXButton handler reports (back = 1, forward = 2).
namespace XButton
{
	inline constexpr uint32_t Back = 1, Forward = 2;
}

// One notch of a mouse wheel.
inline constexpr int WheelDelta = 120;

// Window message numbers, as Windows numbers them, for the keyboard and
// mouse messages that upstream's key handling looks at.
namespace msg
{
	inline constexpr uint32_t KeyDown = 0x0100, KeyUp = 0x0101, Char = 0x0102, SysKeyDown = 0x0104, SysKeyUp = 0x0105, SysChar = 0x0106,
		MouseMove = 0x0200, LButtonDown = 0x0201, LButtonUp = 0x0202, LButtonDblClk = 0x0203,
		RButtonDown = 0x0204, RButtonUp = 0x0205, RButtonDblClk = 0x0206, MButtonDown = 0x0207, MButtonUp = 0x0208,
		MouseWheel = 0x020A, XButtonDown = 0x020B, XButtonUp = 0x020C, MouseHWheel = 0x020E,
		KeyFirst = 0x0100, KeyLast = 0x0109, MouseFirst = 0x0200, MouseLast = 0x020E;
}

// A keyboard or mouse event in the terms of a window message, for
// PreTranslateMessage and the input handler: for key messages wParam is the
// key code and lParam holds the repeat count (low word) and the flags (high
// word: scan code, extended bit 0x100, Alt 0x2000, previous state 0x4000,
// transition 0x8000).
struct Message
{
	QObject *hwnd = nullptr;  // the receiver
	uint32_t message = 0;
	uintptr_t wParam = 0;
	intptr_t lParam = 0;
	QEvent *event = nullptr;  // the Qt event it stands for
};

struct KeyInfo
{
	uint32_t vk = 0;
	uint32_t scanCode = 0;
	bool extended = false;
};

KeyInfo KeyFromQt(const QKeyEvent *e);
// The key message (KeyDown, KeyUp, SysKeyDown or SysKeyUp) of a Qt key
// event. Qt reports a held key as releases and presses; for the releases
// (isAutoRepeat) message is 0.
Message KeyMessage(QKeyEvent *e, QObject *receiver = nullptr);

// The key state the program has seen, as Windows keeps it.
void SetKeyState(uint32_t vk, bool down, uint32_t scanCode, bool extended);
void SyncModifierState(Qt::KeyboardModifiers modifiers);
void ResetKeyState();

// Bit 15 set while the key is down, bit 0 toggled on each press.
int16_t KeyState(uint32_t vk);
inline bool IsKeyDown(uint32_t vk) { return KeyState(vk) < 0; }
// The physical state, where Qt knows it.
bool IsKeyDownNow(uint32_t vk);
// 256 entries, as GetKeyboardState fills them.
void GetKeyboardState(uint8_t *keyState);

uint32_t MouseKeyFlags(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);

// Conversions by the US layout, the reference key maps use.
uint32_t VkToScan(uint32_t vk);
// extendedScan has 0xE0 in its high byte for extended keys.
uint32_t ScanToVk(uint32_t extendedScan, bool distinguishSides = false);
// The key code typing ch, with 0x100 if Shift is needed; -1 if none.
int VkFromChar(char16_t ch);
// The character a key types, or 0.
char16_t CharFromVk(uint32_t vk, bool shift, bool capsLock = false);
// "Page Up", "Num 7", ...
QString KeyName(uint32_t vk, bool extended);

} // namespace mfcless
