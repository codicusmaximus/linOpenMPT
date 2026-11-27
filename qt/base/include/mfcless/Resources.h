// mfcless base: the application's resources, read from the standard .res
// data that qt/resources/ compiles from upstream's mptrack.rc and embeds.
//
// Upstream's code names resources by their IDs from resource.h (IDD_...,
// IDR_..., IDS_...); these functions take the same IDs and return plain
// descriptions that the Qt twins build widgets, menus and shortcuts from.

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <QtGui/QImage>

namespace mfcless::res
{

// A resource name: a number, or a string for named resources.
struct ResourceName
{
	uint16_t id = 0;
	std::u16string name;
	ResourceName() = default;
	ResourceName(uint16_t i) : id(i) { }
	ResourceName(const char16_t *n) : name(n) { }
	bool operator<(const ResourceName &o) const { return id != o.id ? id < o.id : name < o.name; }
	bool operator==(const ResourceName &o) const { return id == o.id && name == o.name; }
};

namespace ResourceType
{
	inline constexpr uint16_t Cursor = 1, Bitmap = 2, Icon = 3, Menu = 4, Dialog = 5, String = 6,
		Accelerator = 9, RCData = 10, GroupCursor = 12, GroupIcon = 14, Version = 16, DialogInit = 240;
}

// The raw bytes of a resource, or an empty span.
struct ResourceData
{
	const unsigned char *data = nullptr;
	std::size_t size = 0;
	explicit operator bool() const { return data != nullptr; }
};
ResourceData FindData(const ResourceName &type, const ResourceName &name);
// The names of all resources of a type.
std::vector<ResourceName> List(const ResourceName &type);

// String table entry. Empty if missing.
std::u16string String(uint32_t id);

// ---- dialog templates (DIALOG / DIALOGEX) ----------------------------------
struct DialogItem
{
	uint32_t id = 0;
	// Predefined classes are named as in Win32 ("BUTTON", "EDIT", "STATIC",
	// "LISTBOX", "SCROLLBAR", "COMBOBOX"); others as written in the script
	// ("msctls_trackbar32", "SysListView32", ...).
	std::u16string className;
	std::u16string text;
	uint16_t textResource = 0;  // text given as a resource ID (icons, bitmaps)
	uint32_t style = 0, exStyle = 0, helpId = 0;
	int16_t x = 0, y = 0, cx = 0, cy = 0;  // dialog units
};
struct DialogTemplate
{
	uint32_t style = 0, exStyle = 0;
	int16_t x = 0, y = 0, cx = 0, cy = 0;  // dialog units
	std::u16string menu, windowClass, caption;
	uint16_t menuId = 0;
	uint16_t pointSize = 0, weight = 0;
	bool italic = false;
	std::u16string font;
	std::vector<DialogItem> items;
};
std::optional<DialogTemplate> Dialog(uint32_t id);

// AFX_DIALOG_LAYOUT: how each control moves and grows with the dialog, in
// percent of the dialog's growth, in template item order.
struct DialogLayoutItem
{
	int16_t moveX = 0, moveY = 0, sizeX = 0, sizeY = 0;
};
std::vector<DialogLayoutItem> DialogLayout(uint32_t id);

// DLGINIT: initial strings of combo boxes, by control ID.
std::map<uint32_t, std::vector<std::string>> DialogInit(uint32_t id);

// ---- menus (MENU / MENUEX) -------------------------------------------------
struct MenuItem
{
	uint32_t id = 0;      // 0 for separators and popups
	std::u16string text;  // with '&' mnemonics and "\t" accelerator text
	uint32_t flags = 0;   // MF_GRAYED 0x1, MF_CHECKED 0x8, MF_POPUP 0x10, MF_MENUBARBREAK 0x20, MF_MENUBREAK 0x40, MF_SEPARATOR 0x800
	bool separator = false;
	std::vector<MenuItem> children;  // for popups
};
std::optional<std::vector<MenuItem>> Menu(uint32_t id);

// ---- accelerators ----------------------------------------------------------
struct Accelerator
{
	uint16_t flags = 0;  // FVIRTKEY 1, FSHIFT 4, FCONTROL 8, FALT 16
	uint16_t key = 0;    // virtual-key code (FVIRTKEY) or character
	uint16_t command = 0;
};
std::vector<Accelerator> Accelerators(uint32_t id);

// ---- images ----------------------------------------------------------------
QImage Bitmap(uint32_t id);
// The icon image closest to the size (largest if 0).
QImage Icon(uint32_t id, int size = 0);
// A file resource of a named type, such as PNG.
QImage Image(const char16_t *type, uint32_t id);

// Dialog units to pixels for a dialog using the given font (Windows' rule:
// horizontal units are a quarter of the font's average character width,
// vertical units an eighth of its height).
struct DialogUnits
{
	int baseX = 6, baseY = 13;
	int X(int du) const { return (du * baseX + 2) / 4; }
	int Y(int du) const { return (du * baseY + 4) / 8; }
};

} // namespace mfcless::res
