#pragma once
#include "machineprovider.h"
#include <QTcpSocket>

class TcpMachineProvider : public MachineProvider {
    Q_OBJECT
public:
    explicit TcpMachineProvider(const QString& host, quint16 port, QObject* parent = nullptr);

public slots:
    void fetchMachines() override;

private slots:
    void onConnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError);

private:
    void resetState();

    QString m_host;
    quint16 m_port;
    QTcpSocket* m_socket = nullptr;

    QByteArray m_buffer;
    QList<Machine> m_pending;
};