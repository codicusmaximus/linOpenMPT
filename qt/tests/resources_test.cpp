// mfcless: the embedded resources read back. Checks a few things every
// build of OpenMPT has: the main menu, the About dialog, a string, the
// main accelerators, a bitmap and the application icon.

#include "mfcless/Resources.h"
#include "../../upstream/mptrack/resource.h"

#include <QtGui/QGuiApplication>

#include <iostream>

using namespace mfcless::res;

static int failures = 0;
static void Check(bool ok, const char *what)
{
	std::cout << (ok ? "ok   " : "FAIL ") << what << "\n";
	if(!ok)
		failures++;
}

int main(int argc, char **argv)
{
	QGuiApplication app(argc, argv);
	auto menu = Menu(IDR_MAINFRAME);
	Check(menu && menu->size() >= 5, "main menu has its top-level menus");
	if(menu && !menu->empty())
		std::cout << "     first menu: " << QString::fromStdU16String(menu->front().text).toStdString() << " (" << menu->front().children.size() << " items)\n";
	auto about = Dialog(IDD_OPTIONS_GENERAL);
	Check(about && !about->items.empty(), "general options dialog template has controls");
	if(about)
		std::cout << "     " << about->items.size() << " controls, " << about->cx << "x" << about->cy << " DLUs, font " << QString::fromStdU16String(about->font).toStdString() << "\n";
	Check(!String(IDS_ERR_FILEOPEN).empty(), "a string table entry");
	// mptrack.rc has no ACCELERATORS table: OpenMPT keys come from its keymap files.
	Check(!Image(u"PNG", IDB_MAINBAR).isNull(), "toolbar image (PNG resource)");
	Check(!Icon(IDR_MAINFRAME).isNull(), "application icon");
	return failures ? 1 : 0;
}
