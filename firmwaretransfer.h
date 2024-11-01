#ifndef FIRMWARETRANSFER_H
#define FIRMWARETRANSFER_H

#include <QDialog>
#include <QProgressDialog>
#include <QSettings>
#include <QThread>
#include <curl/curl.h>

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
	friend int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

signals:
	void requestShowMessage(const QString &title, const QString &message);
	void requestUpdateProgress(float progress);

private slots:

	void on_btnBrowse_clicked();

	void on_btnSet_clicked();

	void on_btnUpgrade_clicked();

	void showMessage(const QString title, const QString message);

	void updateProgress(float progress);

private:
	bool uploadFileToFTP(const QString localFilePath, const QString ftpUrl, const QString username, const QString password);
	Ui::FirmwareTransfer *ui;
	QSettings *configFile;
	QThread *uploadThread = nullptr;
	void upload_thread_cb();
	void startUpload(void);
	QProgressDialog *progressDialog;
};
#endif // FIRMWARETRANSFER_H
