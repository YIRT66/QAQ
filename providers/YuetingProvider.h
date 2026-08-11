#pragma once

#include "IMusicProvider.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <functional>

class YuetingProvider final : public IMusicProvider
{
    Q_OBJECT
public:
    explicit YuetingProvider(QObject *parent = nullptr);

    QString providerId() const override { return QStringLiteral("yueting"); }
    QString displayName() const override { return QStringLiteral("悦听音乐"); }
    QString baseUrl() const override { return m_baseUrl; }
    void setBaseUrl(const QString &url) override;
    bool requiresSuccessfulPing() const override { return false; }
    bool supportsOnDemandResolve() const override { return true; }

    void search(const QString &keywords, int limit = 40) override;
    void loadHome(int limit = 14) override;
    void loadPlaylist(const QString &, int = 300) override;
    void resolveSongUrl(const QString &songId, const QString &level = "exhigh") override;
    void resolveDownloadUrl(const QString &, const QString & = "exhigh") override;
    void loadLyrics(const QString &) override;
    void resolveTrack(const QVariantMap &track, const QString &level = "exhigh") override;
    void ping() override;

private:
    using SearchCallback = std::function<void(const QVariantList &)>;
    using ErrorCallback = std::function<void(const QString &)>;
    void fetchSearch(const QString &keywords, int limit, SearchCallback callback, ErrorCallback failure = {});
    QVariantList parseSearch(const QString &html, int limit) const;
    QString pageUrl(const QString &songId) const;
    QNetworkReply *get(const QUrl &url,
                       std::function<void(const QString &)> success,
                       std::function<void(const QString &)> failure = {});

    QString m_baseUrl = QStringLiteral("https://www.yueting.net");
    QNetworkAccessManager m_network;
    quint64 m_searchGeneration = 0;
};
