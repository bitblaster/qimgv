#include "localuploadserver.h"

#include <QRandomGenerator>

// long enough for a browser to start up cold, short enough that the image doesn't sit there
static const int lifetimeMs = 120000;

LocalUploadServer::LocalUploadServer(QObject *parent) : QObject(parent) {
    connect(&server, &QTcpServer::newConnection, this, &LocalUploadServer::onNewConnection);
    lifetime.setSingleShot(true);
    connect(&lifetime, &QTimer::timeout, this, &LocalUploadServer::stop);
}

LocalUploadServer::~LocalUploadServer() {
    stop();
}

QUrl LocalUploadServer::serve(QString outer, QString inner) {
    stop();
    if(!server.listen(QHostAddress::LocalHost, 0))
        return QUrl();
    // guessing this is the only thing standing between another local process and the image
    token = QString::number(QRandomGenerator::system()->generate64(), 16).rightJustified(16, QLatin1Char('0'));
    outerHtml = std::move(outer);
    innerHtml = std::move(inner);
    lifetime.start(lifetimeMs);
    return QUrl(QStringLiteral("http://127.0.0.1:%1/%2/").arg(server.serverPort()).arg(token));
}

void LocalUploadServer::stop() {
    lifetime.stop();
    server.close();
    for(auto socket : pending.keys())
        socket->deleteLater();
    pending.clear();
    outerHtml.clear();
    innerHtml.clear();
    token.clear();
}

void LocalUploadServer::onNewConnection() {
    while(QTcpSocket *socket = server.nextPendingConnection()) {
        pending.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            pending.remove(socket);
            socket->deleteLater();
        });
    }
}

void LocalUploadServer::onReadyRead(QTcpSocket *socket) {
    auto request = pending.find(socket);
    if(request == pending.end())
        return;
    request->append(socket->readAll());
    // a browser sends the whole thing at once, but nothing says it has to
    if(!request->contains("\r\n\r\n"))
        return;
    QByteArray requestLine = request->left(request->indexOf("\r\n"));
    QList<QByteArray> parts = requestLine.split(' ');
    QByteArray method = parts.value(0);
    QByteArray path = parts.value(1);
    QByteArray outerPath = QStringLiteral("/%1/").arg(token).toUtf8();
    QByteArray innerPath = outerPath + "inner";
    if(method != "GET") {
        respond(socket, "405 Method Not Allowed", QByteArray());
    } else if(path == outerPath) {
        respond(socket, "200 OK", outerHtml.toUtf8());
    } else if(path == innerPath) {
        respond(socket, "200 OK", innerHtml.toUtf8());
        /* The frame is the last thing we owe the browser - from here on the upload is between
         * it and the engine. Closing now keeps the image out of reach for the rest of the
         * session; sockets already open are left to finish writing.
         */
        server.close();
        lifetime.stop();
        outerHtml.clear();
        innerHtml.clear();
    } else {
        respond(socket, "404 Not Found", QByteArray());
    }
}

void LocalUploadServer::respond(QTcpSocket *socket, QByteArray status, QByteArray body) {
    QByteArray response = "HTTP/1.1 " + status + "\r\n"
                          "Content-Type: text/html; charset=utf-8\r\n"
                          "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                          "Cache-Control: no-store\r\n"
                          "Connection: close\r\n"
                          "\r\n";
    socket->write(response + body);
    socket->disconnectFromHost();
}
