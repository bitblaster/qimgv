#include "bingprovider.h"

#include <QUrlQuery>

static const char *uploadUrl = "https://www.bing.com/images/search?view=detailv2&iss=sbiupload&FORM=SBIHMP&sbisrc=UrlPaste";
static const char *userAgent = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                               "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36";

// unlike the other engines bing wants the image as a base64 text field, not as a file part
SearchStep BingProvider::buildRequest(const QByteArray &jpeg) const {
    SearchStep step;
    step.request.setUrl(QUrl(uploadUrl));
    step.request.setHeader(QNetworkRequest::UserAgentHeader, QVariant(userAgent));
    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"imageBin\""));
    part.setBody(jpeg.toBase64());
    step.multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    step.multiPart->append(part);
    return step;
}

/* Bing answers with a relative Location carrying an insightsToken plus a handful of upload
 * flow parameters. Following that url as-is lands on the bing images home page; only the
 * token, on a bare detailV2 url, actually opens the results.
 */
SearchResult BingProvider::handleReply(QNetworkReply *reply, const QByteArray &body) const {
    Q_UNUSED(body)
    SearchResult result;
    QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    QString token = QUrlQuery(reply->url().resolved(redirect)).queryItemValue(QStringLiteral("insightsToken"));
    if(redirect.isEmpty() || token.isEmpty()) {
        result.kind = SearchResult::Failed;
        result.error = QObject::tr("Bing did not return a results page");
        return result;
    }
    QUrl url(QStringLiteral("https://www.bing.com/images/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("view"), QStringLiteral("detailV2"));
    query.addQueryItem(QStringLiteral("insightsToken"), token);
    url.setQuery(query);
    result.kind = SearchResult::Done;
    result.url = url;
    return result;
}
