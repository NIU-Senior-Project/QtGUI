#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDebug>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QDoubleValidator> 
#include <utility>

namespace
{
    const QString kTestTask = QStringLiteral(
        "Task ID: T-20250125-001\n"
        "Workload: Stable Diffusion XL render\n"
        "Assets: /mnt/data/projects/sdxl/scene01\n"
        "Steps: 50, CFG scale: 7.5\n"
        "Expected output: 4k PNG x 12");
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    if (ui->edit_p_price) {
    ui->edit_p_price->setValidator(new QDoubleValidator(0, 999999, 2, ui->edit_p_price));
    ui->edit_p_price->setPlaceholderText("例如 35.00");
    }
    ui->vlay_provider_cards->addWidget(
        createProviderCard("B", "RTX 4070", "12GB", 35.0, "idle")
    );
    ui->vlay_provider_cards->addWidget(
        createProviderCard("C", "RTX 3080", "10GB", 28.0, "rented:UserA")
    );
    ui->edit_file_path->setReadOnly(true);
    ui->edit_file_path->setPlaceholderText("尚未選擇檔案");

    ui->progress_upload->setRange(0, 100);
    ui->progress_upload->setValue(0);

    ui->lbl_eta->setText("剩餘時間:-");
    ui->stackedWidget->setCurrentIndex(0);
    
    qRegisterMetaType<GPUInfo>("GPUInfo");

    m_gpuCatalog = buildMockData();
    resetDetailPanel();

    setupConnections();
    populateMarket();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    if (ui->btn_role_renter)
    {
        connect(ui->btn_role_renter, &QPushButton::clicked, this, [this]() {
            if (ui->stackedWidget)
            {
                ui->stackedWidget->setCurrentIndex(1);
            }
            resetDetailPanel();
        });
    }

    if (ui->btn_role_provider)
    {
        connect(ui->btn_role_provider, &QPushButton::clicked, this, [this]() {
            if (ui->stackedWidget)
            {
                ui->stackedWidget->setCurrentIndex(2);
            }
        });
    }

    connect(ui->btn_upload_task, &QPushButton::clicked, this, [this]() {
    QString path = QFileDialog::getOpenFileName(this, "選擇要上傳的檔案");
    if (path.isEmpty()) return;

    QFileInfo info(path);
    ui->edit_file_path->setText(info.absoluteFilePath());
    uploadTotalBytes = info.size();
    uploadSentBytes = 0;

    ui->progress_upload->setValue(0);

    const double speedBytesPerSec = 2.0 * 1024 * 1024;
    int etaSec = (uploadTotalBytes > 0) ? int(uploadTotalBytes / speedBytesPerSec) : 0;
    if (etaSec < 1) etaSec = 1;
    ui->lbl_eta->setText(QString("剩餘時間:約 %1 秒").arg(etaSec));

    if (!uploadTimer) {
        uploadTimer = new QTimer(this);
        connect(uploadTimer, &QTimer::timeout, this, [this]() {
            const qint64 chunk = 200 * 1024; 
            uploadSentBytes += chunk;
            if (uploadSentBytes > uploadTotalBytes) uploadSentBytes = uploadTotalBytes;

            int percent = (uploadTotalBytes > 0)
                ? int((uploadSentBytes * 100) / uploadTotalBytes)
                : 100;

            ui->progress_upload->setValue(percent);
            const double speedBytesPerSec = 2.0 * 1024 * 1024;
            qint64 remain = uploadTotalBytes - uploadSentBytes;
            int remainSec = int(remain / speedBytesPerSec);
            if (remainSec < 0) remainSec = 0;

            ui->lbl_eta->setText(QString("剩餘時間:約 %1 秒").arg(remainSec));

            if (uploadSentBytes >= uploadTotalBytes) {
                uploadTimer->stop();
                ui->lbl_eta->setText("剩餘時間:上傳完成 ✅");
                QMessageBox::information(this, "上傳完成",
                    "檔案已成功上傳(Demo)。\n下一步可改成送到後端並取得 task_id / 執行狀態。");
            }
        });
    }

    uploadTimer->start(100); 
    });

    if (ui->btn_back_renter)
    {
        connect(ui->btn_back_renter, &QPushButton::clicked, this, [this]() {
            ui->stackedWidget->setCurrentIndex(0);
            resetDetailPanel();
        });
    }

    if (ui->btn_back_provider)
    {
        connect(ui->btn_back_provider, &QPushButton::clicked, this, [this]() {
            ui->stackedWidget->setCurrentIndex(0);
            resetDetailPanel();
        });
    }
}

void MainWindow::populateMarket()
{
    auto *renterLayout = ensureScrollAreaLayout(ui->scrollArea_market);
    auto *providerLayout = ensureScrollAreaLayout(ui->sa_provider_market);

    auto rebuild = [this](QVBoxLayout *layout, bool renterList) {
        if (!layout)
            return;

        QLayoutItem *item = nullptr;
        while ((item = layout->takeAt(0)) != nullptr)
        {
            if (auto *widget = item->widget())
            {
                widget->deleteLater();
            }
            delete item;
        }

        for (const GPUInfo &info : std::as_const(m_gpuCatalog))
        {
            QWidget *card = createMarketCard(info, renterList);
            layout->addWidget(card);
        }

        layout->addStretch(1);
    };

    rebuild(renterLayout, true);
    rebuild(providerLayout, false);
}

QVector<GPUInfo> MainWindow::buildMockData() const
{
    return {
        {QStringLiteral("NVIDIA RTX 4090"), QStringLiteral("24 GB GDDR6X"), QStringLiteral("$6.50/hr"), QStringLiteral("Flagship Ada GPU for extreme workloads.")},
        {QStringLiteral("NVIDIA RTX 4080"), QStringLiteral("16 GB GDDR6X"), QStringLiteral("$5.20/hr"), QStringLiteral("Balanced choice for render and AI tasks.")},
        {QStringLiteral("NVIDIA A100 80GB"), QStringLiteral("80 GB HBM2e"), QStringLiteral("$12.00/hr"), QStringLiteral("Datacenter-grade acceleration for large models." )},
        {QStringLiteral("NVIDIA H100 80GB"), QStringLiteral("80 GB HBM3"), QStringLiteral("$14.50/hr"), QStringLiteral("Cutting-edge Hopper architecture for premium clients.")},
        {QStringLiteral("NVIDIA L40S"), QStringLiteral("48 GB GDDR6"), QStringLiteral("$8.30/hr"), QStringLiteral("High-performance vision and inference GPU.")}
    };
}

QVBoxLayout *MainWindow::ensureScrollAreaLayout(QScrollArea *area)
{
    if (!area)
        return nullptr;

    QWidget *container = area->widget();
    if (!container)
    {
        container = new QWidget(area);
        area->setWidget(container);
    }

    auto *layout = qobject_cast<QVBoxLayout *>(container->layout());
    if (!layout)
    {
        layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);
    }

    return layout;
}

QWidget *MainWindow::createMarketCard(const GPUInfo &info, bool renterList)
{
    auto *button = new QPushButton;
    button->setObjectName(QStringLiteral("gpuCardButton"));
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setMinimumHeight(96);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setStyleSheet(QStringLiteral(
        "QPushButton#gpuCardButton {"
        "text-align: left;"
        "padding: 16px;"
        "border: 1px solid #444444;"
        "border-radius: 12px;"
        "background-color: #2f2f2f;"
        "color: #ffffff;"
        "font-size: 15px;"
        "}"
        "QPushButton#gpuCardButton:hover {"
        "background-color: #3a3a3a;"
        "border-color: #00aaff;"
        "}"
    ));

    const QString cardText = tr("%1\n%2 | %3\n%4")
                                 .arg(info.model, info.vram, info.price, info.description);
    button->setText(cardText);

    if (renterList)
    {
        connect(button, &QPushButton::clicked, this, [this, info]() { handleRenterCardClicked(info); });
    }
    else
    {
       connect(button, &QPushButton::clicked, this, [this, info]() { handleProviderMarketCardClicked(info); });
    }

    return button;
}

void MainWindow::resetDetailPanel()
{
    if (!ui)
        return;

    if (ui->lbl_detail_name)
    {
        ui->lbl_detail_name->setText(tr("Select a GPU"));
    }

    if (ui->lbl_detail_price)
    {
        ui->lbl_detail_price->setText(tr("Choose a GPU from the list to view pricing."));
    }

    if (ui->btn_upload_task)
    {
        ui->btn_upload_task->setEnabled(false);
    }

    m_hasActiveSelection = false;
}

void MainWindow::handleRenterCardClicked(const GPUInfo &info)
{
    if (ui->lbl_detail_name)
    {
        ui->lbl_detail_name->setText(info.model);
    }

    if (ui->lbl_detail_price)
    {
        ui->lbl_detail_price->setText(tr("%1\n%2 | %3")
                                           .arg(info.description, info.vram, info.price));
    }

    if (ui->btn_upload_task)
    {
        ui->btn_upload_task->setEnabled(true);
    }

    m_hasActiveSelection = true;
}

void MainWindow::handleProviderCardClicked()
{
    auto *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    QString hostId = btn->property("hostId").toString();
    QString gpu    = btn->property("gpu").toString();
    QString vram   = btn->property("vram").toString();
    double price   = btn->property("price").toDouble();
    QString status = btn->property("status").toString();

    

    qDebug() << "[Provider Card Clicked]"
             << hostId << gpu << vram << price << status;

}
QPushButton* MainWindow::createProviderCard(const QString& hostId,
                                            const QString& gpu,
                                            const QString& vram,
                                            double price,
                                            const QString& status)
{
    auto *btn = new QPushButton(this);

    btn->setText(QString("%1\n%2 | %3\n$ %4/hr\n%5")
        .arg(hostId)
        .arg(gpu)
        .arg(vram)
        .arg(price, 0, 'f', 2)
        .arg(status));

    btn->setProperty("hostId", hostId);
    btn->setProperty("gpu", gpu);
    btn->setProperty("vram", vram);
    btn->setProperty("price", price);
    btn->setProperty("status", status);

    btn->setMinimumHeight(95);
    btn->setStyleSheet(
        "QPushButton{ text-align:left; padding:10px; border:1px solid #ccc; border-radius:12px; }"
        "QPushButton:hover{ border:1px solid #666; }"
    );

    connect(btn, &QPushButton::clicked, this, [this, hostId, gpu, vram, price, status]() {
        GPUInfo info;
        info.model = hostId;
        info.vram = vram;
        info.price = QString::number(price);
        info.description = QString("%1 | %2").arg(gpu, status);
        handleProviderMarketCardClicked(info);
    });
    return btn;
}

void MainWindow::handleProviderMarketCardClicked(const GPUInfo &info)
{
    qDebug() << "[Provider Market Card]" << info.model << info.vram << info.price;
    
}