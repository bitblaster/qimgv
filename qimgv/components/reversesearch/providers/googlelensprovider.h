#pragma once

#include "components/reversesearch/reversesearchprovider.h"

class GoogleLensProvider : public ReverseSearchProvider {
public:
    QString id() const override { return QStringLiteral("google"); }
    QString displayName() const override { return QStringLiteral("Google Lens"); }
    QString resultHostSuffix() const override { return QStringLiteral("google.com"); }
    SearchStep buildRequest(const QByteArray &jpeg) const override;
    SearchResult handleReply(QNetworkReply *reply, const QByteArray &body) const override;
};
