#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDebug>
#include <QMessageBox>
#include <QPushButton>
#include <QDoubleValidator>
#include <QLayoutItem>

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>

static void clearLayout(QLayout *layout)
{
    if (!layout) return;
    while (QLayoutItem *it = layout->takeAt(0)) {
        if (auto *w = it->widget()) w->deleteLater();
        delete it;
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->edit_p_manager->setText("127.0.0.1:8080");

    nam = new QNetworkAccessManager(this);
    pollTimer = new QTimer(this);
    pollTimer->setInterval(2000);
    connect(pollTimer, &QTimer::timeout, this, &MainWindow::pollJobStatus);

    ui->vlay_provider_cards->insertWidget(0, createProviderCard("B", "RTX 4070", "12GB", 35.0, "idle")); 
    ui->vlay_provider_cards->insertWidget(0, createProviderCard("C", "RTX 3080", "10GB", 28.0, "idle"));
    auto *first = qobject_cast<QPushButton*>(ui->vlay_provider_cards->itemAt(0)->widget());
    if (first) first->click();

    if (ui->edit_p_price) {
        ui->edit_p_price->setValidator(new QDoubleValidator(0, 999999, 2, ui->edit_p_price));
        ui->edit_p_price->setPlaceholderText("例如 35.00");
    }

    ui->stackedWidget->setCurrentIndex(0);
    setupConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// -------------------- HTTP: GET /nodes --------------------
void MainWindow::refreshNodes()
{
    managerIpPort = ui->edit_p_manager->text().trimmed();

    if (managerIpPort.isEmpty()) {
        QMessageBox::warning(this, "缺少 manager", "請輸入 node-manager 位址，例如 127.0.0.1:8080");
        return;
    }

    QUrl url(QString("http://%1/nodes").arg(managerIpPort));
    QNetworkRequest req(url);
    auto *reply = nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, "連線失敗", reply->errorString());
        reply->deleteLater();
        return;
        } 
        QByteArray body = reply->readAll();
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        qDebug() << "HTTP /nodes status =" << code;
        qDebug() << "HTTP /nodes body =" << body;  

        if (code != 200) {
            QMessageBox::warning(this, "nodes 失敗",
                                 QString("HTTP %1\n%2").arg(code).arg(QString::fromUtf8(body)));
            return;
        }

        QJsonParseError err{};
        QJsonDocument doc = QJsonDocument::fromJson(body, &err);
        if (err.error != QJsonParseError::NoError || !doc.isArray()) {
            QMessageBox::warning(this, "nodes 格式錯誤", QString::fromUtf8(body));
            return;
        }

        clearLayout(ui->vlay_renter_nodes);

        for (auto v : doc.array()) {
            QString ip = v.toString();
            auto *btn = new QPushButton(ip, this);
            btn->setMinimumHeight(48);

            connect(btn, &QPushButton::clicked, this, [this, ip]() {
                selectedNodeIp = ip;
                ui->lbl_r_selected_node->setText("已選擇節點：" + ip);
            });

            ui->vlay_renter_nodes->addWidget(btn);
        }

        ui->vlay_renter_nodes->addStretch(1);
    });
}

// -------------------- HTTP: POST /submit_job --------------------
void MainWindow::submitJob()
{
    if (selectedNodeIp.isEmpty()) {
        QMessageBox::warning(this, "未選擇節點", "請先從左側選一個節點。");
        return;
    }

    QString container = ui->edit_r_container->text().trimmed();
    QString script = ui->edit_r_script->toPlainText().trimmed();
    if (container.isEmpty() || script.isEmpty()) {
        QMessageBox::warning(this, "資料不足", "container / script 不能為空。");
        return;
    }

    managerIpPort = ui->edit_p_manager->text().trimmed();
    if (managerIpPort.isEmpty()) {
        QMessageBox::warning(this, "缺少 manager", "請輸入 node-manager 位址，例如 127.0.0.1:8080");
        return;
    }

    QUrl url(QString("http://%1/submit_job").arg(managerIpPort));

    QJsonObject payload{
        {"target_node", selectedNodeIp},
        {"container", container},
        {"script", script}
    };

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto *reply = nam->post(req, QJsonDocument(payload).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, "連線失敗", reply->errorString());
        reply->deleteLater();
        return;
        }
        QByteArray body = reply->readAll();
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        if (code != 200) {
            ui->text_r_stderr->setPlainText(QString("HTTP %1\n%2")
                                            .arg(code).arg(QString::fromUtf8(body)));
            return;
        }

        QJsonParseError err{};
        QJsonDocument doc = QJsonDocument::fromJson(body, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            ui->text_r_stderr->setPlainText(QString::fromUtf8(body));
            return;
        }

        currentJobId = doc.object().value("job_id").toString();
        if (currentJobId.isEmpty()) {
            ui->text_r_stderr->setPlainText("沒有拿到 job_id:\n" + QString::fromUtf8(body));
            return;
        }

        ui->text_r_stdout->setPlainText("已送出任務,job_id=" + currentJobId + "\n等待執行結果...");
        ui->text_r_stderr->clear();

        if (!pollTimer->isActive()) {
            pollTimer->start();
        }
    });
}

// -------------------- HTTP: GET /job_status?id=...&node=... --------------------
void MainWindow::pollJobStatus()
{
    if (currentJobId.isEmpty() || selectedNodeIp.isEmpty() || managerIpPort.isEmpty()) return;

    QUrl url(QString("http://%1/job_status").arg(managerIpPort));
    QUrlQuery q;
    q.addQueryItem("id", currentJobId);
    q.addQueryItem("node", selectedNodeIp);
    url.setQuery(q);

    QNetworkRequest req(url);
    auto *reply = nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, "連線失敗", reply->errorString());
        reply->deleteLater();
        return;
       }
        QByteArray body = reply->readAll();
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        if (code == 202) {
            ui->text_r_stdout->appendPlainText(".");
            return;
        }

        if (code == 200) {
            pollTimer->stop();
            ui->text_r_stdout->appendPlainText("\n=== JOB FINISHED ===\n" + QString::fromUtf8(body));
            return;
        }

        pollTimer->stop();
        ui->text_r_stderr->setPlainText(QString("job_status HTTP %1\n%2")
                                        .arg(code).arg(QString::fromUtf8(body)));
    });
}

// -------------------- HTTP: POST /register --------------------
void MainWindow::registerNode()
{
    managerIpPort = ui->edit_p_manager->text().trimmed(); 
    QString nodeIp = ui->edit_p_node_ip->text().trimmed();
    QString gpuModel = ui->lbl_p_gpu_value->text().trimmed();

    if (managerIpPort.isEmpty() || nodeIp.isEmpty() || gpuModel.isEmpty() || gpuModel == "-") {
        QMessageBox::warning(this, "資料不足", "請確認 manager、本機 IP、GPU 型號都有值（先點左側主機卡片）。");
        return;
    }

    QJsonObject payload{{"ip", nodeIp}, {"gpu_model", gpuModel}};

    QNetworkRequest req(QUrl(QString("http://%1/register").arg(managerIpPort)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto *reply = nam->post(req, QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, nodeIp, gpuModel]() {
        if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, "連線失敗", reply->errorString());
        reply->deleteLater();
        return;
        }
        QByteArray body = reply->readAll();
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        if (code == 200) {
            ui->text_p_log->appendPlainText("[INFO] Registered node: " + nodeIp);

            double price = ui->edit_p_price ? ui->edit_p_price->text().toDouble() : 0.0;
            addMyHostToTop(nodeIp, gpuModel, "-", price, "idle");
        } else {
            ui->text_p_log->appendPlainText(QString("[ERROR] register failed HTTP %1: ").arg(code)
                                            + QString::fromUtf8(body));
        }
    });
}

// -------------------- Provider: host cards --------------------
void MainWindow::handleProviderCardClicked()
{
    auto *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    currentProviderButton = btn; 
     
    QString hostId = btn->property("hostId").toString();
    QString gpu    = btn->property("gpu").toString();
    QString vram   = btn->property("vram").toString();
    double price   = btn->property("price").toDouble();
    QString status = btn->property("status").toString();
    bool owned     = btn->property("owned").toBool();

    if (ui->edit_p_price) {
        ui->edit_p_price->setReadOnly(!owned);
        ui->edit_p_price->setEnabled(owned);
        ui->edit_p_price->setToolTip(owned ? "" : "只有自己上傳的主機才能修改價格");
        ui->edit_p_price->setText(QString::number(price, 'f', 2));
    }

    if (ui->lbl_p_host_value)   ui->lbl_p_host_value->setText(hostId);
    if (ui->lbl_p_gpu_value)    ui->lbl_p_gpu_value->setText(gpu);
    if (ui->lbl_p_vram_value)   ui->lbl_p_vram_value->setText(vram);
    if (ui->lbl_p_status_value) ui->lbl_p_status_value->setText(status);
}

QPushButton* MainWindow::createProviderCard(const QString& hostId,
                                            const QString& gpu,
                                            const QString& vram,
                                            double price,
                                            const QString& status)
{
    auto *btn = new QPushButton(this);
    btn->setText(QString("%1\n%2 | %3\n$ %4/hr\n%5")
                 .arg(hostId).arg(gpu).arg(vram).arg(price, 0, 'f', 2).arg(status));

    btn->setProperty("hostId", hostId);
    btn->setProperty("gpu", gpu);
    btn->setProperty("vram", vram);
    btn->setProperty("price", price);
    btn->setProperty("status", status);
    btn->setProperty("owned", true);

    btn->setMinimumHeight(90);
    btn->setStyleSheet(
        "QPushButton{ text-align:left; padding:10px; border:1px solid #ccc; border-radius:12px; }"
        "QPushButton:hover{ border:1px solid #666; }"
    );
    
    connect(btn, &QPushButton::clicked, this, &MainWindow::handleProviderCardClicked);
    return btn;
}
//-----按下按鈕才會套用新價格----
void MainWindow::applyPrice()
{
    if (!currentProviderButton) {
        QMessageBox::warning(this, "未選擇主機", "請先點左側主機卡片，再修改價格。");
        return;
    }
    if (!currentProviderButton->property("owned").toBool()) {
        QMessageBox::warning(this, "不可修改", "只有自己上傳的主機才能修改價格。");
        return;
    }
    bool ok = false;
    double newPrice = ui->edit_p_price->text().toDouble(&ok);
    if (!ok || newPrice < 0) {
        QMessageBox::warning(this, "價格格式錯誤", "請輸入有效的數字價格。");
        return;
    }
    currentProviderButton->setProperty("price", newPrice);
    QString hostId = currentProviderButton->property("hostId").toString();
    QString gpu    = currentProviderButton->property("gpu").toString();
    QString vram   = currentProviderButton->property("vram").toString();
    QString status = currentProviderButton->property("status").toString();

    currentProviderButton->setText(QString("%1\n%2 | %3\n$ %4/hr\n%5")
        .arg(hostId)
        .arg(gpu)
        .arg(vram)
        .arg(newPrice, 0, 'f', 2)
        .arg(status));
        
    if (ui->text_p_log) {
        ui->text_p_log->appendPlainText(QString("[INFO] Updated price: %1 -> %2")
            .arg(hostId)
            .arg(newPrice, 0, 'f', 2));
    }
}

void MainWindow::addMyHostToTop(const QString& hostId,
                                const QString& gpu,
                                const QString& vram,
                                double price,
                                const QString& status)
{
    if (!ui->vlay_provider_cards) return;

    QPushButton *card = createProviderCard(hostId, gpu, vram, price, status);
    ui->vlay_provider_cards->insertWidget(0, card);
    card->click();
}

// -------------------- UI connections --------------------
void MainWindow::setupConnections()
{
    connect(ui->btn_role_renter, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(1);
        refreshNodes();
    });
    connect(ui->btn_role_provider, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(2);
    });

    connect(ui->btn_back_renter, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(0);
    });
    connect(ui->btn_back_provider, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(0);
    });

    connect(ui->btn_r_refresh_nodes, &QPushButton::clicked, this, &MainWindow::refreshNodes);
    connect(ui->btn_r_submit_job, &QPushButton::clicked, this, &MainWindow::submitJob);
    connect(ui->btn_p_register, &QPushButton::clicked, this, &MainWindow::registerNode);
    connect(ui->btn_p_refresh, &QPushButton::clicked, this, &MainWindow::refreshNodes);
    connect(ui->btn_p_apply_price, &QPushButton::clicked, this, &MainWindow::applyPrice);
}
