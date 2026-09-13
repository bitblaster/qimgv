#pragma once

#include "components/reversesearch/reversesearchprovider.h"

class TinEyeProvider : public ReverseSearchProvider {
public:
    QString id() const override { return QStringLiteral("tineye"); }
    QString displayName() const override { return QStringLiteral("TinEye"); }
    QString resultHostSuffix() const override { return QStringLiteral("tineye.com"); }
    SearchStep buildRequest(const QByteArray &jpeg) const override;
    SearchResult handleReply(QNetworkReply *reply, const QByteArray &body) const override;
};
