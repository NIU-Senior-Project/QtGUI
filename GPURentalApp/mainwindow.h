#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QTimer>
#include <QElapsedTimer>
#include <QPushButton>

class QScrollArea;
class QVBoxLayout;


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct GPUInfo
{
    QString model;
    QString vram;
    QString price;
    QString description;
};

Q_DECLARE_METATYPE(GPUInfo)

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
private slots:
    void handleProviderCardClicked();
    void handleRenterCardClicked();
private:
    void setupConnections();
    void populateMarket();
    QVector<GPUInfo> buildMockData() const;
    QVBoxLayout *ensureScrollAreaLayout(QScrollArea *area);
    QWidget *createMarketCard(const GPUInfo &info, bool renterList);
    void resetDetailPanel();
    void handleRenterCardClicked(const GPUInfo &info);
    void handleProviderCardClicked(const GPUInfo &info);
    void handleProviderMarketCardClicked(const GPUInfo &info);
    QPushButton* createProviderCard(const QString& hostId,
                                const QString& gpu,
                                const QString& vram,
                                double price,
                                const QString& status);
private:    
    Ui::MainWindow *ui = nullptr;

    
    QTimer *uploadTimer = nullptr;
    QElapsedTimer uploadElapsed;
    qint64 uploadTotalBytes = 0;
    qint64 uploadSentBytes = 0;

    QString currentProviderHostId;
    QVector<GPUInfo> m_gpuCatalog;
    bool m_hasActiveSelection = false;
};

#endif
