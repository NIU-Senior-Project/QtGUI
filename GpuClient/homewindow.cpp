#include "homewindow.h"
#include "ui_homewindow.h"

HomeWindow::HomeWindow(QWidget *parent)
    : QWidget(parent), ui(new Ui::HomeWindow)
{
    ui->setupUi(this);

    connect(ui->btnGoRent, &QPushButton::clicked, this, &HomeWindow::goRent);
    connect(ui->btnGoProvide, &QPushButton::clicked, this, &HomeWindow::goProvide);

    // 你 Home UI 沒有 debug 按鈕也沒關係，之後要加再接
}

HomeWindow::~HomeWindow(){ delete ui; }