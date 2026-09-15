#include "googlelensprovider.h"

#include <QDateTime>
#include <QUrlQuery>

static const char *uploadUrl = "https://lens.google.com/v3/upload";

/* Lens only tells the browser that uploaded an image where to find the results, so this is
 * the one provider that doesn't upload anything itself - it just points the form. Why the
 * upload has to come from the browser, and what the page around the form has to look like
 * for the endpoint to accept it, is written down on BrowserUpload.
 */
BrowserUpload GoogleLensProvider::browserUpload() const {
    BrowserUpload upload;
    QUrl url(QString::fromLatin1(uploadUrl));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("stcs"), QString::number(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);
    upload.action = url;
    upload.fieldName = QStringLiteral("encoded_image");
    return upload;
}
