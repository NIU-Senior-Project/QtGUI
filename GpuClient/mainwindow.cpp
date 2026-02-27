#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "machineprovider_tcp.h"

#include <QTableWidgetItem>
#include <QStringList>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //設定表格欄位
    ui->tableWidget->setColumnCount(4);
    QStringList headers;
    headers << "主機" << "GPU型號" << "容量" << "狀態";
    ui->tableWidget->setHorizontalHeaderLabels(headers);

    provider = new TcpMachineProvider("127.0.0.1", 5000, this);

    connect(provider, &MachineProvider::machinesReady, this,
        [this](const QList<Machine>& machines){
            ui->tableWidget->setRowCount(0);
            for (const auto& m : machines) {
                int row = ui->tableWidget->rowCount();
                ui->tableWidget->insertRow(row);
                ui->tableWidget->setItem(row, 0, new QTableWidgetItem(m.name));
                ui->tableWidget->setItem(row, 1, new QTableWidgetItem(m.gpuModel));
                ui->tableWidget->setItem(row, 2, new QTableWidgetItem(m.vram));
                ui->tableWidget->setItem(row, 3, new QTableWidgetItem(m.status));
            }
            ui->statusbar->showMessage("已接收主機清單");
        });

    connect(provider, &MachineProvider::errorOccurred, this,
        [this](const QString& msg){
            ui->statusbar->showMessage("錯誤：" + msg);
        });

    ui->statusbar->showMessage("正在取得主機清單...");
    provider->fetchMachines();  
}

MainWindow::~MainWindow()
{
    delete ui;
}