#pragma once

#include <QHash>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

/* A web server that exists for the length of one search.
 *
 * Some engines tie the results to whoever uploaded the image, so the upload has to come from
 * the browser rather than from us (see BrowserUpload). The only way to hand the browser a
 * page of our own making is to serve it, so this listens on a loopback port, answers exactly
 * two urls - the page and the frame inside it - and shuts itself down afterwards.
 *
 * The pages carry the image the user is looking at, so they are not for anyone else on the
 * machine to read: the port is loopback only, the path holds a random token, the server stops
 * once the frame has been fetched, and a timer stops it anyway if the browser never shows up.
 */
class LocalUploadServer : public QObject {
    Q_OBJECT
public:
    explicit LocalUploadServer(QObject *parent = nullptr);
    ~LocalUploadServer() override;

    // Starts serving the two pages; returns the url to open, or an empty url if the port could
    // not be opened. A search still being served is dropped.
    QUrl serve(QString outerHtml, QString innerHtml);
    void stop();

private:
    void onNewConnection();
    void onReadyRead(QTcpSocket *socket);
    void respond(QTcpSocket *socket, QByteArray status, QByteArray body);

    QTcpServer server;
    QHash<QTcpSocket *, QByteArray> pending; // requests still arriving
    QString token;
    QString outerHtml;
    QString innerHtml;
    QTimer lifetime;
};
