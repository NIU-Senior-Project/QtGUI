#include "machineprovider_http.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

HttpMachineProvider::HttpMachineProvider(const QUrl& baseUrl, QObject* parent)
    : MachineProvider(parent), m_baseUrl(baseUrl) {
    m_nam = new QNetworkAccessManager(this);
}

void HttpMachineProvider::fetchMachines() {
    // 假設未來 endpoint 是 GET {baseUrl}/machines
    QUrl url = m_baseUrl;
    url.setPath(url.path().endsWith("/") ? url.path() + "machines" : url.path() + "/machines");

    QNetworkRequest req(url);
    auto* reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            return;
        }

        const QByteArray body = reply->readAll();
        // 先不硬寫 schema，等你拿到真 API sample 再補
        // 目前先回 error，避免你以為接好了但其實亂 parse
        emit errorOccurred(QString("HTTP provider not implemented yet. Response: %1").arg(QString::fromUtf8(body.left(200))));
    });
}