#include "firmwaretransfer.h"
#include "ui_firmwaretransfer.h"
#include <QFileDialog>

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
}

FirmwareTransfer::~FirmwareTransfer()
{
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


void FirmwareTransfer::on_btnUpgrade_clicked()
{

}

