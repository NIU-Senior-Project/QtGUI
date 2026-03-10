#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QUuid>
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
#include <QRegularExpression>

static void clearLayout(QLayout *layout)
{
    if (!layout) return;
    while (QLayoutItem *it = layout->takeAt(0)) {
        if (auto *w = it->widget()) w->deleteLater();
        delete it;
    }
}
static QString extractJobIdFromText(const QString& s) {
            QRegularExpression re1("\"job_id\"\\s*:\\s*\"([^\"]+)\"");
            auto m1 = re1.match(s);
            if (m1.hasMatch()) return m1.captured(1);

            QRegularExpression re2("job_id\\s*=\\s*([A-Za-z0-9\\-]+)");
            auto m2 = re2.match(s);
            if (m2.hasMatch()) return m2.captured(1);

            return {};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->edit_r_container->setText("nvidia/cuda:13.1.1-cudnn-devel-ubuntu24.04");
    ui->edit_r_script->setPlainText("nvidia-smi");
    ui->edit_p_manager->setText("100.99.159.99:8080");
    ui->sa_renter_nodes->setWidgetResizable(true);
    ui->sa_provider_market->setWidgetResizable(true);

    QFont font;
    font.setPointSize(10);
    this->setFont(font);
    nam = new QNetworkAccessManager(this);
    pollTimer = new QTimer(this);
    pollTimer->setInterval(2000);
    connect(pollTimer, &QTimer::timeout, this, &MainWindow::pollJobStatus);
 
    //ui->vlay_provider_cards->insertWidget(0, createProviderCard("B", "RTX 4070", "12GB", 35.0, "idle")); 
    //ui->vlay_provider_cards->insertWidget(0, createProviderCard("C", "RTX 3080", "10GB", 28.0, "idle"));
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
    if (ui->lbl_r_manager) {
    ui->lbl_r_manager->setText("Manager IP: "+managerIpPort);
    }

    if (managerIpPort.isEmpty()) {
        QMessageBox::warning(this, "缺少 manager", "請輸入 210 的 tailscaleIP:8080");
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

        const auto arr = doc.array();
        qDebug() << "nodes count =" << arr.size();

        for (auto v : arr) {
            QString ip = v.toString();

            if (!m_nodeStatusMap.contains(ip)) {
                m_nodeStatusMap[ip] = "idle";
            }
            QString status = m_nodeStatusMap.value(ip, "idle");
            auto *btn = new QPushButton(QString("Node: %1\nStatus: %2").arg(ip).arg(status),
                                        ui->sa_renter_nodes_content
            );
            btn->setMinimumHeight(50);

            connect(btn, &QPushButton::clicked, this, [this, ip]() {
                selectedNodeIp = ip;
                ui->lbl_r_selected_node->setText("已選擇節點：" + ip);
            });

            ui->vlay_renter_nodes->addWidget(btn);
        }
        ui->vlay_renter_nodes->addStretch(1);
        ui->sa_renter_nodes_content->adjustSize();
        ui->sa_renter_nodes->viewport()->update();
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
        QMessageBox::warning(this, "缺少 manager", "請輸入 node-manager 位址，例如 100.XX.XX.XX:8080");
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
        QByteArray body = reply->readAll();
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        auto netErr = reply->error();
        QString netErrStr = reply->errorString();
        reply->deleteLater();

        if (netErr != QNetworkReply::NoError) {
            QMessageBox::warning(this, "連線失敗", netErrStr);
            return;
        }
        QString text = QString::fromUtf8(body);

        ui->text_r_stdout->setPlainText("submit_job HTTP " + QString::number(code) + "\n" + text);
        ui->text_r_stderr->clear();

        if (code != 200) {
            ui->text_r_stderr->setPlainText("submit_job failed.\nHTTP " + QString::number(code) + "\n" + text);
            return;
        }

        currentJobId = extractJobIdFromText(text);
        if (currentJobId.isEmpty()) {
            ui->text_r_stderr->setPlainText("submit_job 成功但抓不到 job_id。\n請看 stdout 回傳內容是否有 job_id。");
            return;
        } 
        m_nodeStatusMap[selectedNodeIp] = "running";
        refreshNodes();
        refreshProviderNodes();
        m_warnedThisJob = false;
        m_warnReasonThisJob.clear();
        ui->text_r_stdout->appendPlainText("\n解析到 job_id=" + currentJobId + "\n開始輪詢 job_status...");
        if (!pollTimer->isActive()) pollTimer->start();
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
        const auto netErr = reply->error();
        const QString netErrStr = reply->errorString();
        const QByteArray body = reply->readAll();
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        auto warnOnceAndStop = [&](const QString& reason) {
            if (m_warnedThisJob) return;         
            m_warnedThisJob = true;
            m_warnReasonThisJob = reason;
            pollTimer->stop();                   

            if (ui->text_r_stderr) ui->text_r_stderr->setPlainText(reason);

            QMessageBox msg;
            msg.setIcon(QMessageBox::Warning);
            msg.setWindowTitle("連線失敗");
            msg.setText("目前無法取得任務狀態（只會提醒一次）。\n"
                        "請確認 manager / node 連線後，再按一次「送出任務」。");
            msg.setInformativeText(reason);
            msg.setStandardButtons(QMessageBox::Ok);  
            msg.exec();
        };
        if (netErr != QNetworkReply::NoError) {
            warnOnceAndStop(QString("[poll] network error: %1").arg(netErrStr));
            m_nodeStatusMap[selectedNodeIp] = "offline";
            refreshNodes();
            refreshProviderNodes();
            return;
        }
        if (code >= 500) {
            warnOnceAndStop(QString("[poll] server error HTTP %1\n%2")
                            .arg(code)
                            .arg(QString::fromUtf8(body)));
            m_nodeStatusMap[selectedNodeIp] = "offline";
            refreshNodes();
            refreshProviderNodes();                
            return;
        }
        if (code == 202) {
            if (ui->text_r_stdout) ui->text_r_stdout->appendPlainText(".");
            return;
        }

        if (code == 200) {
            pollTimer->stop();
            if (ui->text_r_stdout) ui->text_r_stdout->appendPlainText("\n=== JOB FINISHED ===\n" + QString::fromUtf8(body));
            if (ui->text_r_stderr) ui->text_r_stderr->clear();
             
            m_nodeStatusMap[selectedNodeIp] = "idle";
            refreshNodes();
            refreshProviderNodes();

            return;
        }
        warnOnceAndStop(QString("[poll] unexpected HTTP %1\n%2")
                        .arg(code)
                        .arg(QString::fromUtf8(body)));
    });
}

// -------------------- HTTP: POST /register --------------------
void MainWindow::registerNode()
{
    managerIpPort = ui->edit_p_manager->text().trimmed(); 
    QString nodeIp = ui->edit_p_node_ip->text().trimmed();
    QString gpuModel = ui->edit_p_gpu_model->text().trimmed();
    QString vram     = ui->edit_p_vram->text().trimmed();

    if (managerIpPort.isEmpty() || nodeIp.isEmpty()) {
        QMessageBox::warning(this, "資料不足", "請確認 manager 與本機 IP 都有值。");
        return;
    }

    if (gpuModel.isEmpty() || gpuModel == "-") {
        gpuModel = "";  
    }
    if (vram.isEmpty()) vram = "-";
    QJsonObject payload{{"ip", nodeIp}, {"gpu_model", gpuModel}};

    QNetworkRequest req(QUrl(QString("http://%1/register").arg(managerIpPort)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto *reply = nam->post(req, QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, nodeIp, gpuModel, vram]() {
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
            if (ui->lbl_p_status_value) 
                ui->lbl_p_status_value->setText("idle");
            refreshProviderNodes();    
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
    if (ui->edit_p_gpu_model)   ui->edit_p_gpu_model->setText(gpu);
    if (ui->edit_p_vram)        ui->edit_p_vram->setText(vram);
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
//刪除節點
void MainWindow::deleteNode()
{
    managerIpPort = ui->edit_p_manager->text().trimmed();
    QString nodeIp = ui->lbl_p_host_value->text().trimmed();

    if (managerIpPort.isEmpty() || nodeIp.isEmpty()) {
        QMessageBox::warning(this, "資料不足", "請確認 manager IP 與 node IP");
        return;
    }

    if (QMessageBox::question(this,"確認刪除","確定要刪除這個節點嗎？")
        != QMessageBox::Yes) {
        return;
    }

    QUrl url(QString("http://%1/deregister").arg(managerIpPort));

    QJsonObject payload{
        {"ip", nodeIp}
    };

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto *reply = nam->post(req, QJsonDocument(payload).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, nodeIp]() {

        QByteArray body = reply->readAll();
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        reply->deleteLater();

        if (code == 200) {
            ui->text_p_log->appendPlainText("[INFO] 已刪除節點: " + nodeIp);
            QMessageBox::information(this,"成功","節點已刪除");
            refreshProviderNodes();
            clearProviderInfo();
        } else {
            QMessageBox::warning(this,"刪除失敗",QString::fromUtf8(body));
        }
    });
}
//在刪除節點後，清空右側主機資訊
void MainWindow::clearProviderInfo()
{
    ui->lbl_p_host_value->setText("-");
    ui->edit_p_gpu_model->setText("-");
    ui->edit_p_vram->setText("-");
    ui->edit_p_price->setText("0.00");
    ui->lbl_p_status_value->setText("-");
}

QPushButton* MainWindow::createSimpleProviderNodeCard(const QString& ip)
{
    auto *btn = new QPushButton(this);

    QString status = m_nodeStatusMap.value(ip, "idle");

    btn->setText(QString("Node: %1\nGPU: -\nVRAM: -\nStatus: %2")
                 .arg(ip)
                 .arg(status));

    btn->setProperty("hostId", ip);
    btn->setProperty("gpu", "-");
    btn->setProperty("vram", "-");
    btn->setProperty("price", 0.0);
    btn->setProperty("status", status);
    btn->setProperty("owned", true);

    btn->setMinimumHeight(95);
    btn->setStyleSheet(
        "QPushButton{ text-align:left; padding:10px; border:1px solid #ccc; border-radius:12px; }"
        "QPushButton:hover{ border:1px solid #666; }"
    );

    connect(btn, &QPushButton::clicked, this, &MainWindow::handleProviderCardClicked);
    return btn;
}
// 讓出租者頁面的左側市場列表跟租借者頁面的一樣
void MainWindow::refreshProviderNodes()
{
    managerIpPort = ui->edit_p_manager->text().trimmed();
    if (managerIpPort.isEmpty()) {
        return;
    }

    QUrl url(QString("http://%1/nodes").arg(managerIpPort));
    QNetworkRequest req(url);
    auto *reply = nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            ui->text_p_log->appendPlainText("[ERROR] refreshProviderNodes: " + reply->errorString());
            reply->deleteLater();
            return;
        }

        QByteArray body = reply->readAll();
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        if (code != 200) {
            ui->text_p_log->appendPlainText(
                QString("[ERROR] refreshProviderNodes HTTP %1\n%2")
                    .arg(code)
                    .arg(QString::fromUtf8(body))
            );
            return;
        }

        QJsonParseError err{};
        QJsonDocument doc = QJsonDocument::fromJson(body, &err);
        if (err.error != QJsonParseError::NoError || !doc.isArray()) {
            ui->text_p_log->appendPlainText("[ERROR] provider nodes 格式錯誤");
            return;
        }

        clearLayout(ui->vlay_provider_cards);

        const auto arr = doc.array();
        for (auto v : arr) {
            QString ip = v.toString();
            auto *btn = createSimpleProviderNodeCard(ip);
            ui->vlay_provider_cards->addWidget(btn);
        }

        ui->vlay_provider_cards->addStretch(1);

        ui->text_p_log->appendPlainText(QString("[INFO] 已同步 %1 個節點").arg(arr.size()));
    });
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
        refreshProviderNodes();
    });

    connect(ui->btn_back_renter, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(0);
    });
    connect(ui->btn_back_provider, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(0);
    });
    connect(ui->btn_p_refresh, &QPushButton::clicked, this, [this]() {
        refreshProviderNodes();
        refreshNodes();
    });
    connect(ui->btn_r_refresh_nodes, &QPushButton::clicked, this, &MainWindow::refreshNodes);
    connect(ui->btn_r_submit_job, &QPushButton::clicked, this, &MainWindow::submitJob);
    connect(ui->btn_p_register, &QPushButton::clicked, this, &MainWindow::registerNode);
    connect(ui->btn_p_apply_price, &QPushButton::clicked, this, &MainWindow::applyPrice);
    connect(ui->btn_p_delete_node, &QPushButton::clicked, this, &MainWindow::deleteNode);
}
