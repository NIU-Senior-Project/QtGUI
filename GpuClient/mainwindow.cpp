#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QHostAddress>
#include <QTableWidgetItem>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    ,socket(new QTcpSocket(this))
{
    ui->setupUi(this);

    //設定表格欄位
    ui->tableWidget->setColumnCount(4);
    QStringList headers;
    headers << "主機" << "GPU型號" << "容量" << "狀態";
    ui->tableWidget->setHorizontalHeaderLabels(headers);

    //連結socket訊號
    connect(socket, &QTcpSocket::connected,
            this, &MainWindow::onConnected);
    connect(socket, &QTcpSocket::readyRead,
            this, &MainWindow::onReadyRead);
    connect(socket, &QTcpSocket::errorOccurred,
            this, &MainWindow::onErrorOccurred);

    //嘗試連到本機 127.0.0.1 的5000port
    socket->connectToHost(QHostAddress("127.0.0.1"), 5000);
    ui->statusbar->showMessage("正在連線到伺服器...");

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onConnected(){
    ui->statusbar->showMessage("已連線到伺服器");
    // 未來如果要主動要資料，可以：  socket->write("GET_MACHINES\n");
}

void MainWindow::onReadyRead(){

    //把收到的資料累加到buffer
    buffer.append(socket->readAll());

    int index;
    //以換行(\n)作為一筆資料
    while((index = buffer.indexOf('\n')) != -1){
        QByteArray line = buffer.left(index);
        buffer.remove(0, index +1);

        if(line == "END"){
            //目前先用END當作資料結束標記
            ui->statusbar->showMessage("已接收主機清單");
            continue;
        }

        if(line.isEmpty())
            continue;

        QList<QByteArray> parts = line.split(',');
        if(parts.size() <4)
            continue;

        int row = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(row);
        ui->tableWidget->setItem(row, 0, new QTableWidgetItem(QString::fromUtf8(parts[0])));
        ui->tableWidget->setItem(row, 1, new QTableWidgetItem(QString::fromUtf8(parts[1])));
        ui->tableWidget->setItem(row, 2, new QTableWidgetItem(QString::fromUtf8(parts[2])));
        ui->tableWidget->setItem(row, 3, new QTableWidgetItem(QString::fromUtf8(parts[3])));
    }
}

void MainWindow::onErrorOccurred(QAbstractSocket::SocketError){
    ui->statusbar->showMessage("連線錯誤:" + socket->errorString());
}
