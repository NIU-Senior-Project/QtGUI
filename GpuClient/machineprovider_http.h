#pragma once
#include "machineprovider.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

class HttpMachineProvider : public MachineProvider {
    Q_OBJECT
public:
    explicit HttpMachineProvider(const QUrl& baseUrl, QObject* parent = nullptr);

public slots:
    void fetchMachines() override;

private:
    QNetworkAccessManager* m_nam = nullptr;
    QUrl m_baseUrl;
};