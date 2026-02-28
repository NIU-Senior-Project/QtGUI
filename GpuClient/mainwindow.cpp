#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "machineprovider_tcp.h"
#include "homewindow.h"
#include "rentwindow.h"
#include "providewindow.h"
#include "missionuploadwindow.h"

#include <QTableWidgetItem>
#include <QStringList>
#include <QShortcut>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // --- pages ---
    auto home = new HomeWindow(this);
    auto rent = new RentWindow(this);
    auto provide = new ProvideWindow(this);
    auto mission = new MissionUploadWindow(this);

    ui->layoutHome->addWidget(home);
    ui->layoutRent->addWidget(rent);
    ui->layoutProvide->addWidget(provide);
    ui->layoutMission->addWidget(mission);

    // --- debug table setup (保留你的原設定) ---
    ui->tableWidget->setColumnCount(4);
    QStringList headers;
    headers << "主機" << "GPU型號" << "容量" << "狀態";
    ui->tableWidget->setHorizontalHeaderLabels(headers);

    // --- provider (沿用你原本的 TCP) ---
    provider = new TcpMachineProvider("127.0.0.1", 5050, this);

    // 收到 machines：同時更新 Debug table + Rent page
    connect(provider, &MachineProvider::machinesReady, this,
        [this, rent](const QList<Machine>& machines){
            // Debug table (保留你原本邏輯)
            ui->tableWidget->setRowCount(0);
            for (const auto& m : machines) {
                int row = ui->tableWidget->rowCount();
                ui->tableWidget->insertRow(row);
                ui->tableWidget->setItem(row, 0, new QTableWidgetItem(m.name));
                ui->tableWidget->setItem(row, 1, new QTableWidgetItem(m.gpuModel));
                ui->tableWidget->setItem(row, 2, new QTableWidgetItem(m.vram));
                ui->tableWidget->setItem(row, 3, new QTableWidgetItem(m.status));
            }

            // Rent page（先最小：更新第 0 張示意卡）
            rent->setMachines(machines);

            ui->statusbar->showMessage("已接收主機清單");
        });

    connect(provider, &MachineProvider::errorOccurred, this,
        [this](const QString& msg){
            ui->statusbar->showMessage("錯誤：" + msg);
        });

    // --- navigation (stack index) ---
    // 0 Home, 1 Rent, 2 Provide, 3 Mission, 4 Debug
    connect(home, &HomeWindow::goRent, this, [this](){ ui->stack->setCurrentIndex(1); });
    connect(home, &HomeWindow::goProvide, this, [this](){ ui->stack->setCurrentIndex(2); });

    connect(rent, &RentWindow::refreshRequested, this, [this](){
        ui->statusbar->showMessage("正在取得主機清單...");
        provider->fetchMachines();
    });

    connect(rent, &RentWindow::machineSelected, this, [this, mission](const Machine& m){
        mission->setSelectedMachine(m);
        ui->stack->setCurrentIndex(3);
    });

    // --- debug shortcut ---
    // mac: Cmd+D, win/linux: Ctrl+D
    auto sc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), this);
    connect(sc, &QShortcut::activated, this, [this](){
        int cur = ui->stack->currentIndex();
        ui->stack->setCurrentIndex(cur == 4 ? 0 : 4);
    });

    // init
    ui->statusbar->showMessage("正在取得主機清單...");
    provider->fetchMachines();
}

MainWindow::~MainWindow()
{
    delete ui;
}