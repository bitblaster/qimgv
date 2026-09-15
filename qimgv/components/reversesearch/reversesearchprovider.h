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

/* Where the browser should post the image, for an engine that won't let us post it ourselves.
 *
 * Google Lens hands back a results url that only works for whoever uploaded: the page looks
 * up the image through the cookies the upload was answered with, so opening that url in the
 * browser - a different cookie jar - leaves it waiting for an image that, as far as it can
 * tell, was never sent. The way out is to let the browser do the upload, so that the session
 * that carries it is the one that will show the results.
 *
 * We can't ask it to post from a page of Google's, so the post comes from a page of ours,
 * served on loopback (see LocalUploadServer). Two details are what make it work at all, and
 * both are worth keeping in mind if this ever breaks:
 *
 *  - the upload endpoint answers 403 to anything with a foreign Origin header, and a page on
 *    127.0.0.1 is as foreign as it gets. A sandboxed iframe has an opaque origin, so it sends
 *    "Origin: null", which the endpoint does accept. That is why the form lives in a frame.
 *  - the reply carries X-Frame-Options: DENY, so the results can't land inside that frame.
 *    The form targets _top, which also makes it a top level navigation - the cookies Google
 *    sets along the way are then first party, which is the whole point of the exercise.
 */
struct BrowserUpload {
    QUrl action;       // the form's action, freshly built for this search
    QString fieldName; // the file field the engine expects
    bool isValid() const { return !action.isEmpty() && !fieldName.isEmpty(); }
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

    /* A provider takes one of two routes, and implements the half that goes with it: either
     * it uploads over the network, through buildRequest and handleReply, or it leaves the
     * upload to the browser and only says where it should go, through browserUpload.
     */
    virtual BrowserUpload browserUpload() const { return BrowserUpload(); }
    virtual SearchStep buildRequest(const QByteArray &jpeg) const {
        Q_UNUSED(jpeg)
        return SearchStep();
    }
    virtual SearchResult handleReply(QNetworkReply *reply, const QByteArray &body) const {
        Q_UNUSED(reply)
        Q_UNUSED(body)
        return SearchResult();
    }
};
