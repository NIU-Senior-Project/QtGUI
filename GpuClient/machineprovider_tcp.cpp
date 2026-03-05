#include "machineprovider_tcp.h"

TcpMachineProvider::TcpMachineProvider(const QString& host, quint16 port, QObject* parent)
    : MachineProvider(parent), m_host(host), m_port(port) {
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &TcpMachineProvider::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpMachineProvider::onReadyRead);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_socket, &QTcpSocket::errorOccurred, this, &TcpMachineProvider::onSocketError);
#else
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onSocketError(QAbstractSocket::SocketError)));
#endif
}

void TcpMachineProvider::resetState() {
    m_buffer.clear();
    m_pending.clear();
}

void TcpMachineProvider::fetchMachines() {
    resetState();

    // 若已連線就直接送指令；否則先連線
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write("GET_MACHINES\n");
        return;
    }

    if (m_socket->state() == QAbstractSocket::ConnectingState) {
        // 正在連線中就先不做事，等 connected 再送
        return;
    }

    m_socket->connectToHost(m_host, m_port);
}

void TcpMachineProvider::onConnected() {
    m_socket->write("GET_MACHINES\n");
}

void TcpMachineProvider::onReadyRead() {
    m_buffer += m_socket->readAll();

    // 逐行處理（FakeServer 目前是 text line 協定）
    while (true) {
        int idx = m_buffer.indexOf('\n');
        if (idx < 0) break;

        QByteArray lineBytes = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);

        QString line = QString::fromUtf8(lineBytes).trimmed();
        if (line.isEmpty()) continue;

        if (line == "END") {
            emit machinesReady(m_pending);
            return;
        }

        // 你們 FakeServer 格式：name,gpu,vram (可能更多欄位就忽略後面)
        // e.g. "B,RTX3090,24GB"
        const QStringList parts = line.split(',');
        if (parts.size() >= 3) {
            Machine m;
            m.name = parts[0].trimmed();
            m.gpuModel = parts[1].trimmed();
            m.vram = parts[2].trimmed();
            m.status = (parts.size() >= 4) ? parts[3].trimmed() : "";
            m_pending.push_back(m);
        }
    }
}

void TcpMachineProvider::onSocketError(QAbstractSocket::SocketError) {
    emit errorOccurred(m_socket->errorString());
}