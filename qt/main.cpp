// mfcless: the tracker's entry point. Stage 0 placeholder: proves that the
// engine (upstream's soundlib in tracker configuration) links without MFC.

#include "soundlib/Sndfile.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	app.setApplicationName(QStringLiteral("OpenMPT"));
	QMainWindow window;
	window.setWindowTitle(QStringLiteral("OpenMPT (mfcless)"));
	window.resize(1024, 768);
	window.show();
	return app.exec();
}
