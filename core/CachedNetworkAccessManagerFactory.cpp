#include "CachedNetworkAccessManagerFactory.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkRequest>

namespace {
class EvolveNetworkAccessManager final : public QNetworkAccessManager
{
public:
    explicit EvolveNetworkAccessManager(QObject *parent = nullptr)
        : QNetworkAccessManager(parent) {}

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &original,
                                 QIODevice *outgoingData = nullptr) override
    {
        QNetworkRequest request(original);
        // Several music CDNs are noticeably more reliable when requests look like
        // normal desktop image traffic. This affects artwork only; provider API
        // requests still use their own QNetworkAccessManager instances.
        if (!request.hasRawHeader("User-Agent"))
            request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 EvolveMusic/0.18.1");
        if (!request.hasRawHeader("Accept"))
            request.setRawHeader("Accept", "image/avif,image/webp,image/apng,image/*,*/*;q=0.8");
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
        request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);
        // QML Image has no per-item network timeout. A dead NetEase image edge
        // must fail quickly so CoverImage can advance to the Worker/mirror
        // candidate instead of showing a note placeholder for tens of seconds.
        request.setTransferTimeout(3500);
        return QNetworkAccessManager::createRequest(op, request, outgoingData);
    }
};
}

QNetworkAccessManager *CachedNetworkAccessManagerFactory::create(QObject *parent)
{
    auto *manager = new EvolveNetworkAccessManager(parent);
    auto *cache = new QNetworkDiskCache(manager);

    QString root = qEnvironmentVariable("EVOLVE_MUSIC_CACHE_DIR");
    if (root.trimmed().isEmpty()) {
        // Installed builds often live under Program Files, where writing
        // applicationDirPath()/runtime/cache can fail silently for normal
        // users. Put artwork/network cache in the per-user writable cache
        // directory so covers survive restarts and actually benefit from disk
        // caching after installation.
        root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (root.trimmed().isEmpty())
            root = QDir::tempPath() + QStringLiteral("/EvolveMusic-cache");
    }
    const QString path = QDir(root).filePath("qml-network");
    QDir().mkpath(path);

    cache->setCacheDirectory(path);
    cache->setMaximumCacheSize(512ll * 1024ll * 1024ll);
    manager->setCache(cache);
    return manager;
}
