#pragma once

#include "IMusicProvider.h"
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

class NeteaseProvider : public IMusicProvider
{
    Q_OBJECT
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
public:
    explicit NeteaseProvider(QObject *parent = nullptr);

    QString providerId() const override { return "netease"; }
    QString displayName() const override { return QStringLiteral("网易云音乐"); }
    QString baseUrl() const override { return m_baseUrl; }
    void setBaseUrl(const QString &url) override;

    Q_INVOKABLE void search(const QString &keywords, int limit = 40) override;
    Q_INVOKABLE void loadHome(int limit = 14) override;
    Q_INVOKABLE void loadNewSongs(int limit = 24);
    Q_INVOKABLE void loadPlaylist(const QString &playlistId, int limit = 300) override;
    Q_INVOKABLE void resolveSongUrl(const QString &songId, const QString &level = "exhigh") override;
    Q_INVOKABLE void resolveDownloadUrl(const QString &songId, const QString &level = "exhigh") override;
    Q_INVOKABLE void loadLyrics(const QString &songId) override;
    Q_INVOKABLE void ping() override;

    bool supportsWebLogin() const override { return true; }
    QString loginUrl() const override { return QStringLiteral("https://music.163.com/#/login"); }
    bool isAuthenticated() const override { return m_loggedIn; }
    void importWebSession(const QString &cookieHeader, const QString &userAgent,
                          const QString &storageJson = QString(),
                          const QString &pageUrl = QString()) override;
    void checkAuth() override;
    void logout() override;

signals:
    void baseUrlChanged();
    void newSongsReady(const QVariantList &tracks);

private:
    QNetworkReply *get(const QString &path,
             const QList<QPair<QString, QString>> &query,
             std::function<void(const QJsonObject &)> success,
             std::function<void(const QString &)> failure = {},
             bool useAuthenticatedSession = true);
    bool endpointNeedsAuthenticatedSession(const QString &path) const;
    bool isReadOnlyEndpoint(const QString &path) const;
    void validateCurrentSession(const QString &successMessage = QString());
    QVariantMap parseTrack(const QJsonObject &song) const;
    QVariantMap parsePlaylist(const QJsonObject &item) const;
    void resolveSongUrlAttempt(const QString &songId, const QStringList &levels,
                               int levelIndex, int routeIndex, quint64 generation);

    QString m_baseUrl = "http://127.0.0.1:3000";
    QString m_cookieHeader;
    QString m_browserUserAgent;
    bool m_loggedIn = false;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_searchReply;
    QPointer<QNetworkReply> m_streamReply;
    quint64 m_streamGeneration = 0;
    QPointer<QNetworkReply> m_lyricsReply;
};
