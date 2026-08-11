#pragma once

#include "IMusicProvider.h"
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <functional>

// Adapter for a compliant JSON gateway. This lets Evolve Music connect to
// official/licensed platform backends without hard-coding private web APIs.
// See docs/PROVIDER_GATEWAY.md for the contract.
class JsonGatewayProvider : public IMusicProvider
{
    Q_OBJECT
public:
    JsonGatewayProvider(const QString &id,
                        const QString &name,
                        const QString &defaultBaseUrl,
                        const QString &loginUrl,
                        QObject *parent = nullptr);

    QString providerId() const override { return m_id; }
    QString displayName() const override { return m_name; }
    QString baseUrl() const override { return m_baseUrl; }
    void setBaseUrl(const QString &url) override;

    void search(const QString &keywords, int limit = 40) override;
    void loadHome(int limit = 14) override;
    void loadPlaylist(const QString &playlistId, int limit = 300) override;
    void resolveSongUrl(const QString &songId, const QString &level = "exhigh") override;
    void resolveDownloadUrl(const QString &songId, const QString &level = "exhigh") override;
    void loadLyrics(const QString &songId) override;
    void ping() override;

    bool supportsWebLogin() const override { return !m_loginUrl.isEmpty(); }
    QString loginUrl() const override { return m_loginUrl; }
    bool isAuthenticated() const override { return m_loggedIn; }
    void importWebSession(const QString &cookieHeader, const QString &userAgent,
                          const QString &storageJson = QString(),
                          const QString &pageUrl = QString()) override;
    void checkAuth() override;
    void logout() override;

private:
    QNetworkReply *get(const QString &path,
             const QList<QPair<QString, QString>> &query,
             std::function<void(const QJsonObject &)> success,
             std::function<void(const QString &)> failure = {});
    void post(const QString &path,
              const QJsonObject &body,
              std::function<void(const QJsonObject &)> success,
              std::function<void(const QString &)> failure = {});
    QVariantMap parseTrack(const QJsonObject &track) const;
    QVariantMap parsePlaylist(const QJsonObject &playlist) const;
    void applyAuthResponse(const QJsonObject &root, const QString &fallbackMessage);

    QString m_id;
    QString m_name;
    QString m_baseUrl;
    QString m_loginUrl;
    bool m_loggedIn = false;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_searchReply;
    QPointer<QNetworkReply> m_streamReply;
    QPointer<QNetworkReply> m_lyricsReply;
};
