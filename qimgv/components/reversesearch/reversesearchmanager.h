#pragma once

#include <QImage>
#include <QNetworkAccessManager>
#include <QObject>
#include <memory>
#include <vector>
#include "components/reversesearch/localuploadserver.h"
#include "components/reversesearch/reversesearchprovider.h"

class ReverseSearchManager : public QObject {
    Q_OBJECT
public:
    explicit ReverseSearchManager(QObject *parent = nullptr);

    void search(QString providerId, std::shared_ptr<const QImage> image);

signals:
    void ready(QUrl resultUrl);
    void failed(QString message);

private:
    static QByteArray encodeJpeg(const QImage &image);
    const ReverseSearchProvider *findProvider(const QString &providerId) const;
    void send(SearchStep step, const ReverseSearchProvider *provider);
    void onReplyFinished(QNetworkReply *reply, const ReverseSearchProvider *provider);
    void searchViaBrowser(const BrowserUpload &upload, const QByteArray &jpeg,
                          const ReverseSearchProvider *provider);

    QNetworkAccessManager net;
    std::vector<std::unique_ptr<ReverseSearchProvider>> providers;
    QNetworkReply *activeReply = nullptr;
    LocalUploadServer uploadServer;
};
