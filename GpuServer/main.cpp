#include <QCoreApplication>
#include <QLocale>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDebug>
#include <QHostAddress>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QTcpServer server;

    //當有Client 連進來
    QObject::connect(&server, &QTcpServer::newConnection, [&]() {
        QTcpSocket *client = server.nextPendingConnection();
        qDebug() << "Client connected from"
                 << client->peerAddress().toString()
                 << client->peerPort();

        //之後可以在這裡處理 client 傳來的資料（例如租借請求）
        QObject::connect(client, &QTcpSocket::readyRead, [client]() {
            QByteArray data = client->readAll();
            qDebug() << "Received from client:" << data;
        });

        QObject::connect(client, &QTcpSocket::disconnected, [client]() {
            qDebug() << "Client disconnected";
            client->deleteLater();
        });

        //目前先做：一連進來就送主機清單
        QByteArray msg;
        msg += "B,RTX 4070,12GB,idle\n";
        msg += "C,RTX 3080,10GB,rented:A\n";
        msg += "END\n";

        client->write(msg);
        client->flush();
    });

    //監聽所有ip的5000port
    if (!server.listen(QHostAddress::Any, 5000)) {
        qCritical() << "Cannot start server:" << server.errorString();
        return -1;
    }
    qDebug() << "Server listening on port" << server.serverPort();
    return a.exec();
}
