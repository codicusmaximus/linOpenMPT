// mfcless: builds every dialog template of mptrack.rc as Qt widgets. With an
// output directory, also saves a picture of each, named by resource ID, for
// comparing with the MFC build.

#include "mfcless/DialogBuilder.h"

#include <QtCore/QDir>
#include <QtCore/QSet>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <iostream>

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	const QString outDir = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
	if(!outDir.isEmpty())
		QDir().mkpath(outDir);
	int dialogs = 0, controls = 0, failures = 0;
	QSet<QString> unknown;
	for(const auto &name : mfcless::res::List(mfcless::res::ResourceType::Dialog))
	{
		if(!name.name.empty())
			continue;
		QWidget dlg;
		mfcless::BuiltDialog built;
		if(!mfcless::BuildDialog(&dlg, name.id, built))
		{
			std::cout << "FAIL dialog " << name.id << " did not build\n";
			failures++;
			continue;
		}
		dialogs++;
		controls += static_cast<int>(built.all.size());
		for(QWidget *w : built.all)
		{
			if(w->metaObject() == &QWidget::staticMetaObject)
				unknown.insert(w->property("mfcless_class").toString());
		}
		if(!outDir.isEmpty())
		{
			dlg.setAttribute(Qt::WA_DontShowOnScreen);
			dlg.show();
			dlg.grab().save(QStringLiteral("%1/%2.png").arg(outDir).arg(name.id));
		}
	}
	std::cout << (dialogs > 70 ? "ok   " : "FAIL ") << dialogs << " dialogs, " << controls << " controls built\n";
	if(dialogs <= 70)
		failures++;
	for(const QString &cls : unknown)
		std::cout << "     custom class left as a placeholder: " << cls.toStdString() << "\n";
	return failures ? 1 : 0;
}
