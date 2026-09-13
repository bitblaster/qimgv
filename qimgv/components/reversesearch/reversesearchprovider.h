#pragma once

#include <QByteArray>
#include <QHttpMultiPart>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>

/* One reverse image search engine.
 *
 * The engines have almost nothing in common past "post an image, get a page to open": some
 * want the file as a multipart file part, some as a base64 text field; some answer with a
 * Location header, some with a json body; some hand out an url that works as-is, some an url
 * that has to be rebuilt. So a provider owns both ends - it packs the request and it reads
 * the reply - while ReverseSearchManager keeps what really is shared: jpeg encoding, the
 * network access, object ownership and cancellation.
 */

struct SearchStep {
    QNetworkRequest request;
    QHttpMultiPart *multiPart = nullptr; // ownership passes to the manager
};

struct SearchResult {
    enum Kind { Done, Next, Failed };
    Kind kind = Failed;
    QUrl url;        // Done: the page to open
    SearchStep next; // Next: one more round trip is needed
    QString error;   // Failed: message to show
};

// the jpeg as a multipart file part, which is how most engines expect it
inline QHttpPart imageFilePart(const QString &fieldName, const QByteArray &jpeg) {
    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/jpeg"));
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QVariant("form-data; name=\"" + fieldName + "\"; filename=\"image.jpg\""));
    part.setBody(jpeg);
    return part;
}

class ReverseSearchProvider {
public:
    virtual ~ReverseSearchProvider() = default;
    virtual QString id() const = 0;          // stable key used by actions and settings
    virtual QString displayName() const = 0; // shown to the user
    // the host the result url is expected to live under; anything else is refused
    virtual QString resultHostSuffix() const = 0;
    virtual SearchStep buildRequest(const QByteArray &jpeg) const = 0;
    virtual SearchResult handleReply(QNetworkReply *reply, const QByteArray &body) const = 0;
};
