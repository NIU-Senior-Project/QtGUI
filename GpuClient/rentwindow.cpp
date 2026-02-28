#include "rentwindow.h"
#include "ui_rentwindow.h"

RentWindow::RentWindow(QWidget *parent)
    : QWidget(parent), ui(new Ui::RentWindow)
{
    ui->setupUi(this);
    connect(ui->btnRefresh, &QPushButton::clicked, this, &RentWindow::refreshRequested);

    // 先用你 ui 裡的示意卡 btnRent_0 來當「選擇第一台」
    connect(ui->btnRent_0, &QPushButton::clicked, this, [this](){
        // 真正 machines 在 MainWindow 會填，這裡先空著
    });
}

RentWindow::~RentWindow(){ delete ui; }

void RentWindow::setMachines(const QList<Machine>& machines)
{
    // 最小先把第 0 張示意卡更新成第一台（你之後要做動態卡片我也能幫你補）
    if (machines.isEmpty()) return;

    const Machine& m = machines.first();
    ui->lblName_0->setText(m.name);
    ui->lblGpu_0->setText("GPU: " + m.gpuModel + " / " + m.vram);

    // 讓按鈕真的能選到 m
    disconnect(ui->btnRent_0, nullptr, nullptr, nullptr);
    connect(ui->btnRent_0, &QPushButton::clicked, this, [this, m](){
        emit machineSelected(m);
    });
}