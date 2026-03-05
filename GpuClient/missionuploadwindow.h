#pragma once
#include <QWidget>
#include "machine.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MissionUploadWindow; }
QT_END_NAMESPACE

class MissionUploadWindow : public QWidget {
    Q_OBJECT
public:
    explicit MissionUploadWindow(QWidget *parent=nullptr);
    ~MissionUploadWindow();

    void setSelectedMachine(const Machine& m);

signals:
    void uploadRequested();
    void goBackToRent();
    void goDebug();

private:
    Ui::MissionUploadWindow *ui;
};