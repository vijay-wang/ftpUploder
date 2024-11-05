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
	if (type == "information")
		QMessageBox::information(this, title, message);
	else if (type == "warning")
		QMessageBox::warning(this, title, message);
}

void FirmwareTransfer::startUpload(void)
{
	this->setEnabled(false);
	uploadThread = QThread::create([this]() { this->upload_thread_cb(); });
	uploadThread->start();
	progressThread = QThread::create([this]() { this->progress_thread_cb(); });
	progressThread->start();

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

void FirmwareTransfer::upload_thread_cb()
{
	QString ip = ui->lineEditIP->text();
	QString local_path = ui->lineEditPath->text();
	QFileInfo fileInfo(local_path);
	QString ftpUrl = "ftp://" + ip + "/userdata/" + fileInfo.fileName();
	qDebug() << ftpUrl << "\n";
	qDebug() << local_path << "\n";
	bool ret = uploadFileToFTP(local_path, ftpUrl, "root", "unit123");
	if (!ret) {
		emit requestShowMessage("warning", "ERROR", "Upload firmware failed");
		qDebug("Upload firmware failed\n");
	} else {
		emit requestShowMessage("infomation", "infomation", "The device will restart to complete the upgrade. \nThe upgrade will take 5-10 minutes, please do not power off.");
		qDebug("\n");
	}
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

	// connect(this, &FirmwareTransfer::rUpdateProgress, this, &FirmwareTransfer::updateProgress);

	while (uploadThread->isRunning()) {
		// emit this->rUpdateProgress(uploadProgress);
		progressDialog->setValue(uploadProgress);
		if (uploadProgress == 100)
			break;
		Sleep(100);
	}
}

int FirmwareTransfer::getVersionFile(void)
{
	QString ip = ui->lineEditIP->text();
	QString local_path = "./.tmp_version";
	QString ftpUrl = "ftp://" + ip + "/etc/issue";
	qDebug() << ftpUrl << "\n";
	qDebug() << local_path << "\n";


	return ftpDownloadFile(ftpUrl.toUtf8().data(), local_path.toUtf8().data(), "root", "unit123", 21);
}

static int extract_version_from_tmpfile(const char *filepath, char *sdk_version, size_t len);
void FirmwareTransfer::on_btnUpgrade_clicked()
{

	if (getVersionFile() == 0) {
		qDebug("File downloaded successfully.\n");
	} else {
		emit requestShowMessage("warning", "warning", "Get version failed");
	}

	const char *tmp_version_path = TMP_VERSION_FILE;
	const char *filepath = ui->lineEditPath->text().toLatin1().data();

	int v_sts = compare_sdk_versions(tmp_version_path, filepath);
	if (v_sts == -1)
		qDebug("Failed to extract SDK version\n");

	char dev_version[8] = {0};
	extract_version_from_tmpfile(TMP_VERSION_FILE, dev_version, 8);
	deleteFile(TMP_VERSION_FILE);

	if (v_sts == VERSION_NEWER) {
		qDebug("Stat to upgrade firmware\n");
		startUpload();
	} else if (v_sts == VERSION_OLDER || v_sts == VERSION_EQUAL) {
		char info[512] = {0};
		sprintf(info, "The device's SDK version(%s) is newer than or same as the version of %s, the upgrade operation will not be performed", dev_version, filepath);
		qDebug("%s\n", info);
		emit requestShowMessage("information", "information", info);
	} else {
		emit requestShowMessage("information", "information", "Extract version infomation failed, please check your firmware path");
	}
}

void FirmwareTransfer::updateProgress(float progress)
{
	progressDialog->setValue(uploadProgress);
}

// 回调函数，用于写入下载的数据
size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	size_t written = fwrite(ptr, size, nmemb, stream);
	return written;
}

int FirmwareTransfer::ftpDownloadFile(const char *ftpUrl, const char *savePath, const char *username, const char *password, int port) {
	CURL *curl;
	CURLcode res;
	FILE *file;

	// 打开文件用于写入（确保文件指针有效）
	file = fopen(savePath, "wb");
	if (!file) {
		fprintf(stderr, "Failed to open file: %s\n", savePath);
		return 1;
	}

	// 全局初始化
	if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
		fprintf(stderr, "curl_global_init() failed\n");
		fclose(file);
		return 1;
	}

	curl = curl_easy_init();

	// curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	if (!curl) {
		fprintf(stderr, "Failed to initialize CURL.\n");
		fclose(file);
		curl_global_cleanup();
		return 1;
	}

	// 设置 URL
	curl_easy_setopt(curl, CURLOPT_URL, ftpUrl);

	// 设置端口号
	curl_easy_setopt(curl, CURLOPT_PORT, port);

	// 设置用户名和密码
	char userpwd[128];
	snprintf(userpwd, sizeof(userpwd), "%s:%s", username, password);
	curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);

	// 禁用 EPSV（使用 PASV 模式）
	curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 0L);

	// 设置写入文件的回调函数
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

	// 执行文件下载
	res = curl_easy_perform(curl);

	// 检查结果
	if (res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		fclose(file);
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		return 1;
	}

	// 释放资源
	curl_easy_cleanup(curl);
	fclose(file);
	curl_global_cleanup();
	return 0;
}


// Helper function to compare version strings like "1.2.3"
static int compare_versions(const char *v1, const char *v2) {
	int major1, minor1, patch1;
	int major2, minor2, patch2;

	if (sscanf(v1, "%d.%d.%d", &major1, &minor1, &patch1) != 3) return -1;
	if (sscanf(v2, "%d.%d.%d", &major2, &minor2, &patch2) != 3) return -1;

	if (major1 > major2) return 2;
	if (major1 < major2) return 3;
	if (minor1 > minor2) return 2;
	if (minor1 < minor2) return 3;
	if (patch1 > patch2) return 2;
	if (patch1 < patch2) return 3;

	return 1;
}

// Function to extract SDK version from file name
static int extract_version_from_filename(const char *filepath, char *sdk_version, size_t len) {
	// Locate the last backslash or forward slash to find the file name
	const char *filename = strrchr(filepath, '\\');
	if (!filename) filename = strrchr(filepath, '/');
	if (!filename) filename = filepath; // No path, assume full string is the file name
	else filename++; // Move past the backslash or forward slash

	const char *sdk_start = strstr(filename, "SdkV");
	if (!sdk_start) return -1;

	sdk_start += strlen("SdkV");
	const char *sdk_end = strchr(sdk_start, '-');
	if (!sdk_end) sdk_end = strchr(sdk_start, '.');  // Handle end of file name

	if (!sdk_end || sdk_end - sdk_start >= len) return -1;

	strncpy(sdk_version, sdk_start, sdk_end - sdk_start);
	sdk_version[sdk_end - sdk_start] = '\0';
	return 0;
}

// Function to extract SDK version from .tmp_version file
static int extract_version_from_tmpfile(const char *filepath, char *sdk_version, size_t len) {
	FILE *file = fopen(filepath, "r");
	if (!file) return -1;

	char buffer[256];
	if (!fgets(buffer, sizeof(buffer), file)) {
		fclose(file);
		return -1;
	}
	fclose(file);

	const char *sdk_start = strstr(buffer, "sdk:");
	if (!sdk_start) return -1;

	sdk_start += strlen("sdk:");
	const char *sdk_end = strchr(sdk_start, ':');
	if (!sdk_end) sdk_end = strchr(sdk_start, '\n');  // Handle end of line

	if (!sdk_end || sdk_end - sdk_start >= len) return -1;

	strncpy(sdk_version, sdk_start, sdk_end - sdk_start);
	sdk_version[sdk_end - sdk_start] = '\0';
	return 0;
}

// Main function to compare versions
int FirmwareTransfer::compare_sdk_versions(const char *tmp_version_path, const char *filepath) {
	char tmp_sdk_version[16], file_sdk_version[16];

	if (extract_version_from_tmpfile(tmp_version_path, tmp_sdk_version, sizeof(tmp_sdk_version)) == -1)
		return -1;
	if (extract_version_from_filename(filepath, file_sdk_version, sizeof(file_sdk_version)) == -1)
		return -1;

	return compare_versions(file_sdk_version, tmp_sdk_version);
}


bool FirmwareTransfer::deleteFile(const QString &filePath) {
	QFile file(filePath);
	if (file.exists()) {
		if (file.remove()) {
			qDebug() << "File deleted successfully.";
			return true;
		} else {
			qDebug() << "Failed to delete file.";
			return false;
		}
	} else {
		qDebug() << "File does not exist.";
		return false;
	}
}
