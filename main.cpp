#include "firmwaretransfer.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QIcon>
#include <QFile>
#include <QDate>

QFile logFile;

void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
	// 打开日志文件（追加模式）
	if (!logFile.isOpen()) {
		logFile.setFileName("application.log"); // 设置日志文件名
		logFile.open(QIODevice::WriteOnly | QIODevice::Append);
	}

	// 创建输出流
	QTextStream out(&logFile);
	out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ") << " ";

	// 根据消息类型，添加前缀信息
	switch (type) {
	case QtDebugMsg:
		out << "[DEBUG] ";
		break;
	case QtInfoMsg:
		out << "[INFO] ";
		break;
	case QtWarningMsg:
		out << "[WARNING] ";
		break;
	case QtCriticalMsg:
		out << "[CRITICAL] ";
		break;
	case QtFatalMsg:
		out << "[FATAL] ";
		abort();
	}

	// 写入消息内容和文件、行号
	out << msg << " (" << context.file << ":" << context.line << ")\n";
	out.flush();  // 刷新日志文件

	// 输出到终端（标准输出）
	if (type == QtDebugMsg || type == QtInfoMsg || type == QtWarningMsg || type == QtCriticalMsg) {
		QTextStream(stdout) << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ") << " "
				    << msg << " (" << context.file << ":" << context.line << ")\n";
	}
}

int main(int argc, char *argv[])
{
	int ret;
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
	qInstallMessageHandler(customMessageHandler);
	FirmwareTransfer w;
	QIcon icon("./icon/UNIT.ico");
	w.setWindowIcon(icon);
	w.show();
	ret = a.exec();
	logFile.close();
	return ret;
}
