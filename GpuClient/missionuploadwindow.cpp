#include "missionuploadwindow.h"
#include "ui_missionuploadwindow.h"
#include <QFileDialog>

MissionUploadWindow::MissionUploadWindow(QWidget *parent)
    : QWidget(parent), ui(new Ui::MissionUploadWindow)
{
    ui->setupUi(this);

    connect(ui->btnUploadMission, &QPushButton::clicked, this, [this](){
        // 先做最小：挑檔，顯示檔名
        const QString path = QFileDialog::getOpenFileName(this, "Select mission file");
        if (!path.isEmpty()) {
            ui->lblSelectedFile->setText("Selected: " + path);
        }
        emit uploadRequested();
    });
}

MissionUploadWindow::~MissionUploadWindow(){ delete ui; }

void MissionUploadWindow::setSelectedMachine(const Machine& m)
{
    ui->infoName->setText("Name: " + m.name);
    ui->infoGpu->setText("GPU: " + m.gpuModel);
    ui->infoIP->setText("VRAM: " + m.vram);  // 你的 Machine 沒有 IP，所以先顯示 VRAM 很合理
}