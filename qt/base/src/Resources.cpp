// mfcless base: the .res reader (see mfcless/Resources.h).
//
// The format is Windows' compiled resource file: a list of entries, each a
// header (sizes, type, name, language, flags) followed by the data, both
// aligned to 4 bytes. The data formats (dialog templates, menus, string
// tables, accelerator tables, bitmaps, icon groups) are documented Win32
// structures; they are decoded here byte by byte, with no Windows headers.

#include "mfcless/Resources.h"

#include <QtCore/QBuffer>
#include <QtCore/QByteArray>
#include <QtGui/QImageReader>

#include <cstring>
#include <map>
#include <mutex>

namespace mfcless
{
// Generated in qt/resources/.
const unsigned char *EmbeddedResources(std::size_t &size);
}

namespace mfcless::res
{

namespace
{

class Reader
{
public:
	Reader(const unsigned char *data, std::size_t size) : m_data(data), m_size(size) { }

	bool Ok() const { return m_ok; }
	std::size_t Pos() const { return m_pos; }
	std::size_t Remaining() const { return m_pos <= m_size ? m_size - m_pos : 0; }
	void Seek(std::size_t pos) { m_pos = pos; if(m_pos > m_size) m_ok = false; }
	void Align(std::size_t a) { m_pos = (m_pos + a - 1) / a * a; }

	uint8_t U8()
	{
		if(m_pos + 1 > m_size) { m_ok = false; return 0; }
		return m_data[m_pos++];
	}
	uint16_t U16()
	{
		if(m_pos + 2 > m_size) { m_ok = false; m_pos = m_size; return 0; }
		uint16_t v = static_cast<uint16_t>(m_data[m_pos] | (m_data[m_pos + 1] << 8));
		m_pos += 2;
		return v;
	}
	uint32_t U32()
	{
		uint32_t lo = U16();
		uint32_t hi = U16();
		return lo | (hi << 16);
	}
	int16_t S16() { return static_cast<int16_t>(U16()); }

	// A NUL-terminated UTF-16 string.
	std::u16string Str()
	{
		std::u16string s;
		while(m_ok)
		{
			char16_t c = U16();
			if(!c)
				break;
			s.push_back(c);
		}
		return s;
	}
	// Either 0xFFFF + ordinal, or a string ("sz_Or_Ord").
	ResourceName NameOrId()
	{
		ResourceName n;
		const uint16_t first = Peek16();
		if(first == 0xFFFF)
		{
			U16();
			n.id = U16();
		} else
		{
			n.name = Str();
		}
		return n;
	}
	uint16_t Peek16() const
	{
		if(m_pos + 2 > m_size)
			return 0;
		return static_cast<uint16_t>(m_data[m_pos] | (m_data[m_pos + 1] << 8));
	}
	const unsigned char *Here() const { return m_data + m_pos; }

private:
	const unsigned char *m_data;
	std::size_t m_size;
	std::size_t m_pos = 0;
	bool m_ok = true;
};


struct Key
{
	ResourceName type, name;
	bool operator<(const Key &o) const
	{
		if(!(type == o.type))
			return type < o.type;
		return name < o.name;
	}
};

const std::map<Key, ResourceData> &Index()
{
	static std::map<Key, ResourceData> index;
	static std::once_flag once;
	std::call_once(once, []()
	{
		std::size_t size = 0;
		const unsigned char *data = EmbeddedResources(size);
		Reader r(data, size);
		while(r.Ok() && r.Remaining() >= 32)
		{
			const std::size_t start = r.Pos();
			const uint32_t dataSize = r.U32();
			const uint32_t headerSize = r.U32();
			Key key;
			key.type = r.NameOrId();
			key.name = r.NameOrId();
			if(!r.Ok() || headerSize < 16 || start + headerSize + dataSize > size)
				break;
			r.Seek(start + headerSize);
			// The first entry of a .res file is an empty marker (type 0).
			if(dataSize > 0 && !(key.type.id == 0 && key.type.name.empty()))
				index.emplace(key, ResourceData{data + start + headerSize, dataSize});
			r.Seek(start + headerSize + dataSize);
			r.Align(4);
		}
	});
	return index;
}

std::string ToUtf8(const std::u16string &s)
{
	return QString::fromStdU16String(s).toStdString();
}

} // namespace


ResourceData FindData(const ResourceName &type, const ResourceName &name)
{
	const auto &index = Index();
	auto it = index.find(Key{type, name});
	if(it == index.end())
		return {};
	return it->second;
}


std::u16string String(uint32_t id)
{
	// Strings are stored in blocks of 16; block n holds IDs (n - 1) * 16 to n * 16 - 1.
	const auto block = FindData(ResourceType::String, static_cast<uint16_t>((id >> 4) + 1));
	if(!block)
		return {};
	Reader r(block.data, block.size);
	for(uint32_t i = 0; i < 16 && r.Ok(); i++)
	{
		const uint16_t length = r.U16();
		std::u16string s;
		for(uint16_t c = 0; c < length && r.Ok(); c++)
			s.push_back(r.U16());
		if(i == (id & 15))
			return s;
	}
	return {};
}


std::optional<DialogTemplate> Dialog(uint32_t id)
{
	const auto res = FindData(ResourceType::Dialog, static_cast<uint16_t>(id));
	if(!res)
		return std::nullopt;
	Reader r(res.data, res.size);
	DialogTemplate t;
	const uint16_t dlgVer = r.U16();
	const uint16_t signature = r.U16();
	const bool extended = (dlgVer == 1 && signature == 0xFFFF);
	uint16_t count = 0;
	if(extended)
	{
		r.U32();  // help ID
		t.exStyle = r.U32();
		t.style = r.U32();
	} else
	{
		r.Seek(0);
		t.style = r.U32();
		t.exStyle = r.U32();
	}
	count = r.U16();
	t.x = r.S16();
	t.y = r.S16();
	t.cx = r.S16();
	t.cy = r.S16();
	const ResourceName menu = r.NameOrId();
	t.menuId = menu.id;
	t.menu = menu.name;
	const ResourceName cls = r.NameOrId();
	t.windowClass = cls.name;
	t.caption = r.Str();
	constexpr uint32_t DS_SETFONT = 0x40, DS_SHELLFONT = 0x48;
	if((t.style & DS_SETFONT) || (t.style & DS_SHELLFONT) == DS_SHELLFONT)
	{
		t.pointSize = r.U16();
		if(extended)
		{
			t.weight = r.U16();
			t.italic = r.U8() != 0;
			r.U8();  // charset
		}
		t.font = r.Str();
	}
	for(uint16_t i = 0; i < count && r.Ok(); i++)
	{
		r.Align(4);
		DialogItem item;
		if(extended)
		{
			item.helpId = r.U32();
			item.exStyle = r.U32();
			item.style = r.U32();
		} else
		{
			item.style = r.U32();
			item.exStyle = r.U32();
		}
		item.x = r.S16();
		item.y = r.S16();
		item.cx = r.S16();
		item.cy = r.S16();
		item.id = extended ? r.U32() : r.U16();
		const ResourceName itemClass = r.NameOrId();
		if(itemClass.name.empty())
		{
			switch(itemClass.id)
			{
			case 0x80: item.className = u"BUTTON"; break;
			case 0x81: item.className = u"EDIT"; break;
			case 0x82: item.className = u"STATIC"; break;
			case 0x83: item.className = u"LISTBOX"; break;
			case 0x84: item.className = u"SCROLLBAR"; break;
			case 0x85: item.className = u"COMBOBOX"; break;
			default: break;
			}
		} else
		{
			item.className = itemClass.name;
			for(auto &c : item.className)
				if(c >= u'a' && c <= u'z' && (item.className == u"button" || item.className == u"edit" || item.className == u"static" || item.className == u"listbox" || item.className == u"scrollbar" || item.className == u"combobox"))
					c = static_cast<char16_t>(c - u'a' + u'A');
		}
		const ResourceName text = r.NameOrId();
		item.text = text.name;
		item.textResource = text.id;
		const uint16_t extra = r.U16();
		r.Seek(r.Pos() + extra);
		t.items.push_back(std::move(item));
	}
	if(!r.Ok())
		return std::nullopt;
	return t;
}


std::vector<DialogLayoutItem> DialogLayout(uint32_t id)
{
	std::vector<DialogLayoutItem> items;
	const auto res = FindData(ResourceName(u"AFX_DIALOG_LAYOUT"), static_cast<uint16_t>(id));
	if(!res)
		return items;
	Reader r(res.data, res.size);
	if(r.U16() != 0)  // version
		return items;
	while(r.Ok() && r.Remaining() >= 8)
	{
		DialogLayoutItem item;
		item.moveX = r.S16();
		item.moveY = r.S16();
		item.sizeX = r.S16();
		item.sizeY = r.S16();
		items.push_back(item);
	}
	return items;
}


std::map<uint32_t, std::vector<std::string>> DialogInit(uint32_t id)
{
	std::map<uint32_t, std::vector<std::string>> result;
	const auto res = FindData(ResourceType::DialogInit, static_cast<uint16_t>(id));
	if(!res)
		return result;
	Reader r(res.data, res.size);
	while(r.Ok() && r.Remaining() >= 2)
	{
		const uint16_t control = r.U16();
		if(control == 0)
			break;
		const uint16_t message = r.U16();
		const uint32_t length = r.U32();
		std::string text(reinterpret_cast<const char *>(r.Here()), std::min<std::size_t>(length, r.Remaining()));
		r.Seek(r.Pos() + length);
		while(!text.empty() && text.back() == '\0')
			text.pop_back();
		constexpr uint16_t CB_ADDSTRING = 0x0143, LB_ADDSTRING = 0x0180, CBEM_INSERTITEM = 0x040B;
		if(message == CB_ADDSTRING || message == LB_ADDSTRING || message == CBEM_INSERTITEM)
			result[control].push_back(text);
	}
	return result;
}


namespace
{
void ReadMenuItems(Reader &r, std::vector<MenuItem> &items)
{
	constexpr uint16_t MF_POPUP = 0x10, MF_END = 0x80, MF_SEPARATOR = 0x800;
	while(r.Ok() && r.Remaining() >= 2)
	{
		MenuItem item;
		const uint16_t flags = r.U16();
		item.flags = flags & ~MF_END;
		if(!(flags & MF_POPUP))
			item.id = r.U16();
		item.text = r.Str();
		item.separator = (!(flags & MF_POPUP) && item.id == 0 && item.text.empty()) || (flags & MF_SEPARATOR);
		if(flags & MF_POPUP)
			ReadMenuItems(r, item.children);
		items.push_back(std::move(item));
		if(flags & MF_END)
			break;
	}
}

void ReadMenuExItems(Reader &r, std::vector<MenuItem> &items)
{
	constexpr uint16_t MFR_END = 0x80, MFR_POPUP = 0x01;
	constexpr uint32_t MFT_SEPARATOR = 0x800;
	while(r.Ok() && r.Remaining() >= 14)
	{
		r.Align(4);
		MenuItem item;
		const uint32_t type = r.U32();
		const uint32_t state = r.U32();
		item.id = r.U32();
		const uint16_t resInfo = r.U16();
		item.text = r.Str();
		item.flags = type | state;
		item.separator = (type & MFT_SEPARATOR) != 0;
		if(resInfo & MFR_POPUP)
		{
			r.Align(4);
			r.U32();  // help ID
			item.flags |= 0x10;
			ReadMenuExItems(r, item.children);
		}
		items.push_back(std::move(item));
		if(resInfo & MFR_END)
			break;
	}
}
} // namespace


std::optional<std::vector<MenuItem>> Menu(uint32_t id)
{
	const auto res = FindData(ResourceType::Menu, static_cast<uint16_t>(id));
	if(!res)
		return std::nullopt;
	Reader r(res.data, res.size);
	const uint16_t version = r.U16();
	const uint16_t headerSize = r.U16();
	std::vector<MenuItem> items;
	if(version == 1)
	{
		r.Seek(4 + headerSize);
		ReadMenuExItems(r, items);
	} else
	{
		r.Seek(4 + headerSize);
		ReadMenuItems(r, items);
	}
	return items;
}


std::vector<Accelerator> Accelerators(uint32_t id)
{
	std::vector<Accelerator> result;
	const auto res = FindData(ResourceType::Accelerator, static_cast<uint16_t>(id));
	if(!res)
		return result;
	Reader r(res.data, res.size);
	while(r.Ok() && r.Remaining() >= 8)
	{
		Accelerator a;
		const uint16_t flags = r.U16();
		a.key = r.U16();
		a.command = r.U16();
		r.U16();
		a.flags = flags & 0x7F;
		result.push_back(a);
		if(flags & 0x80)
			break;
	}
	return result;
}


namespace
{
// A BMP file from a bitmap resource (a DIB without the file header).
QImage DibToImage(const ResourceData &res)
{
	if(res.size < 40)
		return {};
	Reader r(res.data, res.size);
	const uint32_t headerSize = r.U32();
	r.U32();  // width
	r.U32();  // height
	r.U16();  // planes
	const uint16_t bitCount = r.U16();
	const uint32_t compression = r.U32();
	r.U32();  // image size
	r.U32();
	r.U32();
	uint32_t colorsUsed = r.U32();
	if(colorsUsed == 0 && bitCount <= 8)
		colorsUsed = 1u << bitCount;
	uint32_t masks = (compression == 3 && headerSize == 40) ? 12 : 0;  // BI_BITFIELDS
	const uint32_t offset = 14 + headerSize + masks + colorsUsed * 4;
	QByteArray file;
	file.reserve(static_cast<int>(14 + res.size));
	auto put16 = [&](uint16_t v) { file.append(static_cast<char>(v & 0xFF)); file.append(static_cast<char>(v >> 8)); };
	auto put32 = [&](uint32_t v) { put16(static_cast<uint16_t>(v & 0xFFFF)); put16(static_cast<uint16_t>(v >> 16)); };
	file.append('B');
	file.append('M');
	put32(static_cast<uint32_t>(14 + res.size));
	put32(0);
	put32(offset);
	file.append(reinterpret_cast<const char *>(res.data), static_cast<int>(res.size));
	QImage image;
	image.loadFromData(file, "BMP");
	return image;
}
} // namespace


QImage Bitmap(uint32_t id)
{
	return DibToImage(FindData(ResourceType::Bitmap, static_cast<uint16_t>(id)));
}


QImage Icon(uint32_t id, int size)
{
	const auto group = FindData(ResourceType::GroupIcon, static_cast<uint16_t>(id));
	if(!group)
		return {};
	Reader r(group.data, group.size);
	r.U16();  // reserved
	r.U16();  // type
	const uint16_t count = r.U16();
	struct Entry { int width; uint16_t iconId; uint16_t bits; uint32_t bytes; };
	std::vector<Entry> entries;
	for(uint16_t i = 0; i < count && r.Ok(); i++)
	{
		Entry e;
		e.width = r.U8();
		if(e.width == 0)
			e.width = 256;
		r.U8();  // height
		r.U8();  // colours
		r.U8();
		r.U16();  // planes
		e.bits = r.U16();
		e.bytes = r.U32();
		e.iconId = r.U16();
		entries.push_back(e);
	}
	if(entries.empty())
		return {};
	const Entry *best = &entries[0];
	for(const auto &e : entries)
	{
		const bool better = size > 0
			? (std::abs(e.width - size) < std::abs(best->width - size) || (e.width == best->width && e.bits > best->bits))
			: (e.width > best->width || (e.width == best->width && e.bits > best->bits));
		if(better)
			best = &e;
	}
	const auto icon = FindData(ResourceType::Icon, best->iconId);
	if(!icon)
		return {};
	// A one-image .ico file around it, which Qt reads (PNG or DIB inside).
	QByteArray file;
	auto put16 = [&](uint16_t v) { file.append(static_cast<char>(v & 0xFF)); file.append(static_cast<char>(v >> 8)); };
	auto put32 = [&](uint32_t v) { put16(static_cast<uint16_t>(v & 0xFFFF)); put16(static_cast<uint16_t>(v >> 16)); };
	put16(0);
	put16(1);
	put16(1);
	file.append(static_cast<char>(best->width >= 256 ? 0 : best->width));
	file.append(static_cast<char>(best->width >= 256 ? 0 : best->width));
	file.append('\0');
	file.append('\0');
	put16(1);
	put16(best->bits);
	put32(static_cast<uint32_t>(icon.size));
	put32(22);
	file.append(reinterpret_cast<const char *>(icon.data), static_cast<int>(icon.size));
	QImage image;
	if(!image.loadFromData(file, "ICO"))
		image.loadFromData(QByteArray(reinterpret_cast<const char *>(icon.data), static_cast<int>(icon.size)));
	return image;
}


QImage Image(const char16_t *type, uint32_t id)
{
	const auto res = FindData(ResourceName(type), static_cast<uint16_t>(id));
	if(!res)
		return {};
	QImage image;
	image.loadFromData(reinterpret_cast<const uchar *>(res.data), static_cast<int>(res.size));
	return image;
}

} // namespace mfcless::res
