#include "reversesearchmanager.h"

#include <QBuffer>
#include "components/reversesearch/providers/bingprovider.h"
#include "components/reversesearch/providers/googlelensprovider.h"
#include "components/reversesearch/providers/tineyeprovider.h"

// engines choke on huge uploads and gain nothing from them - this is plenty to recognize by
static const int maxUploadSide = 2048;
static const int jpegQuality = 85;

ReverseSearchManager::ReverseSearchManager(QObject *parent) : QObject(parent) {
    // the only place to touch when adding an engine
    providers.push_back(std::make_unique<GoogleLensProvider>());
    providers.push_back(std::make_unique<BingProvider>());
    providers.push_back(std::make_unique<TinEyeProvider>());
}

void ReverseSearchManager::search(QString providerId, std::shared_ptr<const QImage> image) {
    auto provider = findProvider(providerId);
    if(!provider) {
        emit failed(tr("Unknown search engine: ") + providerId);
        return;
    }
    if(!image || image->isNull()) {
        emit failed(tr("Could not read image"));
        return;
    }
    // pressing the shortcut again replaces the search in flight instead of opening two tabs
    if(activeReply) {
        activeReply->abort();
        activeReply = nullptr;
    }
    QByteArray jpeg = encodeJpeg(*image);
    if(jpeg.isEmpty()) {
        emit failed(tr("Could not encode image"));
        return;
    }
    send(provider->buildRequest(jpeg), provider);
}

/* Re-encode whatever we are showing as a moderately sized jpeg. This makes the source format
 * irrelevant - tiff, heic and raw all reach the engine as something it accepts - and keeps
 * uploads small enough that bing doesn't silently drop them.
 */
QByteArray ReverseSearchManager::encodeJpeg(const QImage &image) {
    QImage scaled = image;
    if(scaled.width() > maxUploadSide || scaled.height() > maxUploadSide)
        scaled = scaled.scaled(maxUploadSide, maxUploadSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly);
    if(!scaled.save(&buffer, "JPG", jpegQuality))
        return QByteArray();
    return jpeg;
}

const ReverseSearchProvider *ReverseSearchManager::findProvider(const QString &providerId) const {
    for(auto const &provider : providers) {
        if(provider->id() == providerId)
            return provider.get();
    }
    return nullptr;
}

void ReverseSearchManager::send(SearchStep step, const ReverseSearchProvider *provider) {
    // we want the Location header itself, not the page it points at
    step.request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *reply = step.multiPart ? net.post(step.request, step.multiPart)
                                          : net.get(step.request);
    if(step.multiPart)
        step.multiPart->setParent(reply);
    activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, provider]() {
        onReplyFinished(reply, provider);
    });
}

void ReverseSearchManager::onReplyFinished(QNetworkReply *reply, const ReverseSearchProvider *provider) {
    reply->deleteLater();
    // a reply we aborted in favor of a newer search
    if(activeReply != reply)
        return;
    activeReply = nullptr;
    if(reply->error() == QNetworkReply::OperationCanceledError)
        return;
    // a redirect arrives here as an error-free reply, so only bail out on real failures
    if(reply->error() != QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isNull()) {
        emit failed(tr("Search failed: ") + reply->errorString());
        return;
    }
    SearchResult result = provider->handleReply(reply, reply->readAll());
    switch(result.kind) {
        case SearchResult::Done:
            // never hand an unexpected host to the browser
            if(!result.url.isValid() || !result.url.host().endsWith(provider->resultHostSuffix()))
                emit failed(tr("Search failed: unexpected response from ") + provider->displayName());
            else
                emit ready(result.url);
            break;
        case SearchResult::Next:
            send(result.next, provider);
            break;
        case SearchResult::Failed:
            emit failed(result.error);
            break;
    }
}
