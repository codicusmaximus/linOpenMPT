/*
 * AboutDialog.cpp
 * ---------------
 * Purpose: About dialog with credits, system information and a fancy demo effect.
 * Notes  : mfcless twin of upstream's mptrack/AboutDialog.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "AboutDialog.h"
#include "mfcless/Widgets.h"
#include "HighDPISupport.h"
#include "Image.h"
#include "Mptrack.h"
#include "MPTrackUtil.h"
#include "TrackerSettings.h"
#include "mpt/format/join.hpp"
#include "mpt/string/utility.hpp"
#include "resource.h"
#include "../common/version.h"
#include "../misc/mptWine.h"

#if !MPT_OS_WINDOWS
#include <sys/utsname.h>
#endif


OPENMPT_NAMESPACE_BEGIN


CAboutDlg *CAboutDlg::instance = nullptr;

void CRippleBitmap::RegisterMessageMap()
{
	mfcless::Window<QWidget>::RegisterMessageMap();
	// ON_WM_PAINT() (mfcless::Window)
	// ON_WM_ERASEBKGND(): by hand (Qt event override)
	// ON_WM_MOUSEMOVE() (mfcless::Window)
	// ON_WM_MOUSEHOVER(): by hand (Qt event override)
	// ON_WM_MOUSELEAVE() (mfcless::Window)
}


CRippleBitmap::CRippleBitmap()
{
	m_bitmapSrc = LoadPixelImage(GetResource(MAKEINTRESOURCE(IDB_MPTRACK), UL_("PNG")));
	m_bitmapTarget = std::make_unique<RawGDIDIB>(m_bitmapSrc->Width(), m_bitmapSrc->Height());
	m_offset1.assign(m_bitmapSrc->Pixels().size(), 0);
	m_offset2.assign(m_bitmapSrc->Pixels().size(), 0);
	m_frontBuf = m_offset2.data();
	m_backBuf = m_offset1.data();

	// Pre-fill first and last row of output bitmap, since those won't be touched.
	const RawGDIDIB::Pixel *in1 = m_bitmapSrc->Pixels().data(), *in2 = m_bitmapSrc->Pixels().data() + (m_bitmapSrc->Height() - 1) * m_bitmapSrc->Width();
	RawGDIDIB::Pixel *out1 = m_bitmapTarget->Pixels().data(), *out2 = m_bitmapTarget->Pixels().data() + (m_bitmapSrc->Height() - 1) * m_bitmapSrc->Width();
	for(uint32 i = 0; i < m_bitmapSrc->Width(); i++)
	{
		*(out1++) = *(in1++);
		*(out2++) = *(in2++);
	}

	MemsetZero(m_bi);
	m_bi.biSize = sizeof(BITMAPINFOHEADER);
	m_bi.biWidth = m_bitmapSrc->Width();
	m_bi.biHeight = -(int32)m_bitmapSrc->Height();
	m_bi.biPlanes = 1;
	m_bi.biBitCount = 32;
	m_bi.biCompression = BI_RGB;
	m_bi.biSizeImage = m_bitmapSrc->Width() * m_bitmapSrc->Height() * 4;
}


CRippleBitmap::~CRippleBitmap()
{
	if(!m_showMouse)
	{
		ShowCursor(true);
	}
}


void CRippleBitmap::OnMouseMove(uint32 nFlags, mfcless::Point point)
{

	// Rate limit in order to avoid too may ripples.
	uint32 now = mfcless::TickCount();
	if(now - m_lastRipple < UPDATE_INTERVAL)
		return;
	m_lastRipple = now;

	// Initiate ripples at cursor location
	point.x = HighDPISupport::ScalePixelsInv(point.x, this);
	point.y = HighDPISupport::ScalePixelsInv(point.y, this);
	Limit(point.x, 1, int(m_bitmapSrc->Width()) - 2);
	Limit(point.y, 2, int(m_bitmapSrc->Height()) - 3);
	int32 *p = m_backBuf + point.x + point.y * m_bitmapSrc->Width();
	p[0] += (nFlags & mfcless::MouseKey::Left) ? 50 : 150;
	p[0] += (nFlags & mfcless::MouseKey::Middle) ? 150 : 0;

	int32 w = m_bitmapSrc->Width();
	// Make the initial point of this ripple a bit "fatter".
	p[-1]     += p[0] / 2; p[1]      += p[0] / 2;
	p[-w]     += p[0] / 2; p[w]      += p[0] / 2;
	p[-w - 1] += p[0] / 4; p[-w + 1] += p[0] / 4;
	p[w - 1]  += p[0] / 4; p[w + 1]  += p[0] / 4;

	m_damp = !(nFlags & mfcless::MouseKey::Right);
	m_activity = true;

	// Wine will only ever generate MouseLeave message when the message
	// queue is completely empty and the hover timeout has expired.
	// This results in a hidden mouse cursor long after it had already left the
	// control.
	// Avoid hiding the mouse cursor on Wine. Interferring with the users input
	// methods is an absolute no-go.
	if(mpt::OS::Windows::IsWine())
	{
		return;
	}

	TRACKMOUSEEVENT me;
	me.cbSize = sizeof(TRACKMOUSEEVENT);
	me.hwndTrack = this;
	me.dwFlags = TME_LEAVE | TME_HOVER;
	me.dwHoverTime = 1500;

	if(TrackMouseEvent(&me) && m_showMouse)
	{
		ShowCursor(false);
		m_showMouse = false;
	}
}


void CRippleBitmap::OnMouseLeave()
{
	if(!m_showMouse)
	{
		ShowCursor(true);
		m_showMouse = true;
	}
}


void CRippleBitmap::OnPaint()
{
	mfcless::PaintDC dc(this);

	mfcless::Rect rect;
	GetClientRect(rect);
	StretchDIBits(dc.m_hDC,
		0, 0, rect.Width(), rect.Height(),
		0, 0, m_bitmapTarget->Width(), m_bitmapTarget->Height(),
		m_bitmapTarget->Pixels().data(),
		reinterpret_cast<BITMAPINFO *>(&m_bi), DIB_RGB_COLORS, mfcless::rop::SrcCopy);
}


bool CRippleBitmap::Animate()
{
	// Were there any pixels being moved in the last frame?
	if(!m_activity)
		return false;

	uint32 now = mfcless::TickCount();
	if(now - m_lastFrame < UPDATE_INTERVAL)
		return true;
	m_lastFrame = now;
	m_activity = false;

	m_frontBuf = (m_frame ? m_offset2 : m_offset1).data();
	m_backBuf = (m_frame ? m_offset1 : m_offset2).data();

	// Spread the ripples...
	const int32 w = m_bitmapSrc->Width(), h = m_bitmapSrc->Height();
	const int32 numPixels = w * (h - 2);
	const int32 *back = m_backBuf + w;
	int32 *front = m_frontBuf + w;
	for(int32 i = numPixels; i != 0; i--, back++, front++)
	{
		(*front) = (back[-1] + back[1] + back[w] + back[-w]) / 2 - (*front);
		if(m_damp) (*front) -= (*front) >> 5;
	}

	// ...and compute the final picture.
	const int32 *offset = m_frontBuf + w;
	const RawGDIDIB::Pixel *pixelIn = m_bitmapSrc->Pixels().data() + w;
	RawGDIDIB::Pixel *pixelOut = m_bitmapTarget->Pixels().data() + w;
	RawGDIDIB::Pixel *limitMin = m_bitmapSrc->Pixels().data(), *limitMax = m_bitmapSrc->Pixels().data() + m_bitmapSrc->Pixels().size() - 1;
	for(int32 i = numPixels; i != 0; i--, pixelIn++, pixelOut++, offset++)
	{
		// Compute pixel displacement
		const int32 xOff = offset[-1] - offset[1];
		const int32 yOff = offset[-w] - offset[w];

		if(xOff | yOff)
		{
			const RawGDIDIB::Pixel *p = pixelIn + xOff + yOff * w;
			Limit(p, limitMin, limitMax);
			// Add a bit of shading depending on how far we're displacing the pixel...
			pixelOut->r = mpt::saturate_cast<uint8>(p->r + (p->r * xOff) / 32);
			pixelOut->g = mpt::saturate_cast<uint8>(p->g + (p->g * xOff) / 32);
			pixelOut->b = mpt::saturate_cast<uint8>(p->b + (p->b * xOff) / 32);
			// ...and mix it with original picture
			pixelOut->r = (pixelOut->r + pixelIn->r) / 2u;
			pixelOut->g = (pixelOut->g + pixelIn->g) / 2u;
			pixelOut->b = (pixelOut->b + pixelIn->b) / 2u;
			// And now some cheap image smoothing...
			pixelOut[-1].r = (pixelOut->r + pixelOut[-1].r) / 2u;
			pixelOut[-1].g = (pixelOut->g + pixelOut[-1].g) / 2u;
			pixelOut[-1].b = (pixelOut->b + pixelOut[-1].b) / 2u;
			pixelOut[-w].r = (pixelOut->r + pixelOut[-w].r) / 2u;
			pixelOut[-w].g = (pixelOut->g + pixelOut[-w].g) / 2u;
			pixelOut[-w].b = (pixelOut->b + pixelOut[-w].b) / 2u;
			m_activity = true;	// Also use this to update activity status...
		} else
		{
			*pixelOut = *pixelIn;
		}
	}

	m_frame = !m_frame;

	InvalidateRect(NULL, false);

	return true;
}


CAboutDlg::~CAboutDlg()
{
	instance = nullptr;
}


void CAboutDlg::OnOK()
{
	instance = nullptr;
	if(m_TimerID != 0)
	{
		KillTimer(m_TimerID);
		m_TimerID = 0;
	}
	DestroyWindow();
	delete this;
}


void CAboutDlg::OnCancel()
{
	OnOK();
}


bool CAboutDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

	mpt::ustring app;
	app += MPT_UFORMAT("OpenMPT{} ({}, {} bit)\r\n")(
			mpt::ToUnicode(mpt::Charset::ASCII, OPENMPT_BUILD_VARIANT_MONIKER),
			mpt::OS::Windows::Name(mpt::OS::Windows::GetProcessArchitecture()),
			mpt::arch_bits)
		+ UL_("Version ") + Build::GetVersionStringSimple() + UL_("\r\n\r\n")
		+ Build::GetURL(Build::Url::Website) + UL_("\r\n");
	SetItemText(IDC_EDIT3, mpt::ToUnicode(app));

	SubclassDlgItem(IDC_BITMAP1, m_bmp);

	m_Tab->InsertItem(TCIF_TEXT, 0, UL_("OpenMPT"), 0, 0, 0, 0);
	m_Tab->InsertItem(TCIF_TEXT, 1, UL_("Components"), 0, 0, 0, 0);
	m_Tab->InsertItem(TCIF_TEXT, 2, UL_("Credits"), 0, 0, 0, 0);
	m_Tab->InsertItem(TCIF_TEXT, 3, UL_("License"), 0, 0, 0, 0);
	m_Tab->InsertItem(TCIF_TEXT, 4, UL_("Resources"), 0, 0, 0, 0);
	if(mpt::OS::Windows::IsWine()) m_Tab->InsertItem(TCIF_TEXT, 5, UL_("Wine"), 0, 0, 0, 0);
	m_Tab->setCurrentIndex(0);

	OnTabChange();

	if(m_TimerID != 0)
	{
		KillTimer(m_TimerID);
		m_TimerID = 0;
	}
	m_TimerID = SetTimer(TIMERID_ABOUT_DEFAULT, CRippleBitmap::UPDATE_INTERVAL, nullptr);

	return true;
}


void CAboutDlg::OnTimer(uintptr_t nIDEvent)
{
	if(nIDEvent == m_TimerID)
	{
		m_bmp->Animate();
	}
}


void CAboutDlg::OnTabChange()
{
	mfcless::SetWindowText(m_TabEdit, mpt::ToUnicode(mpt::replace(GetTabText(m_Tab->currentIndex()), U_("\n"), U_("\r\n"))));
}


#ifdef MPT_ENABLE_ARCH_INTRINSICS
static mpt::ustring CPUFeaturesToString(mpt::arch::current::feature_flags procSupport)
{
	std::vector<mpt::ustring> features;
#if MPT_COMPILER_MSVC
#if defined(MPT_ENABLE_ARCH_X86)
	features.push_back(UL_("x86"));
#endif
#if defined(MPT_ENABLE_ARCH_AMD64)
	features.push_back(UL_("amd64"));
#endif
	struct ProcFlag
	{
		decltype(procSupport) flag;
		const mpt::uchar *name;
	};
	static constexpr ProcFlag flags[] =
	{
		{ mpt::arch::current::feature::none, UL_("") },
#if defined(MPT_ENABLE_ARCH_X86) || defined(MPT_ENABLE_ARCH_AMD64)
		{ mpt::arch::x86::feature::mmx, UL_("mmx") },
		{ mpt::arch::x86::feature::sse, UL_("sse") },
		{ mpt::arch::x86::feature::sse2, UL_("sse2") },
		{ mpt::arch::x86::feature::sse3, UL_("sse3") },
		{ mpt::arch::x86::feature::ssse3, UL_("ssse3") },
		{ mpt::arch::x86::feature::sse4_1, UL_("sse4.1") },
		{ mpt::arch::x86::feature::sse4_2, UL_("sse4.2") },
		{ mpt::arch::x86::feature::avx, UL_("avx") },
		{ mpt::arch::x86::feature::avx2, UL_("avx2") },
#endif
	};
	for(const auto &f : flags)
	{
		if(procSupport & f.flag) features.push_back(f.name);
	}
#else
	MPT_UNUSED_VARIABLE(procSupport);
#endif
	return mpt::join_format(features, U_(" "));
}
#endif // MPT_ENABLE_ARCH_INTRINSICS


mpt::ustring CAboutDlg::GetTabText(int tab)
{
	const mpt::ustring lf = UL_("\n");
	const mpt::ustring yes = UL_("yes");
	const mpt::ustring no = UL_("no");
#ifdef MPT_ENABLE_ARCH_INTRINSICS
	const mpt::arch::current::cpu_info CPUInfo = mpt::arch::get_cpu_info();
#endif // MPT_ENABLE_ARCH_INTRINSICS
	mpt::ustring text;
	switch(tab)
	{
		case 0:
			text = UL_("OpenMPT - Open ModPlug Tracker\n\n")
				+ MPT_UFORMAT("Version: {}\n")(Build::GetVersionStringExtended())
				+ MPT_UFORMAT("Source Code: {}\n")(SourceInfo::Current().GetUrlWithRevision() + UL_(" ") + SourceInfo::Current().GetStateString())
				+ MPT_UFORMAT("Build Date: {}\n")(Build::GetBuildDateString())
				+ MPT_UFORMAT("Compiler: {}\n")(Build::GetBuildCompilerString())
				+ MPT_UFORMAT("Architecture: {}\n")(mpt::OS::Windows::Name(mpt::OS::Windows::GetProcessArchitecture()));
#if MPT_OS_WINDOWS
			text += MPT_UFORMAT("Required Windows Kernel Level: {}\n")(mpt::OS::Windows::Version::GetName(mpt::OS::Windows::Version::GetMinimumKernelLevel()))
				+ MPT_UFORMAT("Required Windows API Level: {}\n")(mpt::OS::Windows::Version::GetName(mpt::OS::Windows::Version::GetMinimumAPILevel()));
#endif // MPT_OS_WINDOWS
			{
				text += UL_("Required CPU features: ");
				std::vector<mpt::ustring> features;
				#ifdef MPT_ENABLE_ARCH_INTRINSICS
					#if MPT_ARCH_AMD64
						features.push_back(UL_("x86-64"));
						if(mpt::arch::current::assumed_features() & mpt::arch::current::feature::avx) features.push_back(UL_("avx"));
						if(mpt::arch::current::assumed_features() & mpt::arch::current::feature::avx2) features.push_back(UL_("avx2"));
					#elif MPT_ARCH_X86
						if(mpt::arch::current::assumed_features() & mpt::arch::current::feature::sse) features.push_back(UL_("sse"));
						if(mpt::arch::current::assumed_features() & mpt::arch::current::feature::sse2) features.push_back(UL_("sse2"));
						if(mpt::arch::current::assumed_features() & mpt::arch::current::feature::avx) features.push_back(UL_("avx"));
						if(mpt::arch::current::assumed_features() & mpt::arch::current::feature::avx2) features.push_back(UL_("avx2"));
					#endif
				#endif
				text += mpt::join_format(features, U_(" "));
				text += lf;
			}
#ifdef MPT_ENABLE_ARCH_INTRINSICS
			text += MPT_UFORMAT("Optional CPU features used: {}\n")(CPUFeaturesToString(CPU::GetEnabledFeatures()));
#endif // MPT_ENABLE_ARCH_INTRINSICS
			text += lf;
			text += MPT_UFORMAT("System Architecture: {}\n")(mpt::OS::Windows::Name(mpt::OS::Windows::GetHostArchitecture()));
#ifdef MPT_ENABLE_ARCH_INTRINSICS
#if MPT_ARCH_X86 || MPT_ARCH_AMD64
			text += MPT_UFORMAT("CPU: {}, Family {}, Model {}, Stepping {} ({}/{})\n")
				( mpt::ToUnicode(mpt::Charset::ASCII, (CPUInfo.get_vendor_string().length() > 0) ? CPUInfo.get_vendor_string() : std::string("Generic"))
				, CPUInfo.get_family()
				, CPUInfo.get_model()
				, CPUInfo.get_stepping()
				, mpt::ToUnicode(mpt::Charset::ASCII, CPUInfo.get_vendor_string())
				, mpt::ufmt::hex0<8>(CPUInfo.get_cpuid())
				);
			text += MPT_UFORMAT("CPU Name: {}\n")(mpt::ToUnicode(mpt::Charset::ASCII, (CPUInfo.get_brand_string().length() > 0) ? CPUInfo.get_brand_string() : std::string("")));
#endif
			text += MPT_UFORMAT("Available CPU features: {}\n")(CPUFeaturesToString(CPUInfo.get_features()));
#endif // MPT_ENABLE_ARCH_INTRINSICS
#if MPT_OS_WINDOWS
			text += MPT_UFORMAT("Operating System: {}\n\n")(mpt::OS::Windows::Version::GetCurrentName());
#else
			{
				struct utsname name{};
				::uname(&name);
				text += MPT_UFORMAT("Operating System: {} {} ({})\n\n")(mpt::ToUnicode(mpt::Charset::UTF8, name.sysname), mpt::ToUnicode(mpt::Charset::UTF8, name.release), mpt::ToUnicode(mpt::Charset::UTF8, name.machine));
			}
#endif // MPT_OS_WINDOWS
			text += MPT_UFORMAT("OpenMPT Install Path{1}: {0}\n")(theApp.GetInstallPath(), theApp.IsPortableMode() ? UV_(" (portable)") : UV_(""));
			text += MPT_UFORMAT("OpenMPT Executable Path{1}: {0}\n")(theApp.GetInstallBinArchPath(), theApp.IsPortableMode() ? UV_(" (portable)") : UV_(""));
			text += MPT_UFORMAT("Settings{1}: {0}\n")(theApp.GetConfigFileName(), theApp.IsPortableMode() ? UV_(" (portable)") : UV_(""));
			break;
		case 1:
			{
			std::vector<std::string> components = ComponentManager::Instance()->GetRegisteredComponents();
			if(!TrackerSettings::Instance().ComponentsKeepLoaded)
				{
					text += UL_("Components are loaded and unloaded as needed.\n\n");
					for(const auto &component : components)
					{
						ComponentInfo info = ComponentManager::Instance()->GetComponentInfo(component);
						mpt::ustring name = mpt::ToUnicode(mpt::Charset::ASCII, (info.name.substr(0, 9) == "Component") ? info.name.substr(9) : info.name);
						if(!info.settingsKey.empty())
						{
							name = mpt::ToUnicode(mpt::Charset::ASCII, info.settingsKey);
						}
						text += name + lf;
					}
				} else
				{
					for(int available = 1; available >= 0; --available)
					{
						if(available)
						{
							text += UL_("Loaded Components:\n");
						} else
						{
							text += UL_("\nUnloaded Components:\n");
						}
						for(const auto &component : components)
						{
							ComponentInfo info = ComponentManager::Instance()->GetComponentInfo(component);
							if(available  && info.state != ComponentStateAvailable) continue;
							if(!available && info.state == ComponentStateAvailable) continue;
							mpt::ustring name = mpt::ToUnicode(mpt::Charset::ASCII, (info.name.substr(0, 9) == "Component") ? info.name.substr(9) : info.name);
							if(!info.settingsKey.empty())
							{
								name = mpt::ToUnicode(mpt::Charset::ASCII, info.settingsKey);
							}
							text += MPT_UFORMAT("{}: {}")
								( name
								, info.state == ComponentStateAvailable ? UV_("ok") :
									info.state == ComponentStateUnavailable? UV_("missing") :
									info.state == ComponentStateUnintialized ? UV_("not loaded") :
									info.state == ComponentStateBlocked ? UV_("blocked") :
									info.state == ComponentStateUnregistered ? UV_("unregistered") :
									UV_("unknown")
								);
							if(info.type != ComponentTypeUnknown)
							{
								text += MPT_UFORMAT(" ({})")
									( info.type == ComponentTypeBuiltin ? UV_("builtin") :
										info.type == ComponentTypeSystem ? UV_("system") :
										info.type == ComponentTypeSystemInstallable ? UV_("system, optional") :
										info.type == ComponentTypeBundled ? UV_("bundled") :
										info.type == ComponentTypeForeign ? UV_("foreign") :
										UV_("unknown")
									);
							}
							text += lf;
						}
					}
				}
			}
			break;
		case 2:
			text += Build::GetFullCreditsString();
			break;
		case 3:
			text += Build::GetLicenseString();
			break;
		case 4:
			text += UL_("Website:\n") + Build::GetURL(Build::Url::Website);
			text += UL_("\n\nForum:\n") + Build::GetURL(Build::Url::Forum);
			text += UL_("\n\nBug Tracker:\n") + Build::GetURL(Build::Url::Bugtracker);
			text += UL_("\n\nUpdates:\n") + Build::GetURL(Build::Url::Updates);
			break;
#if MPT_OS_WINDOWS
		case 5:
			try
			{
				if(!theApp.GetWine())
				{
					text += UL_("Wine integration not available.\n");
				} else
				{

					mpt::Wine::Context & wine = *theApp.GetWine();
					
					text += MPT_UFORMAT("Windows: {}\n")
						( mpt::osinfo::windows::Version::Current().IsWindows() ? yes : no
						);
					text += MPT_UFORMAT("Windows version: {}\n")
						( 
						mpt::osinfo::windows::Version::Current().IsAtLeast(mpt::osinfo::windows::Version::Win10, 22000) ? UV_("Windows 11") :
						mpt::osinfo::windows::Version::Current().IsAtLeast(mpt::osinfo::windows::Version::Win10) ? UV_("Windows 10") :
						mpt::osinfo::windows::Version::Current().IsAtLeast(mpt::osinfo::windows::Version::Win81) ? UV_("Windows 8.1") :
						mpt::osinfo::windows::Version::Current().IsAtLeast(mpt::osinfo::windows::Version::Win8) ? UV_("Windows 8") :
						mpt::osinfo::windows::Version::Current().IsAtLeast(mpt::osinfo::windows::Version::Win7) ? UV_("Windows 7") :
						mpt::osinfo::windows::Version::Current().IsAtLeast(mpt::osinfo::windows::Version::WinVista) ? UV_("Windows Vista") :
						mpt::osinfo::windows::Version::Current().IsAtLeast(mpt::osinfo::windows::Version::WinXP) ? UV_("Windows XP") :
						mpt::osinfo::windows::Version::Current().IsAtLeast(mpt::osinfo::windows::Version::Win2000) ? UV_("Windows 2000") :
						mpt::osinfo::windows::Version::Current().IsAtLeast(mpt::osinfo::windows::Version::WinNT4) ? UV_("Windows NT4") :
						UV_("unknown")
						);
					text += MPT_UFORMAT("Windows original: {}\n")
						( mpt::OS::Windows::IsOriginal() ? yes : no
						);

					text += UL_("\n");

					text += MPT_UFORMAT("Wine: {}\n")
						( mpt::OS::Windows::IsWine() ? yes : no
						);
					text += MPT_UFORMAT("Wine Version: {}\n")
						( mpt::ToUnicode(mpt::Charset::UTF8, wine.VersionContext().RawVersion())
						);
					text += MPT_UFORMAT("Wine Build ID: {}\n")
						( mpt::ToUnicode(mpt::Charset::UTF8, wine.VersionContext().RawBuildID())
						);
					text += MPT_UFORMAT("Wine Host Sys Name: {}\n")
						( mpt::ToUnicode(mpt::Charset::UTF8, wine.VersionContext().RawHostSysName())
						);
					text += MPT_UFORMAT("Wine Host Release: {}\n")
						( mpt::ToUnicode(mpt::Charset::UTF8, wine.VersionContext().RawHostRelease())
						);

					text += UL_("\n");

					text += MPT_UFORMAT("uname -m: {}\n")
						( mpt::ToUnicode(mpt::Charset::UTF8, wine.Uname_m())
						);
					text += MPT_UFORMAT("HOME: {}\n")
						( mpt::ToUnicode(mpt::Charset::UTF8, wine.HOME())
						);
					text += MPT_UFORMAT("XDG_DATA_HOME: {}\n")
						( mpt::ToUnicode(mpt::Charset::UTF8, wine.XDG_DATA_HOME())
						);
					text += MPT_UFORMAT("XDG_CACHE_HOME: {}\n")
						( mpt::ToUnicode(mpt::Charset::UTF8, wine.XDG_CACHE_HOME())
						);
					text += MPT_UFORMAT("XDG_CONFIG_HOME: {}\n")
						( mpt::ToUnicode(mpt::Charset::UTF8, wine.XDG_CONFIG_HOME())
						);

					text += UL_("\n");

					text += MPT_UFORMAT("OpenMPT folder: {}\n")
						( theApp.GetInstallPath().ToUnicode()
						);
					text += MPT_UFORMAT("OpenMPT folder (host): {}\n")
						( mpt::ToUnicode(mpt::Charset::UTF8, wine.PathToPosix(theApp.GetInstallPath()))
						);
					text += MPT_UFORMAT("OpenMPT config folder: {}\n")
						( theApp.GetConfigPath().ToUnicode()
						);
					text += MPT_UFORMAT("OpenMPT config folder (host): {}\n")
						( mpt::ToUnicode(mpt::Charset::UTF8, wine.PathToPosix(theApp.GetConfigPath()))
						);
					text += MPT_UFORMAT("Host root: {}\n")
						( wine.PathToWindows("/").ToUnicode()
						);

				}
			} catch(const mpt::Wine::Exception & e)
			{
				text += UL_("Exception: ") + mpt::get_exception_text<mpt::ustring>(e) + UL_("\n");
			}
			break;
#endif // MPT_OS_WINDOWS
	}
	return text;
}


void CAboutDlg::DoDataExchange(DataExchange &dx)
{
	DialogBase::DoDataExchange(dx);
	ExchangeControl(dx, IDC_TABABOUT, m_Tab);
	ExchangeControl(dx, IDC_EDITABOUT, m_TabEdit);
}


void CAboutDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	// ON_WM_TIMER() (mfcless::Window)
	AddNotify(IDC_TABABOUT, mfcless::Notify::SelChange, &CAboutDlg::OnTabChange);
}



OPENMPT_NAMESPACE_END
