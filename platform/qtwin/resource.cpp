// Win32 resources on Linux: a module's resources are a compiled .res file
// (tools/rc/rc.py builds it from upstream's mptrack.rc, in the format rc.exe
// writes), linked into the executable and registered at startup. FindResource,
// LoadString, LoadBitmap, LoadIcon, LoadCursor and LoadImage read it as
// Windows reads the resource section of an executable.
#include "QtWin.h"

#include <QtCore/QFile>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

#include <algorithm>
#include <cstring>
#include <map>
#include <tuple>

namespace linopenmpt::qtwin
{
namespace
{

// A resource type or name: a number, or an upper-case string.
struct ResId
{
	WORD id = 0;
	std::u16string name;
	bool IsId() const { return name.empty(); }
	bool operator==(const ResId &o) const { return id == o.id && name == o.name; }
	bool operator<(const ResId &o) const { return std::tie(id, name) < std::tie(o.id, o.name); }
};

struct Resource
{
	ResId type;
	ResId name;
	WORD language = 0;
	const BYTE *data = nullptr;
	DWORD size = 0;
};

struct Module
{
	std::vector<Resource> resources;
};

std::map<HMODULE, Module> &Modules()
{
	static auto *modules = new std::map<HMODULE, Module>; // leaked: used until exit
	return *modules;
}

HMODULE const g_executable = reinterpret_cast<HMODULE>(static_cast<ULONG_PTR>(0x400000));

std::u16string Upper(std::u16string s)
{
	for(auto &c : s)
	{
		if(c >= u'a' && c <= u'z')
			c = static_cast<char16_t>(c - u'a' + u'A');
	}
	return s;
}

ResId ToResId(LPCTSTR value)
{
	ResId r;
	if(IS_INTRESOURCE(value))
	{
		r.id = static_cast<WORD>(reinterpret_cast<ULONG_PTR>(value));
		return r;
	}
	const QString text = QString::fromWCharArray(value);
	if(text.startsWith(QLatin1Char('#')))
	{
		r.id = static_cast<WORD>(text.mid(1).toUInt());
		return r;
	}
	r.name = Upper(text.toStdU16String());
	return r;
}

// Reads a type or name field of a .res header: 0xFFFF and a number, or a
// NUL-terminated UTF-16 string.
const BYTE *ReadResId(const BYTE *p, const BYTE *end, ResId &out)
{
	if(p + 2 > end)
		return nullptr;
	WORD first;
	std::memcpy(&first, p, 2);
	if(first == 0xFFFF)
	{
		if(p + 4 > end)
			return nullptr;
		std::memcpy(&out.id, p + 2, 2);
		return p + 4;
	}
	std::u16string s;
	while(p + 2 <= end)
	{
		char16_t c;
		std::memcpy(&c, p, 2);
		p += 2;
		if(c == 0)
			break;
		s.push_back(c);
	}
	out.name = Upper(s);
	return p;
}

Module *ModuleOf(HMODULE module)
{
	if(module == nullptr)
		module = g_executable;
	auto it = Modules().find(module);
	return it != Modules().end() ? &it->second : nullptr;
}

const Resource *Find(HMODULE module, const ResId &type, const ResId &name, int language = -1)
{
	Module *m = ModuleOf(module);
	if(m == nullptr)
		return nullptr;
	const Resource *best = nullptr;
	int bestRank = 100;
	for(const Resource &r : m->resources)
	{
		if(!(r.type == type) || !(r.name == name))
			continue;
		// As Windows: the requested language, neutral, US English, then any.
		int rank = 4;
		if(language >= 0 && r.language == language)
			rank = 0;
		else if(r.language == 0)
			rank = 1;
		else if(r.language == 0x0409)
			rank = 2;
		else if(PRIMARYLANGID(r.language) == LANG_ENGLISH)
			rank = 3;
		if(rank < bestRank)
		{
			best = &r;
			bestRank = rank;
		}
	}
	return best;
}

ResId Id(WORD id)
{
	ResId r;
	r.id = id;
	return r;
}

std::wstring FromUtf16(const char16_t *s, std::size_t length)
{
	return QString::fromUtf16(s, static_cast<qsizetype>(length)).toStdWString();
}

// ---- icons and cursors -------------------------------------------------------------

// One image of an icon or cursor, from a resource group or a file.
struct IconImage
{
	int width = 0;
	int height = 0;
	int bitCount = 0;
	const BYTE *data = nullptr;
	DWORD size = 0;
	QPoint hotspot;
};

qreal ScreenRatio()
{
	const QScreen *screen = QGuiApplication::primaryScreen();
	return screen ? screen->devicePixelRatio() : 1.0;
}

// As LookupIconIdFromDirectoryEx: the closest size, preferring larger
// images to scale down, then the most colours.
const IconImage *Choose(const std::vector<IconImage> &images, int cx, int cy)
{
	const IconImage *best = nullptr;
	long bestScore = 0;
	for(const IconImage &image : images)
	{
		const int dx = image.width - cx, dy = image.height - cy;
		long score = (dx < 0 ? -dx * 4 : dx) + (dy < 0 ? -dy * 4 : dy);
		score = score * 64 - image.bitCount;
		if(best == nullptr || score < bestScore)
		{
			best = &image;
			bestScore = score;
		}
	}
	return best;
}

QImage DecodeIconImage(const IconImage &icon)
{
	if(icon.size >= 8 && std::memcmp(icon.data, "\x89PNG\r\n\x1a\n", 8) == 0)
		return QImage::fromData(icon.data, static_cast<int>(icon.size), "PNG").convertToFormat(QImage::Format_ARGB32);
	if(icon.size < sizeof(BITMAPINFOHEADER))
		return QImage();
	BITMAPINFOHEADER header;
	std::memcpy(&header, icon.data, sizeof(header));
	const int width = header.biWidth;
	const int height = header.biHeight / 2; // the XOR image and the AND mask
	const int bpp = header.biBitCount;
	if(width <= 0 || height <= 0)
		return QImage();
	int colors = header.biClrUsed ? static_cast<int>(header.biClrUsed) : (bpp <= 8 ? 1 << bpp : 0);
	if(header.biCompression == BI_BITFIELDS)
		colors = 3;
	const std::size_t infoSize = header.biSize + colors * 4;
	const std::size_t xorStride = ((width * bpp + 31) / 32) * 4;
	const std::size_t andStride = ((width + 31) / 32) * 4;
	if(icon.size < infoSize + xorStride * height)
		return QImage();
	std::vector<BYTE> info(icon.data, icon.data + infoSize);
	reinterpret_cast<BITMAPINFOHEADER *>(info.data())->biHeight = height;
	const BYTE *xorBits = icon.data + infoSize;
	QImage image = DecodeDib(xorBits, reinterpret_cast<const BITMAPINFO *>(info.data())).convertToFormat(QImage::Format_ARGB32);
	if(image.isNull())
		return image;
	bool hasAlpha = false;
	if(bpp == 32)
	{
		for(int y = 0; y < height && !hasAlpha; y++)
		{
			const auto *line = reinterpret_cast<const QRgb *>(xorBits + y * xorStride);
			for(int x = 0; x < width; x++)
			{
				if(qAlpha(line[x]) != 0)
				{
					hasAlpha = true;
					break;
				}
			}
		}
	}
	const BYTE *andBits = xorBits + xorStride * height;
	const bool haveMask = icon.size >= infoSize + xorStride * height + andStride * height;
	for(int y = 0; y < height; y++)
	{
		auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
		const BYTE *mask = haveMask ? andBits + (height - 1 - y) * andStride : nullptr; // bottom-up
		for(int x = 0; x < width; x++)
		{
			if(hasAlpha)
			{
				line[x] = reinterpret_cast<const QRgb *>(xorBits + (height - 1 - y) * xorStride)[x];
				continue;
			}
			const bool transparent = mask != nullptr && (mask[x / 8] & (0x80 >> (x % 8)));
			line[x] = transparent ? 0 : (line[x] | 0xFF000000u);
		}
	}
	return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

HICON MakeIcon(const std::vector<IconImage> &images, bool cursor, int cx, int cy)
{
	if(images.empty())
		return nullptr;
	// Load the size the screen shows, so icons stay sharp at high DPI.
	const qreal ratio = ScreenRatio();
	const int px = qRound(cx * ratio), py = qRound(cy * ratio);
	const IconImage *chosen = Choose(images, px, py);
	QImage image = DecodeIconImage(*chosen);
	if(image.isNull())
		return nullptr;
	if(image.width() != px || image.height() != py)
		image = image.scaled(px, py, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	image.setDevicePixelRatio(ratio);
	auto *icon = new IconData;
	icon->image = image;
	icon->isCursor = cursor;
	icon->hotspot = QPoint(chosen->hotspot.x() * px / std::max(chosen->width, 1), chosen->hotspot.y() * py / std::max(chosen->height, 1));
	return reinterpret_cast<HICON>(icon);
}

std::vector<IconImage> GroupImages(HMODULE module, const Resource &group, bool cursor)
{
	std::vector<IconImage> images;
	if(group.size < 6)
		return images;
	WORD count;
	std::memcpy(&count, group.data + 4, 2);
	for(WORD i = 0; i < count; i++)
	{
		const BYTE *entry = group.data + 6 + 14 * i;
		if(entry + 14 > group.data + group.size)
			break;
		IconImage image;
		WORD id;
		if(cursor)
		{
			WORD w, h, planes, bits;
			std::memcpy(&w, entry, 2);
			std::memcpy(&h, entry + 2, 2);
			std::memcpy(&planes, entry + 4, 2);
			std::memcpy(&bits, entry + 6, 2);
			image.width = w;
			image.height = h / 2;
			image.bitCount = bits;
		} else
		{
			image.width = entry[0] ? entry[0] : 256;
			image.height = entry[1] ? entry[1] : 256;
			WORD bits;
			std::memcpy(&bits, entry + 6, 2);
			image.bitCount = bits;
		}
		std::memcpy(&id, entry + 12, 2);
		const Resource *r = Find(module, Id(cursor ? 1 : 3), Id(id), group.language);
		if(r == nullptr)
			continue;
		image.data = r->data;
		image.size = r->size;
		if(cursor && r->size >= 4)
		{
			WORD hx, hy;
			std::memcpy(&hx, r->data, 2);
			std::memcpy(&hy, r->data + 2, 2);
			image.hotspot = QPoint(hx, hy);
			image.data += 4;
			image.size -= 4;
		}
		images.push_back(image);
	}
	return images;
}

// An .ico or .cur file.
std::vector<IconImage> FileImages(const QByteArray &file, bool cursor)
{
	std::vector<IconImage> images;
	const auto *data = reinterpret_cast<const BYTE *>(file.constData());
	if(file.size() < 6)
		return images;
	WORD count;
	std::memcpy(&count, data + 4, 2);
	for(WORD i = 0; i < count; i++)
	{
		const BYTE *entry = data + 6 + 16 * i;
		if(entry + 16 > data + file.size())
			break;
		IconImage image;
		image.width = entry[0] ? entry[0] : 256;
		image.height = entry[1] ? entry[1] : 256;
		WORD a, b;
		DWORD size, offset;
		std::memcpy(&a, entry + 4, 2);
		std::memcpy(&b, entry + 6, 2);
		std::memcpy(&size, entry + 8, 4);
		std::memcpy(&offset, entry + 12, 4);
		if(offset + static_cast<qint64>(size) > file.size())
			continue;
		if(cursor)
			image.hotspot = QPoint(a, b);
		else
			image.bitCount = b;
		image.data = data + offset;
		image.size = size;
		images.push_back(image);
	}
	return images;
}

struct SharedKey
{
	HMODULE module;
	ResId name;
	bool cursor;
	int cx, cy;
	bool operator<(const SharedKey &o) const { return std::tie(module, name, cursor, cx, cy) < std::tie(o.module, o.name, o.cursor, o.cx, o.cy); }
};

} // namespace

HICON LoadResourceIcon(HINSTANCE module, LPCTSTR name, bool cursor, int cx, int cy, bool shared)
{
	const ResId id = ToResId(name);
	static auto *cache = new std::map<SharedKey, HICON>; // shared icons live until exit, as on Windows
	const SharedKey key{module, id, cursor, cx, cy};
	if(shared)
	{
		auto it = cache->find(key);
		if(it != cache->end())
			return it->second;
	}
	const Resource *group = Find(module, Id(cursor ? 12 : 14), id);
	if(group == nullptr)
		return nullptr;
	HICON icon = MakeIcon(GroupImages(module, *group, cursor), cursor, cx, cy);
	if(icon != nullptr && shared)
	{
		reinterpret_cast<IconData *>(icon)->shared = true;
		(*cache)[key] = icon;
	}
	return icon;
}

} // namespace linopenmpt::qtwin

using namespace linopenmpt::qtwin;

// ---- the resource API -----------------------------------------------------------------

void LinOpenMPTRegisterResources(HMODULE module, const void *res, size_t size)
{
	Module &m = Modules()[module];
	m.resources.clear();
	const auto *p = static_cast<const BYTE *>(res);
	const BYTE *end = p + size;
	while(p + 8 <= end)
	{
		DWORD dataSize, headerSize;
		std::memcpy(&dataSize, p, 4);
		std::memcpy(&headerSize, p + 4, 4);
		if(headerSize < 16 || p + headerSize + dataSize > end)
			break;
		Resource r;
		const BYTE *h = ReadResId(p + 8, p + headerSize, r.type);
		if(h)
			h = ReadResId(h, p + headerSize, r.name);
		if(h)
		{
			h = p + ((h - p + 3) & ~3);
			// DataVersion, MemoryFlags, LanguageId
			if(h + 8 <= p + headerSize)
				std::memcpy(&r.language, h + 6, 2);
			r.data = p + headerSize;
			r.size = dataSize;
			if(!(r.type.IsId() && r.type.id == 0 && dataSize == 0))
				m.resources.push_back(r);
		}
		p += headerSize + dataSize;
		p += (4 - ((p - static_cast<const BYTE *>(res)) & 3)) & 3;
	}
}

HMODULE GetModuleHandle(LPCTSTR name)
{
	return name == nullptr ? g_executable : nullptr;
}

HRSRC FindResourceEx(HMODULE module, LPCTSTR type, LPCTSTR name, WORD language)
{
	return reinterpret_cast<HRSRC>(const_cast<Resource *>(Find(module, ToResId(type), ToResId(name), language)));
}

HRSRC FindResource(HMODULE module, LPCTSTR name, LPCTSTR type)
{
	return reinterpret_cast<HRSRC>(const_cast<Resource *>(Find(module, ToResId(type), ToResId(name))));
}

HGLOBAL LoadResource(HMODULE, HRSRC resource)
{
	const auto *r = reinterpret_cast<const Resource *>(resource);
	return r ? const_cast<BYTE *>(r->data) : nullptr;
}

LPVOID LockResource(HGLOBAL data)
{
	return data;
}

BOOL FreeResource(HGLOBAL)
{
	return FALSE; // as on Windows (resources stay mapped)
}

DWORD SizeofResource(HMODULE, HRSRC resource)
{
	const auto *r = reinterpret_cast<const Resource *>(resource);
	return r ? r->size : 0;
}

int LoadString(HINSTANCE instance, UINT id, LPTSTR buffer, int max)
{
	if(buffer == nullptr)
		return 0;
	const Resource *block = Find(instance, Id(6), Id(static_cast<WORD>((id >> 4) + 1)));
	std::wstring text;
	bool found = false;
	if(block != nullptr)
	{
		const BYTE *p = block->data;
		const BYTE *end = p + block->size;
		for(UINT i = 0; i < 16 && p + 2 <= end; i++)
		{
			WORD length;
			std::memcpy(&length, p, 2);
			p += 2;
			if(i == (id & 15))
			{
				std::u16string s(length, u'\0');
				std::memcpy(s.data(), p, std::min<std::size_t>(length * 2, end - p));
				text = FromUtf16(s.data(), s.size());
				found = length > 0;
				break;
			}
			p += length * 2;
		}
	}
	if(max == 0)
	{
		// The caller gets a read-only pointer to the string (not terminated
		// on Windows; terminated here, which callers accept).
		static auto *strings = new std::map<std::pair<HINSTANCE, UINT>, std::wstring>;
		const std::wstring &stored = (*strings)[{instance, id}] = text;
		*reinterpret_cast<LPCTSTR *>(buffer) = found ? stored.c_str() : nullptr;
		return found ? static_cast<int>(stored.size()) : 0;
	}
	const int count = std::min(static_cast<int>(text.size()), max - 1);
	std::wmemcpy(buffer, text.data(), count);
	buffer[count] = 0;
	return found ? count : 0;
}

HBITMAP LoadBitmap(HINSTANCE instance, LPCTSTR name)
{
	if(instance == nullptr)
		return nullptr; // OBM_ system bitmaps: none
	const Resource *r = Find(instance, Id(2), ToResId(name));
	if(r == nullptr || r->size < sizeof(BITMAPINFOHEADER))
		return nullptr;
	const auto *info = reinterpret_cast<const BITMAPINFO *>(r->data);
	const BITMAPINFOHEADER &h = info->bmiHeader;
	int colors = h.biClrUsed ? static_cast<int>(h.biClrUsed) : (h.biBitCount <= 8 ? 1 << h.biBitCount : 0);
	if(h.biCompression == BI_BITFIELDS)
		colors = 3;
	const BYTE *bits = r->data + h.biSize + colors * 4;
	const int height = h.biHeight < 0 ? -h.biHeight : h.biHeight;
	// A device-dependent bitmap, as LoadBitmap makes one.
	HBITMAP bitmap = CreateCompatibleBitmap(nullptr, h.biWidth, height);
	if(bitmap != nullptr)
		SetDIBits(nullptr, bitmap, 0, static_cast<UINT>(height), bits, info, DIB_RGB_COLORS);
	return bitmap;
}

HANDLE LoadImage(HINSTANCE instance, LPCTSTR name, UINT type, int cx, int cy, UINT flags)
{
	if(type == IMAGE_BITMAP)
	{
		if(flags & LR_LOADFROMFILE)
		{
			QImage image(QString::fromWCharArray(name));
			if(image.isNull())
				return nullptr;
			image = image.convertToFormat(QImage::Format_ARGB32);
			if(cx > 0 && cy > 0)
				image = image.scaled(cx, cy, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			BITMAPINFO bmi{};
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = image.width();
			bmi.bmiHeader.biHeight = -image.height();
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;
			void *bits = nullptr;
			HBITMAP bitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
			if(bitmap != nullptr && bits != nullptr)
			{
				for(int y = 0; y < image.height(); y++)
					std::memcpy(static_cast<BYTE *>(bits) + y * image.width() * 4, image.constScanLine(y), image.width() * 4);
			}
			return bitmap;
		}
		if(flags & LR_CREATEDIBSECTION)
		{
			const Resource *r = Find(instance, Id(2), ToResId(name));
			if(r == nullptr)
				return nullptr;
			const auto *info = reinterpret_cast<const BITMAPINFO *>(r->data);
			const BITMAPINFOHEADER &h = info->bmiHeader;
			int colors = h.biClrUsed ? static_cast<int>(h.biClrUsed) : (h.biBitCount <= 8 ? 1 << h.biBitCount : 0);
			if(h.biCompression == BI_BITFIELDS)
				colors = 3;
			const std::size_t offset = h.biSize + colors * 4;
			void *bits = nullptr;
			HBITMAP bitmap = CreateDIBSection(nullptr, info, DIB_RGB_COLORS, &bits, nullptr, 0);
			if(bitmap != nullptr && bits != nullptr)
			{
				const int height = h.biHeight < 0 ? -h.biHeight : h.biHeight;
				const std::size_t size = static_cast<std::size_t>(((h.biWidth * h.biBitCount + 31) / 32) * 4) * height;
				std::memcpy(bits, r->data + offset, std::min<std::size_t>(size, r->size - offset));
			}
			return bitmap;
		}
		return LoadBitmap(instance, name);
	}
	if(type != IMAGE_ICON && type != IMAGE_CURSOR)
		return nullptr;
	const bool cursor = type == IMAGE_CURSOR;
	if(cx == 0 && cy == 0)
	{
		// LR_DEFAULTSIZE or not, the system size is what upstream asks for.
		cx = GetSystemMetrics(cursor ? SM_CXCURSOR : SM_CXICON);
		cy = GetSystemMetrics(cursor ? SM_CYCURSOR : SM_CYICON);
	}
	if(flags & LR_LOADFROMFILE)
	{
		QFile file(QString::fromWCharArray(name));
		if(!file.open(QIODevice::ReadOnly))
			return nullptr;
		const QByteArray data = file.readAll();
		return MakeIcon(FileImages(data, cursor), cursor, cx, cy);
	}
	if(instance == nullptr)
		return cursor ? static_cast<HANDLE>(LoadCursor(nullptr, name)) : static_cast<HANDLE>(LoadIcon(nullptr, name));
	return LoadResourceIcon(instance, name, cursor, cx, cy, (flags & LR_SHARED) != 0);
}
