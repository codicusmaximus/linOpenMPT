/*
 * PatternFont.cpp
 * ---------------
 * Purpose: Code for creating pattern font bitmaps
 * Notes  : mfcless twin of upstream's mptrack/PatternFont.cpp.
 *          Hand-edited from the twin.py draft: the custom font is drawn
 *          with QPainter instead of GDI.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "PatternFont.h"
#include "mfcless/Widgets.h"
#include "HighDPISupport.h"
#include "Mptrack.h"
#include "Mainfrm.h"
#include "TrackerSettings.h"
#include "../soundlib/Tables.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QImage>
#include <QtGui/QPainter>

OPENMPT_NAMESPACE_BEGIN

//////////////////////////////////////////////
// Font Definitions

// Medium Font (Default)
static constexpr PATTERNFONT gDefaultPatternFont = 
{
	nullptr,
	nullptr,
	92,13,	// Column Width & Height
	0,0,	// Clear location
	130,8,	// Space Location.
	{20, 20, 24, 9, 15},		// Element Widths
	{0, 0, 0, 0, 0},			// Padding pixels contained in element width
	20,13,	// Numbers 0-F (hex)
	30,13,	// Numbers 10-29 (dec)
	64,26,	// A-M#
	78,26,	// N-Z?										// MEDIUM FONT !!!
	0, 0,
	{ 7, 5 }, 8,	// Note & Octave Width
	42,13,			// Volume Column Effects
	8,8,
	-1,
	8,		// 8+7 = 15
	-3, -1, 12,
	1,		// Margin for first digit of PC event parameter number
	2,		// Margin for first digit of PC event parameter value
	1,		// Margin for second digit of parameter
	13,		// Vertical spacing between letters in the bitmap
};


//////////////////////////////////////////////////
// Small Font

static constexpr PATTERNFONT gSmallPatternFont =
{
	nullptr,
	nullptr,
	70,11,	// Column Width & Height
	92,0,	// Clear location
	130,8,	// Space Location.
	{16, 14, 18, 7, 11},		// Element Widths
	{0, 0, 0, 0, 0},			// Padding pixels contained in element width
	108,13,	// Numbers 0-F (hex)
	120,13,	// Numbers 10-29 (dec)
	142,26,	// A-M#
	150,26,	// N-Z?										// SMALL FONT !!!
	92, 0,	// Notes
	{ 5, 5 }, 6,	// Note & Octave Width
	132,13,		// Volume Column Effects
	6,5,
	-1,
	6,		// 8+7 = 15
	-3,	1, 9,	// InstrOfs + nInstrHiWidth
	1,		// Margin for first digit of PC event parameter number
	2,		// Margin for first digit of PC event parameter value
	1,		// Margin for second digit of parameter
	13,		// // Vertical spacing between letters in the bitmap
};

// NOTE: See also CViewPattern::DrawNote() when changing stuff here
// or adding new fonts - The custom tuning note names might require
// some additions there.

const PATTERNFONT *PatternFont::currentFont = nullptr;

static MODPLUGDIB customFontBitmap;
static MODPLUGDIB customFontBitmapASCII;

// mfcless: the custom font is drawn with a QPainter into a white image, and
// turned into upstream's 4-bit bitmap (black 0, white 15) afterwards.
static void DrawChar(QPainter &painter, QChar ch, int x, int y, int w, int h)
{
	painter.drawText(QRect(x, y, w, h), Qt::AlignCenter | Qt::TextSingleLine, QString(ch));
}

static void DrawChar(QPainter &painter, char ch, int x, int y, int w, int h)
{
	DrawChar(painter, QChar::fromLatin1(ch), x, y, w, h);
}

#if MPT_CXX_AT_LEAST(20) && MPT_USTRING_MODE_UTF8
static void DrawChar(QPainter &painter, char8_t ch8, int x, int y, int w, int h)
{
	DrawChar(painter, mpt::unsafe_char_convert<char>(ch8), x, y, w, h);
}
#endif

// The image as a top-down 4-bit bitmap of width x height (the width a multiple of 8).
static uint8 *ToNibbles(const QImage &image, int width, int height)
{
	uint8 *bits = new uint8[width * height / 2];
	for(int y = 0; y < height; y++)
	{
		const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
		uint8 *out = bits + y * width / 2;
		for(int x = 0; x < width; x += 2)
		{
			const uint8 hi = (x < image.width() && qGray(line[x]) < 128) ? 0x0 : 0xF;
			const uint8 lo = (x + 1 < image.width() && qGray(line[x + 1]) < 128) ? 0x0 : 0xF;
			out[x / 2] = static_cast<uint8>((hi << 4) | lo);
		}
	}
	return bits;
}

template<typename char_t>
static void DrawString(QPainter &hDC, const char_t *text, int len, int x, int y, int w, int h)
{
	for(int i = 0; i < len; i++)
	{
		DrawChar(hDC, text[i], x, y, w, h);
		x += w;
	}
}

void PatternFont::UpdateFont(QWidget * hwnd)
{
	const int dpi = HighDPISupport::GetDpiForWindow(hwnd);
	FontSetting font = TrackerSettings::Instance().patternFont;
	const PATTERNFONT *builtinFont = nullptr;
	if(font.name == PATTERNFONT_SMALL || font.name.empty())
	{
		builtinFont = &gSmallPatternFont;
	} else if(font.name == PATTERNFONT_LARGE)
	{
		builtinFont = &gDefaultPatternFont;
	}

	const int builtInFontSize = Util::muldivr(std::min(font.size + 1, 11), dpi, 96);
	if(builtinFont != nullptr && builtInFontSize < 2)
	{
		currentFont = builtinFont;
		return;
	}

	static PATTERNFONT pf{};
	currentFont = &pf;

	static FontSetting previousFont;
	static int previousDPI = 0;
	if(previousFont == font && previousDPI == dpi)
	{
		// Nothing to do
		return;
	}
	previousFont = font;
	previousDPI = dpi;
	DeleteFontData();
	pf.dib = &customFontBitmap;
	pf.dibASCII = nullptr;

	// Upscale built-in font?
	if(builtinFont != nullptr)
	{
		// Copy and scale original 4-bit bitmap
		customFontBitmap.bmiHeader = CMainFrame::bmpNotes->bmiHeader;
		customFontBitmap.bmiHeader.biWidth *= builtInFontSize;
		customFontBitmap.bmiHeader.biHeight *= builtInFontSize;
		customFontBitmap.bmiHeader.biSizeImage = customFontBitmap.bmiHeader.biWidth * customFontBitmap.bmiHeader.biHeight / 2;
		uint8 *scaledBits = new uint8[customFontBitmap.bmiHeader.biSizeImage];
		customFontBitmap.lpDibBits = scaledBits;

		// Upscale the image (ugly code ahead)
		const uint8 *origPixels = CMainFrame::bmpNotes->lpDibBits;
		uint8 *scaledPixels = scaledBits;
		const int bytesPerLine = customFontBitmap.bmiHeader.biWidth / 2, scaleBytes = bytesPerLine * builtInFontSize;
		bool outPos = false;
		for(int y = 0; y < CMainFrame::bmpNotes->bmiHeader.biHeight; y++, scaledPixels += scaleBytes - bytesPerLine)
		{
			for(int x = 0; x < CMainFrame::bmpNotes->bmiHeader.biWidth; x++)
			{
				uint8 pixel = *origPixels;
				if(x % 2u == 0)
				{
					pixel >>= 4;
				} else
				{
					pixel &= 0x0F;
					origPixels++;
				}
				for(int scaleX = 0; scaleX < builtInFontSize; scaleX++)
				{
					if(!outPos)
					{
						for(int scaleY = 0; scaleY < scaleBytes; scaleY += bytesPerLine)
						{
							scaledPixels[scaleY] = pixel << 4;
						}
					} else
					{
						for(int scaleY = 0; scaleY < scaleBytes; scaleY += bytesPerLine)
						{
							scaledPixels[scaleY] |= pixel;
						}
						scaledPixels++;
					}
					outPos = !outPos;
				}
			}
		}
		pf.nWidth = (builtinFont->nWidth - 4) * builtInFontSize + 4;
		pf.nHeight = builtinFont->nHeight * builtInFontSize;
		pf.nClrX = builtinFont->nClrX * builtInFontSize;
		pf.nClrY = builtinFont->nClrY * builtInFontSize;
		pf.nSpaceX = builtinFont->nSpaceX * builtInFontSize;
		pf.nSpaceY = builtinFont->nSpaceY * builtInFontSize;
		for(std::size_t i = 0; i < std::size(pf.nEltWidths); i++)
		{
			pf.nEltWidths[i] = builtinFont->nEltWidths[i] * builtInFontSize;
			pf.padding[i] = builtinFont->padding[i] * builtInFontSize;
		}
		pf.nNumX = builtinFont->nNumX * builtInFontSize;
		pf.nNumY = builtinFont->nNumY * builtInFontSize;
		pf.nNum10X = builtinFont->nNum10X * builtInFontSize;
		pf.nNum10Y = builtinFont->nNum10Y * builtInFontSize;
		pf.nAlphaAM_X = builtinFont->nAlphaAM_X * builtInFontSize;
		pf.nAlphaAM_Y = builtinFont->nAlphaAM_Y * builtInFontSize;
		pf.nAlphaNZ_X = builtinFont->nAlphaNZ_X * builtInFontSize;
		pf.nAlphaNZ_Y = builtinFont->nAlphaNZ_Y * builtInFontSize;
		pf.nNoteX = builtinFont->nNoteX * builtInFontSize;
		pf.nNoteY = builtinFont->nNoteY * builtInFontSize;
		pf.nNoteWidth[0] = builtinFont->nNoteWidth[0] * builtInFontSize;
		pf.nNoteWidth[1] = builtinFont->nNoteWidth[1] * builtInFontSize;
		pf.nOctaveWidth = builtinFont->nOctaveWidth * builtInFontSize;
		pf.nVolX = builtinFont->nVolX * builtInFontSize;
		pf.nVolY = builtinFont->nVolY * builtInFontSize;
		pf.nVolCmdWidth = builtinFont->nVolCmdWidth * builtInFontSize;
		pf.nVolHiWidth = builtinFont->nVolHiWidth * builtInFontSize;
		pf.nCmdOfs = builtinFont->nCmdOfs * builtInFontSize;
		pf.nParamHiWidth = builtinFont->nParamHiWidth * builtInFontSize;
		pf.nInstrOfs = builtinFont->nInstrOfs * builtInFontSize;
		pf.nInstr10Ofs = builtinFont->nInstr10Ofs * builtInFontSize;
		pf.nInstrHiWidth = builtinFont->nInstrHiWidth * builtInFontSize;
		pf.pcParamMargin = builtinFont->pcParamMargin * builtInFontSize;
		pf.pcValMargin = builtinFont->pcValMargin * builtInFontSize;
		pf.paramLoMargin = builtinFont->paramLoMargin * builtInFontSize;
		pf.spacingY = builtinFont->spacingY * builtInFontSize;

		// Create 4-pixel border
		const int bmWidth2 = pf.dib->bmiHeader.biWidth / 2;
		for(int y = 0, x = (customFontBitmap.bmiHeader.biHeight - pf.nClrY - pf.nHeight) * bmWidth2 + (pf.nClrX + pf.nWidth - 4) / 2; y < pf.nHeight; y++, x += bmWidth2)
		{
			scaledBits[x] = 0xEC;
			scaledBits[x + 1] = 0xC4;
		}

		return;
	}

	// Create our own font!

	// Point size to pixels
	font.size = -mfcless::MulDiv(font.size, dpi, 720);

	QFont gdiFont(mfcless::ToQString(mpt::ToUnicode(font.name)));
	gdiFont.setPixelSize(std::max(1, -font.size));
	gdiFont.setBold(font.flags[FontSetting::Bold]);
	gdiFont.setItalic(font.flags[FontSetting::Italic]);
	gdiFont.setFixedPitch(true);
	gdiFont.setStyleStrategy(QFont::NoAntialias);

	const QFontMetrics metrics(gdiFont);
	const int charWidth = std::max(1, metrics.horizontalAdvance(u'W')), charHeight = std::max(1, metrics.height());
	const int spacing = charWidth / 4;
	const int width = charWidth * 12 + spacing * 2 + 4, height = charHeight * 21;

	pf.nWidth = width;					// Column Width & Height, including 4-pixels border
	pf.nHeight = charHeight;
	pf.nClrX = pf.nClrY = 0;			// Clear (empty note) location
	pf.nSpaceX = charWidth * 7;			// White location (must be big enough)
	pf.nSpaceY = charHeight;
	pf.nEltWidths[0] = charWidth * 3;	// Note
	pf.padding[0] = 0;
	pf.nEltWidths[1] = charWidth * 3 + spacing;	// Instr
	pf.padding[1] = spacing;
	pf.nEltWidths[2] = charWidth * 3 + spacing;	// Volume
	pf.padding[2] = spacing;
	pf.nEltWidths[3] = charWidth;		// Command letter
	pf.padding[3] = 0;
	pf.nEltWidths[4] = charWidth * 2;	// Command param
	pf.padding[4] = 0;
	pf.nNumX = charWidth * 3;			// Vertically-oriented numbers 0x00-0x0F
	pf.nNumY = charHeight;
	pf.nNum10X = charWidth * 4;			// Numbers 10-29
	pf.nNum10Y = charHeight;
	pf.nAlphaAM_X = charWidth * 6;		// Letters A-M +#
	pf.nAlphaAM_Y = charHeight * 2;
	pf.nAlphaNZ_X = charWidth * 7;		// Letters N-Z +?
	pf.nAlphaNZ_Y = charHeight * 2;
	pf.nNoteX = 0;						// Notes ..., C-, C#, ...
	pf.nNoteY = 0;
	pf.nNoteWidth[0] = charWidth;		// Total width of note (C#)
	pf.nNoteWidth[1] = charWidth;		// Total width of note (C#)
	pf.nOctaveWidth = charWidth;		// Octave Width
	pf.nVolX = charWidth * 8;			// Volume Column Effects
	pf.nVolY = charHeight;
	pf.nVolCmdWidth = charWidth;		// Width of volume effect
	pf.nVolHiWidth = charWidth;			// Width of first character in volume parameter
	pf.nCmdOfs = 0;						// XOffset (-xxx) around the command letter
	pf.nParamHiWidth = charWidth;
	pf.nInstrOfs = -charWidth;
	pf.nInstr10Ofs = 0;
	pf.nInstrHiWidth = charWidth * 2;
	pf.pcParamMargin = 0;
	pf.pcValMargin = 0;
	pf.paramLoMargin = 0;				// Margin for second digit of parameter
	pf.spacingY = charHeight;

	{

	pf.dib->bmiHeader.biSize = 40;
	pf.dib->bmiHeader.biWidth = ((width + 7) & ~7);	// 4-byte alignment
	pf.dib->bmiHeader.biHeight = -(int32)height;
	pf.dib->bmiHeader.biSizeImage = pf.dib->bmiHeader.biWidth * height / 2;
	pf.dib->bmiHeader.biPlanes = 1;
	pf.dib->bmiHeader.biBitCount = 4;
	pf.dib->bmiHeader.biCompression = 0;
	pf.dib->bmiColors[0] = rgb2quad(mfcless::Rgb(0x00, 0x00, 0x00));
	pf.dib->bmiColors[15] = rgb2quad(mfcless::Rgb(0xFF, 0xFF, 0xFF));

	QImage image(pf.dib->bmiHeader.biWidth, height, QImage::Format_RGB32);
	image.fill(Qt::white);
	{
	QPainter hDC(&image);
	hDC.setFont(gdiFont);
	hDC.setPen(Qt::black);

	// Empty cells (dots - i-th bit set = dot in the i-th column of a cell)
	const uint8 dots[5] = { 1 | 2 | 4, 2 | 4, 2 | 4, 1, 1 | 2 };
	const auto dotStr = TrackerSettings::Instance().patternFontDot.Get();
	auto dotChar = dotStr.empty() ? UC_(' ') : dotStr[0];
	for(int cell = 0, offset = 0; cell < static_cast<int>(std::size(dots)); cell++)
	{
		uint8 dot = dots[cell];
		for(int i = 0; dot != 0; i++)
		{
			if(dot & 1) DrawChar(hDC, dotChar, pf.nClrX + offset + i * charWidth, pf.nClrY, charWidth, charHeight);
			dot >>= 1;
		}
		offset += pf.nEltWidths[cell];
	}

	// Notes
	for(int i = 0; i < 12; i++)
	{
		DrawString(hDC, NoteNamesSharp[i], 2, pf.nNoteX, pf.nNoteY + (i + 1) * charHeight, charWidth, charHeight);
	}
	DrawString(hDC, "^^", 2, pf.nNoteX, pf.nNoteY + 13 * charHeight, charWidth, charHeight);
	DrawString(hDC, "==", 2, pf.nNoteX, pf.nNoteY + 14 * charHeight, charWidth, charHeight);
	DrawString(hDC, "PC", 2, pf.nNoteX, pf.nNoteY + 15 * charHeight, charWidth, charHeight);
	DrawString(hDC, "PCs", 3, pf.nNoteX, pf.nNoteY + 16 * charHeight, charWidth, charHeight);
	DrawString(hDC, "~~", 2, pf.nNoteX, pf.nNoteY + 17 * charHeight, charWidth, charHeight);

	// Hex chars
	const char hexChars[] = "0123456789ABCDEF";
	for(int i = 0; i < 16; i++)
	{
		DrawChar(hDC, hexChars[i], pf.nNumX, pf.nNumY + i * charHeight, charWidth, charHeight);
	}
	// Double-digit numbers
	for(int i = 0; i < 20; i++)
	{
		char s[2];
		s[0] = char('1' + i / 10);
		s[1] = char('0' + i % 10);
		DrawString(hDC, s, 2, pf.nNum10X, pf.nNum10Y + i * charHeight, charWidth, charHeight);
	}

	// Volume commands
	const char volEffects[]= " vpcdabuhlrgfe:o";
	static_assert(mpt::array_size<decltype(volEffects)>::size - 1 == MAX_VOLCMDS);
	for(int i = 0; i < MAX_VOLCMDS; i++)
	{
		DrawChar(hDC, volEffects[i], pf.nVolX, pf.nVolY + i * charHeight, charWidth, charHeight);
	}

	// Letters A-Z
	for(int i = 0; i < 13; i++)
	{
		DrawChar(hDC, char('A' + i), pf.nAlphaAM_X, pf.nAlphaAM_Y + i * charHeight, charWidth, charHeight);
		DrawChar(hDC, char('N' + i), pf.nAlphaNZ_X, pf.nAlphaNZ_Y + i * charHeight, charWidth, charHeight);
	}
	// Special chars
	DrawChar(hDC, '#', pf.nAlphaAM_X, pf.nAlphaAM_Y + 13 * charHeight, charWidth, charHeight);
	DrawChar(hDC, '?', pf.nAlphaNZ_X, pf.nAlphaNZ_Y + 13 * charHeight, charWidth, charHeight);
	DrawChar(hDC, 'b', pf.nAlphaAM_X, pf.nAlphaAM_Y + 14 * charHeight, charWidth, charHeight);
	DrawChar(hDC, '\\', pf.nAlphaNZ_X, pf.nAlphaNZ_Y + 14 * charHeight, charWidth, charHeight);
	DrawChar(hDC, '-', pf.nAlphaAM_X, pf.nAlphaAM_Y + 15 * charHeight, charWidth, charHeight);
	DrawChar(hDC, ':', pf.nAlphaNZ_X, pf.nAlphaNZ_Y + 15 * charHeight, charWidth, charHeight);
	DrawChar(hDC, '+', pf.nAlphaAM_X, pf.nAlphaAM_Y + 16 * charHeight, charWidth, charHeight);
	DrawChar(hDC, '*', pf.nAlphaNZ_X, pf.nAlphaNZ_Y + 16 * charHeight, charWidth, charHeight);
	DrawChar(hDC, 'd', pf.nAlphaAM_X, pf.nAlphaAM_Y + 17 * charHeight, charWidth, charHeight);

	}
	uint8 *bits = ToNibbles(image, pf.dib->bmiHeader.biWidth, height);
	pf.dib->lpDibBits = bits;
	// Create 4-pixel border
	const int bmWidth2 = pf.dib->bmiHeader.biWidth / 2;
	for(int y = 0, x = width / 2 - 2; y < height; y++, x += bmWidth2)
	{
		bits[x] = 0xEC;
		bits[x + 1] = 0xC4;
	}

	}

	{

		pf.dibASCII = &customFontBitmapASCII;

		pf.dibASCII->bmiHeader.biSize = 40;
		pf.dibASCII->bmiHeader.biWidth = ((charWidth * 128 + 7) & ~7);	// 4-byte alignment
		pf.dibASCII->bmiHeader.biHeight = -(int32)charHeight;
		pf.dibASCII->bmiHeader.biSizeImage = pf.dibASCII->bmiHeader.biWidth * charHeight / 2;
		pf.dibASCII->bmiHeader.biPlanes = 1;
		pf.dibASCII->bmiHeader.biBitCount = 4;
		pf.dibASCII->bmiHeader.biCompression = 0;
		pf.dibASCII->bmiColors[0] = rgb2quad(mfcless::Rgb(0x00, 0x00, 0x00));
		pf.dibASCII->bmiColors[15] = rgb2quad(mfcless::Rgb(0xFF, 0xFF, 0xFF));

		QImage image(pf.dibASCII->bmiHeader.biWidth, charHeight, QImage::Format_RGB32);
		image.fill(Qt::white);
		{
			QPainter hDC(&image);
			hDC.setFont(gdiFont);
			hDC.setPen(Qt::black);
			for(uint32 c = 32; c < 128; ++c)
			{
				DrawChar(hDC, mpt::unsafe_char_convert<char>(static_cast<char32_t>(c)), charWidth * c, 0, charWidth, charHeight);
			}
		}
		pf.dibASCII->lpDibBits = ToNibbles(image, pf.dibASCII->bmiHeader.biWidth, charHeight);
	}
}


void PatternFont::DeleteFontData()
{
	delete[] customFontBitmap.lpDibBits;
	customFontBitmap.lpDibBits = nullptr;
	delete[] customFontBitmapASCII.lpDibBits;
	customFontBitmapASCII.lpDibBits = nullptr;
}

OPENMPT_NAMESPACE_END
