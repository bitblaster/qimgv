#include "googlelensprovider.h"

#include <QDateTime>
#include <QUrlQuery>

static const char *uploadUrl = "https://lens.google.com/v3/upload";

// Lens serves a different path to something that doesn't look like a browser.
static const char *userAgent = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                               "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36";

/* Without a consent cookie the upload is bounced to consent.google.com and we never get the
 * results url. This is the cookie a browser is left with after accepting, and it is all the
 * endpoint checks for. If Lens ever starts answering with a consent.google.com redirect
 * again, this value is the first thing to refresh.
 */
static const char *consentCookie = "SOCS=CAISNQgQEitib3FfaWRlbnRpdHlmcm9udGVuZHVpc2VydmVyXzIwMjQwMzEwLjA2X3AwGgJlbiADGgYIgLC_rwY";

SearchStep GoogleLensProvider::buildRequest(const QByteArray &jpeg) const {
    SearchStep step;
    QUrl url(uploadUrl);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("stcs"), QString::number(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);
    step.request.setUrl(url);
    step.request.setHeader(QNetworkRequest::UserAgentHeader, QVariant(userAgent));
    step.request.setRawHeader("Cookie", consentCookie);
    step.multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    step.multiPart->append(imageFilePart(QStringLiteral("encoded_image"), jpeg));
    return step;
}

SearchResult GoogleLensProvider::handleReply(QNetworkReply *reply, const QByteArray &body) const {
    Q_UNUSED(body)
    SearchResult result;
    QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if(redirect.isEmpty()) {
        result.kind = SearchResult::Failed;
        result.error = QObject::tr("Google Lens did not return a results page");
        return result;
    }
    result.kind = SearchResult::Done;
    result.url = reply->url().resolved(redirect);
    return result;
}
