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
    uploadServer.stop();
    QByteArray jpeg = encodeJpeg(*image);
    if(jpeg.isEmpty()) {
        emit failed(tr("Could not encode image"));
        return;
    }
    BrowserUpload upload = provider->browserUpload();
    if(upload.isValid()) {
        searchViaBrowser(upload, jpeg, provider);
        return;
    }
    send(provider->buildRequest(jpeg), provider);
}

/* The page the browser is sent to, and the frame that does the work. The image rides along as
 * base64 inside the frame rather than as a file of its own: the frame is sandboxed, so its
 * origin is opaque and fetching anything from us would be a cross origin request we'd have to
 * open up for. Inlining costs a third of the image in size, on loopback, and saves all that.
 */
static QString uploadFrameHtml(const BrowserUpload &upload, const QByteArray &jpeg) {
    // base64 is alphanumeric, so nothing in here can break out of the string it goes into
    return QStringLiteral(R"(<!doctype html>
<meta charset="utf-8">
<body>
<script>
const b64 = "%1";
const bin = atob(b64);
const bytes = new Uint8Array(bin.length);
for(let i = 0; i < bin.length; i++)
    bytes[i] = bin.charCodeAt(i);
const file = new File([new Blob([bytes], {type: "image/jpeg"})], "image.jpg", {type: "image/jpeg"});
const form = document.createElement("form");
form.method = "POST";
form.enctype = "multipart/form-data";
form.action = "%2";
form.target = "_top";
const input = document.createElement("input");
input.type = "file";
input.name = "%3";
const transfer = new DataTransfer();
transfer.items.add(file);
input.files = transfer.files;
form.appendChild(input);
document.body.appendChild(form);
form.submit();
</script>
</body>
)").arg(QString::fromLatin1(jpeg.toBase64()),
        upload.action.toString(QUrl::FullyEncoded),
        upload.fieldName);
}

static QString uploadPageHtml(const QString &engineName) {
    return QStringLiteral(R"(<!doctype html>
<meta charset="utf-8">
<title>%1</title>
<style>
body { font-family: sans-serif; margin: 3em; color: #444; }
</style>
<body>
<p>%2</p>
<iframe src="inner" sandbox="allow-scripts allow-forms allow-top-navigation"
        style="width: 0; height: 0; border: 0;"></iframe>
</body>
)").arg(engineName, ReverseSearchManager::tr("Uploading image..."));
}

void ReverseSearchManager::searchViaBrowser(const BrowserUpload &upload, const QByteArray &jpeg,
                                            const ReverseSearchProvider *provider) {
    QUrl pageUrl = uploadServer.serve(uploadPageHtml(provider->displayName()),
                                      uploadFrameHtml(upload, jpeg));
    if(pageUrl.isEmpty()) {
        emit failed(tr("Could not open a local port for ") + provider->displayName());
        return;
    }
    emit ready(pageUrl);
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
