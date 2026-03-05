#include "providewindow.h"
#include "ui_providewindow.h"

ProvideWindow::ProvideWindow(QWidget *parent)
    : QWidget(parent), ui(new Ui::ProvideWindow)
{
    ui->setupUi(this);

    connect(ui->btnProvide, &QPushButton::clicked, this, [this](){
        emit provideRequested(
            ui->editLogin->text(),
            ui->editName->text(),
            ui->editGpu->text(),
            ui->editIP->text()
        );
    });
}

ProvideWindow::~ProvideWindow(){ delete ui; }