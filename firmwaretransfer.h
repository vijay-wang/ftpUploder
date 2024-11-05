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

signals:
	void requestShowMessage(const QString type, const QString &title, const QString &message);
	void rUpdateProgress(float progress);

private slots:

	void on_btnBrowse_clicked();

	void on_btnSet_clicked();

	void on_btnUpgrade_clicked();

	void showMessage(const QString type, const QString title, const QString message);

	void updateProgress(float progress);




private:
	int ftpDownloadFile(const char *ftpUrl, const char *savePath, const char *username, const char *password, int port);
	bool uploadFileToFTP(const QString localFilePath, const QString ftpUrl, const QString username, const QString password);
	Ui::FirmwareTransfer *ui;
	QSettings *configFile;
	QThread *uploadThread;
	QThread *progressThread;
	void upload_thread_cb();
	void progress_thread_cb();
	void startUpload(void);
	QProgressDialog *progressDialog;
	int getVersionFile(void);
	bool deleteFile(const QString &filePath);
	int compare_sdk_versions(const char *tmp_version_path, const char *filepath);
};

typedef enum {
	VERSION_FAILED = -1,
	VERSION_EQUAL = 1,
	VERSION_NEWER,
	VERSION_OLDER,

} VerionRes;

#define TMP_VERSION_FILE ".tmp_version"
#endif // FIRMWARETRANSFER_H
