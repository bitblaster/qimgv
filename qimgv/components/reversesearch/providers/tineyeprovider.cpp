#include "tineyeprovider.h"

#include <QJsonDocument>
#include <QJsonObject>

static const char *uploadUrl = "https://tineye.com/api/v1/result_json/?sort=score&order=desc";
static const char *userAgent = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                               "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36";

SearchStep TinEyeProvider::buildRequest(const QByteArray &jpeg) const {
    SearchStep step;
    step.request.setUrl(QUrl(uploadUrl));
    step.request.setHeader(QNetworkRequest::UserAgentHeader, QVariant(userAgent));
    step.multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    step.multiPart->append(imageFilePart(QStringLiteral("image"), jpeg));
    return step;
}

/* TinEye answers the upload with json rather than a redirect; the query hash in it names the
 * page that shows the matches. A picture that is too small or too flat to fingerprint comes
 * back without one, under a suggestions key.
 */
SearchResult TinEyeProvider::handleReply(QNetworkReply *reply, const QByteArray &body) const {
    Q_UNUSED(reply)
    SearchResult result;
    QJsonObject json = QJsonDocument::fromJson(body).object();
    QString hash = json.value(QStringLiteral("query_hash")).toString();
    bool hasMatches = json.contains(QStringLiteral("num_matches"));
    if(hash.isEmpty() || !hasMatches) {
        result.kind = SearchResult::Failed;
        result.error = QObject::tr("TinEye could not fingerprint this image");
        return result;
    }
    result.kind = SearchResult::Done;
    result.url = QUrl(QStringLiteral("https://tineye.com/search/") + hash);
    return result;
}
