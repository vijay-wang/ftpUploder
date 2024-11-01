#include "firmwaretransfer.h"
#include "ui_firmwaretransfer.h"
#include <QFileDialog>
#include <QDebug>
#include <QFileInfo>
#include <QMessageBox>

FirmwareTransfer::FirmwareTransfer(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FirmwareTransfer)
{
	ui->setupUi(this);
	configFile = new QSettings(CONFIG_FILE, QSettings::IniFormat);
	QString ip = configFile->value(NET_SECTION_NAME"ip").toString();
	QString path = configFile->value(FILE_SECTION_NAME"path").toString();
	ui->lineEditIP->setText(ip);
	ui->lineEditPath->setText(path);
	connect(this, &FirmwareTransfer::requestShowMessage, this, &FirmwareTransfer::showMessage);
}

FirmwareTransfer::~FirmwareTransfer()
{
	if (uploadThread && uploadThread->isRunning()) {
		uploadThread->quit();
		uploadThread->wait();
	}
	delete uploadThread;
	delete configFile;
	delete ui;
}

void FirmwareTransfer::on_btnBrowse_clicked()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("All files (*.*)"));
	if (!fileName.isEmpty()) {
		ui->lineEditPath->setText(fileName);
		QString path = ui->lineEditPath->text();
		configFile->setValue(FILE_SECTION_NAME"/path", path);
	}
}


void FirmwareTransfer::on_btnSet_clicked()
{
	QString ip = ui->lineEditIP->text();
	configFile->setValue(NET_SECTION_NAME"/ip", ip);
}

size_t read_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
	FILE *file = static_cast<FILE*>(stream);
	size_t retcode = fread(ptr, size, nmemb, file);
	return retcode;
}

bool FirmwareTransfer::uploadFileToFTP(const QString localFilePath, const QString ftpUrl, const QString username, const QString password) {
	static bool initialized = false;
	if (!initialized) {
		curl_global_init(CURL_GLOBAL_DEFAULT);
		initialized = true;
	}

	CURL *curl;
	CURLcode res;
	FILE *file = fopen(localFilePath.toStdString().c_str(), "rb");
	if (!file) {
		qDebug() << "Failed to open file for upload.";
		return false;
	}

	QFileInfo fileInfo(localFilePath);  // 获取文件信息
	curl = curl_easy_init();
	if(curl) {
		std::string ftpUrlStr = ftpUrl.toStdString();
		std::string userPwdStr = QString("%1:%2").arg(username, password).toStdString();

		// 设置必要的选项
		curl_easy_setopt(curl, CURLOPT_URL, ftpUrlStr.c_str());
		curl_easy_setopt(curl, CURLOPT_USERPWD, userPwdStr.c_str());
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

		// 设置回调函数
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
		curl_easy_setopt(curl, CURLOPT_READDATA, file);
		curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(fileInfo.size()));

		// 执行上传操作
		res = curl_easy_perform(curl);

		if(res != CURLE_OK) {
			qDebug() << "curl_easy_perform() failed:" << curl_easy_strerror(res);
		} else {
			qDebug() << "File uploaded successfully.";
		}

		curl_easy_cleanup(curl);
	} else {
		qDebug() << "curl_easy_init() failed.";
		fclose(file);
		return false;
	}

	fclose(file);

	return (res == CURLE_OK);
}

void FirmwareTransfer::showMessage(const QString title, const QString message)
{
	QMessageBox::information(this, title, message);
}

void FirmwareTransfer::startUpload(void)
{
	uploadThread = QThread::create([this]() { this->upload_thread_cb(); });
	uploadThread->start();
}

void FirmwareTransfer::upload_thread_cb()
{
	QString ip = ui->lineEditIP->text();
	QString local_path = ui->lineEditPath->text();
	QFileInfo fileInfo(local_path);
	QString ftpUrl = "ftp://" + ip + "/userdata/" + fileInfo.fileName();
	qDebug() << ftpUrl << "\n";
	qDebug() << local_path << "\n";
	bool ret = uploadFileToFTP(local_path, ftpUrl, "root", "unit123");
	if (!ret)
		emit requestShowMessage("ERROR", "upload firmware failed");
}

void FirmwareTransfer::on_btnUpgrade_clicked()
{
	startUpload();
}

