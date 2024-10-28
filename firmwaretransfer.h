#ifndef FIRMWARETRANSFER_H
#define FIRMWARETRANSFER_H

#include <QDialog>
#include <QSettings>

#ifdef QT_DEBUG
#define CONFIG_FILE "C:\\Users\\ww107\\Desktop\\share\\Upgrade\\config.ini"
#else
#define CONFIG_FILE "config.ini"
#endif

#define NET_SECTION_NAME "/NET/"
#define FILE_SECTION_NAME "/FILE/"

QT_BEGIN_NAMESPACE
namespace Ui {
class FirmwareTransfer;
}
QT_END_NAMESPACE

class FirmwareTransfer : public QDialog
{
	Q_OBJECT

public:
	FirmwareTransfer(QWidget *parent = nullptr);
	~FirmwareTransfer();

private slots:

	void on_btnBrowse_clicked();

	void on_btnSet_clicked();

	void on_btnUpgrade_clicked();

private:
	Ui::FirmwareTransfer *ui;
	QSettings *configFile;
};
#endif // FIRMWARETRANSFER_H
