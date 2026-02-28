#include "rentwindow.h"
#include "ui_rentwindow.h"
#include <QPushButton>

RentWindow::RentWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RentWindow)
{
    ui->setupUi(this);

    connect(ui->btnRefresh, &QPushButton::clicked,
            this, &RentWindow::refreshRequested);

    connect(ui->btnRent_0, &QPushButton::clicked, this, [this](){
        qDebug() << "RentWindow this =" << this;
        emit rentClicked(0);  // ✅ 不管有沒有 machines，按了就發
    });
}

RentWindow::~RentWindow()
{
    delete ui;
}

void RentWindow::setMachines(const QList<Machine>& machines)
{
    ui->machineCard_0->setVisible(!machines.isEmpty());
    if (machines.isEmpty()) return;

    const Machine mCopy = machines.first();

    ui->lblName_0->setText(mCopy.name);
    ui->lblGpu_0->setText("GPU: " + mCopy.gpuModel + " / " + mCopy.vram);

    disconnect(ui->btnRent_0, nullptr, nullptr, nullptr);
    connect(ui->btnRent_0, &QPushButton::clicked, this, [this, mCopy](){
        emit machineSelected(mCopy);
    });
}