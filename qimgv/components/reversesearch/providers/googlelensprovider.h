#pragma once

#include "components/reversesearch/reversesearchprovider.h"

class GoogleLensProvider : public ReverseSearchProvider {
public:
    QString id() const override { return QStringLiteral("google"); }
    QString displayName() const override { return QStringLiteral("Google Lens"); }
    QString resultHostSuffix() const override { return QStringLiteral("google.com"); }
    BrowserUpload browserUpload() const override;
};
