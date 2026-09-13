#pragma once

#include "components/reversesearch/reversesearchprovider.h"

class BingProvider : public ReverseSearchProvider {
public:
    QString id() const override { return QStringLiteral("bing"); }
    QString displayName() const override { return QStringLiteral("Bing Visual Search"); }
    QString resultHostSuffix() const override { return QStringLiteral("bing.com"); }
    SearchStep buildRequest(const QByteArray &jpeg) const override;
    SearchResult handleReply(QNetworkReply *reply, const QByteArray &body) const override;
};
