#pragma once
#include <QWidget>
#include <QList>
#include "machine.h"

QT_BEGIN_NAMESPACE
namespace Ui { class RentWindow; }
QT_END_NAMESPACE

class RentWindow : public QWidget {
    Q_OBJECT
public:
    explicit RentWindow(QWidget *parent=nullptr);
    ~RentWindow();

    void setMachines(const QList<Machine>& machines);

signals:
    void refreshRequested();
    void machineSelected(const Machine& m);
    void goHome();
    void goDebug();

private:
    Ui::RentWindow *ui;
};