#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

// Stable boundary between the application layer and a music catalog/playback source.
// Providers must respect the upstream platform's authentication, playback and
// subscription rules. Evolve Music only chooses among URLs the provider is
// legitimately allowed to return; it never bypasses DRM or paid access controls.
class IMusicProvider : public QObject
{
    Q_OBJECT
public:
    explicit IMusicProvider(QObject *parent = nullptr) : QObject(parent) {}
    ~IMusicProvider() override = default;

    virtual QString providerId() const = 0;
    virtual QString displayName() const = 0;
    virtual QString baseUrl() const = 0;
    virtual void setBaseUrl(const QString &url) = 0;

    virtual void search(const QString &keywords, int limit = 40) = 0;
    virtual void loadHome(int limit = 14) = 0;
    virtual void loadPlaylist(const QString &playlistId, int limit = 300) = 0;
    virtual void resolveSongUrl(const QString &songId, const QString &level = "exhigh") = 0;
    virtual void resolveDownloadUrl(const QString &songId, const QString &level = "exhigh") = 0;
    virtual void loadLyrics(const QString &songId) = 0;
    virtual void ping() = 0;

    virtual bool requiresSuccessfulPing() const { return true; }
    virtual bool supportsOnDemandResolve() const { return false; }
    virtual void resolveTrack(const QVariantMap &, const QString & = "exhigh") {
        emit errorOccurred(QStringLiteral("该音乐源不支持按歌曲信息动态查找播放地址"));
    }

    // Authentication is deliberately provider-owned. The embedded browser only
    // gives a provider the session that the user created on that provider's own
    // website; the provider still decides what the account may legally play.
    virtual bool supportsWebLogin() const { return false; }
    virtual QString loginUrl() const { return {}; }
    virtual bool isAuthenticated() const { return false; }
    virtual void importWebSession(const QString &, const QString &,
                                  const QString & = QString(),
                                  const QString & = QString()) {
        emit authStateReady(false, QString(), QStringLiteral("该音乐源暂不支持网页登录同步"));
    }
    virtual void checkAuth() {
        emit authStateReady(false, QString(), QStringLiteral("未登录"));
    }
    virtual void logout() {
        emit authStateReady(false, QString(), QStringLiteral("已退出登录"));
    }

signals:
    void searchReady(const QVariantList &tracks);
    void homeReady(const QVariantList &playlists);
    void playlistReady(const QVariantMap &playlist, const QVariantList &tracks);
    void streamUrlReady(const QString &songId, const QString &url);
    void downloadUrlReady(const QString &songId, const QString &url);
    void lyricsReady(const QString &songId, const QString &lyric, const QString &translatedLyric);
    void pingReady(bool ok, const QString &message);
    void authStateReady(bool loggedIn, const QString &displayName, const QString &message);
    void errorOccurred(const QString &message);
};
