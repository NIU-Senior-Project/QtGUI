#pragma once
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class HomeWindow; }
QT_END_NAMESPACE

class HomeWindow : public QWidget {
    Q_OBJECT
public:
    explicit HomeWindow(QWidget *parent=nullptr);
    ~HomeWindow();

signals:
    void goRent();
    void goProvide();
    void goDebug();

private:
    Ui::HomeWindow *ui;
};