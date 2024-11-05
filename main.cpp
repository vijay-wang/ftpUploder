#include "firmwaretransfer.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QIcon>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	QTranslator translator;
	const QStringList uiLanguages = QLocale::system().uiLanguages();
	for (const QString &locale : uiLanguages) {
		const QString baseName = "FirmwareTransfer_" + QLocale(locale).name();
		if (translator.load(":/i18n/" + baseName)) {
			a.installTranslator(&translator);
			break;
		}
	}
	FirmwareTransfer w;
	// QIcon icon("C:\\Users\\ww107\\Documents\\FirmwareTransfer\build\\Desktop_Qt_6_5_3_MinGW_64_bit-Debug\\debug\\icon.png");
	// w.setWindowIcon(icon);
	w.show();
	return a.exec();
}
