#pragma once
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class ProvideWindow; }
QT_END_NAMESPACE

class ProvideWindow : public QWidget {
    Q_OBJECT
public:
    explicit ProvideWindow(QWidget *parent=nullptr);
    ~ProvideWindow();

signals:
    void provideRequested(QString login, QString name, QString gpu, QString ip);
    void goHome();
    void goDebug();

private:
    Ui::ProvideWindow *ui;
};