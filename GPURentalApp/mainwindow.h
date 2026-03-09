#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QPushButton>
#include <QNetworkAccessManager>

class QScrollArea;
class QVBoxLayout;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void handleProviderCardClicked();
    void refreshNodes();
    void submitJob();
    void pollJobStatus();
    void registerNode();
    void applyPrice();
    void deleteNode();

private:
    void setupConnections();
    QPushButton* createProviderCard(const QString& hostId,
                                    const QString& gpu,
                                    const QString& vram,
                                    double price,
                                    const QString& status);

    void addMyHostToTop(const QString& hostId,
                        const QString& gpu,
                        const QString& vram,
                        double price,
                        const QString& status);

private:
    Ui::MainWindow *ui = nullptr;
    QPushButton *currentProviderButton = nullptr;
    QNetworkAccessManager *nam = nullptr;
    QTimer *pollTimer = nullptr;
    QString managerIpPort;       
    QString selectedNodeIp;
    QString currentJobId;
    bool m_warnedThisJob = false;   
    QString m_warnReasonThisJob;    

};

#endif // MAINWINDOW_H