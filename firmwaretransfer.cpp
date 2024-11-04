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

static float uploadProgress = 0.0;
// 进度回调函数
int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
	if (ultotal > 0) { // 如果总大小已知
		double progress = (double)ulnow / (double)ultotal * 100.0;
		printf("\rTransfer file progress: %.2f %%", progress);
		uploadProgress = progress;
		fflush(stdout);
	}
	return 0; // 返回0表示继续传输
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

		// 设置进度回调函数
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L); // 启用进度显示

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

void FirmwareTransfer::showMessage(const QString type, const QString title, const QString message)
{
	if (type == "infomation")
		QMessageBox::information(this, title, message);
	else if (type == "warning")
		QMessageBox::warning(this, title, message);
}

void FirmwareTransfer::startUpload(void)
{
	uploadThread = QThread::create([this]() { this->upload_thread_cb(); });
	uploadThread->start();
	progressThread = QThread::create([this]() { this->progress_thread_cb(); });
	progressThread->start();

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
		emit requestShowMessage("warning", "ERROR", "Upload firmware failed");
	else
		emit requestShowMessage("infomation", "infomation", "Please reboot the device to complete the upgrade");
	uploadProgress = 0;
}

void FirmwareTransfer::progress_thread_cb()
{
	progressDialog = new QProgressDialog(this);
	progressDialog->setMinimumWidth(300);               	// 设置最小宽度
	progressDialog->setMaximumWidth(300);               	// 设置最小宽度
	// progressDialog.setWindowModality(Qt::NonModal);	// 非模态，其它窗口正常交互  Qt::WindowModal 模态
	progressDialog->setWindowModality(Qt::WindowModal);	// 非模态，其它窗口正常交互  Qt::WindowModal 模态
	progressDialog->setMinimumDuration(0);          	// 等待0秒后显示
	progressDialog->setWindowTitle(tr("Progress"));      	// 标题名
	progressDialog->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
	progressDialog->setLabelText(tr("Upload Progress"));    // 标签的
	// progressDialog.setCancelButtonText(tr("不读了"));   	// 取消按钮
	progressDialog->setCancelButton(nullptr);   		// 不显示按钮
	progressDialog->setRange(0, 100);

	connect(this, &FirmwareTransfer::rUpdateProgress, this, &FirmwareTransfer::updateProgress);
	while (uploadThread->isRunning()) {
		emit this->rUpdateProgress(uploadProgress);
		if (uploadProgress == 100)
			break;
		Sleep(50);
	}
}

void FirmwareTransfer::on_btnUpgrade_clicked()
{
	this->setEnabled(false);
	startUpload();
	connect(uploadThread, &QThread::finished, this, [this]() {
		qDebug() << "uploadThread finished. isRunning:" << uploadThread->isRunning();
	});

	connect(progressThread, &QThread::finished, this, [this]() {
		uploadThread->wait();  // 确保线程完全结束
		progressThread->wait();  // 确保线程完全结束
		qDebug() << "progressThread finished. isRunning:" << progressThread->isRunning();
		this->setEnabled(true);
	});
}

void FirmwareTransfer::updateProgress(float progress)
{
	progressDialog->setValue(uploadProgress);
}
