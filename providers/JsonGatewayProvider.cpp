#include "JsonGatewayProvider.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace {
bool sensitiveTransportAllowed(const QString &baseUrl)
{
    const QUrl url(baseUrl);
    const QString host = url.host().toLower();
    return url.scheme().toLower() == "https" || host == "localhost" || host == "127.0.0.1" || host == "::1";
}
}

JsonGatewayProvider::JsonGatewayProvider(const QString &id,
                                         const QString &name,
                                         const QString &defaultBaseUrl,
                                         const QString &loginUrl,
                                         QObject *parent)
    : IMusicProvider(parent), m_id(id), m_name(name), m_baseUrl(defaultBaseUrl), m_loginUrl(loginUrl)
{
}

void JsonGatewayProvider::setBaseUrl(const QString &url)
{
    QString normalized = url.trimmed();
    while (normalized.endsWith('/')) normalized.chop(1);
    m_baseUrl = normalized;
}

QNetworkReply *JsonGatewayProvider::get(const QString &path,
                              const QList<QPair<QString, QString>> &query,
                              std::function<void(const QJsonObject &)> success,
                              std::function<void(const QString &)> failure)
{
    if (m_baseUrl.isEmpty()) {
        const QString message = m_name + " 尚未配置网关地址";
        if (failure) failure(message); else emit errorOccurred(message);
        return nullptr;
    }

    QUrl url(m_baseUrl + path);
    QUrlQuery q;
    for (const auto &pair : query) q.addQueryItem(pair.first, pair.second);
    q.addQueryItem("_t", QString::number(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "EvolveMusic/0.3 Qt6");
    req.setRawHeader("Accept", "application/json");
    req.setTransferTimeout(5000);

    QNetworkReply *reply = m_network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, success = std::move(success), failure = std::move(failure)] {
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            const QString message = QString("%1 网关请求失败：%2").arg(m_name, reply->errorString());
            if (failure) failure(message); else emit errorOccurred(message);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            const QString message = QString("%1 网关返回无效 JSON：%2").arg(m_name, parseError.errorString());
            if (failure) failure(message); else emit errorOccurred(message);
            reply->deleteLater();
            return;
        }
        success(doc.object());
        reply->deleteLater();
    });
    return reply;
}

void JsonGatewayProvider::post(const QString &path,
                               const QJsonObject &body,
                               std::function<void(const QJsonObject &)> success,
                               std::function<void(const QString &)> failure)
{
    if (m_baseUrl.isEmpty()) {
        const QString message = m_name + " 尚未配置网关地址";
        if (failure) failure(message); else emit errorOccurred(message);
        return;
    }
    QNetworkRequest req(QUrl(m_baseUrl + path));
    req.setHeader(QNetworkRequest::UserAgentHeader, "EvolveMusic/0.3 Qt6");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setTransferTimeout(5000);
    QNetworkReply *reply = m_network.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, success = std::move(success), failure = std::move(failure)] {
        if (reply->error() != QNetworkReply::NoError) {
            const QString message = QString("%1 网关请求失败：%2").arg(m_name, reply->errorString());
            if (failure) failure(message); else emit errorOccurred(message);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            const QString message = QString("%1 网关返回无效 JSON：%2").arg(m_name, parseError.errorString());
            if (failure) failure(message); else emit errorOccurred(message);
            reply->deleteLater();
            return;
        }
        success(doc.object());
        reply->deleteLater();
    });
}

QVariantMap JsonGatewayProvider::parseTrack(const QJsonObject &track) const
{
    QVariantMap out;
    out["id"] = track.value("id").toVariant().toString();
    out["title"] = track.value("title").toString(track.value("name").toString("未知歌曲"));
    out["artist"] = track.value("artist").toString();
    out["album"] = track.value("album").toString();
    out["cover"] = track.value("cover").toString();
    out["duration"] = track.value("duration").toVariant().toLongLong();
    const QString access = track.value("access").toString("unknown").toLower();
    out["access"] = access;
    out["playable"] = track.contains("playable") ? track.value("playable").toBool() : true;
    return out;
}

QVariantMap JsonGatewayProvider::parsePlaylist(const QJsonObject &playlist) const
{
    QVariantMap out;
    out["id"] = playlist.value("id").toVariant().toString();
    out["name"] = playlist.value("name").toString();
    out["cover"] = playlist.value("cover").toString();
    out["description"] = playlist.value("description").toString();
    out["playCount"] = playlist.value("playCount").toVariant().toLongLong();
    out["trackCount"] = playlist.value("trackCount").toInt();
    return out;
}

void JsonGatewayProvider::search(const QString &keywords, int limit)
{
    if (keywords.trimmed().isEmpty()) { emit searchReady({}); return; }
    if (m_searchReply && m_searchReply->isRunning()) m_searchReply->abort();
    m_searchReply = get("/search", {{"keywords", keywords}, {"limit", QString::number(limit)}}, [this](const QJsonObject &root) {
        const QJsonArray rows = root.value("tracks").toArray();
        QVariantList tracks;
        tracks.reserve(rows.size());
        for (const auto &row : rows) tracks << parseTrack(row.toObject());
        emit searchReady(tracks);
    });
}

void JsonGatewayProvider::loadHome(int limit)
{
    get("/home", {{"limit", QString::number(limit)}}, [this](const QJsonObject &root) {
        const QJsonArray rows = root.value("playlists").toArray();
        QVariantList playlists;
        playlists.reserve(rows.size());
        for (const auto &row : rows) playlists << parsePlaylist(row.toObject());
        emit homeReady(playlists);
    });
}

void JsonGatewayProvider::loadPlaylist(const QString &playlistId, int limit)
{
    get("/playlist", {{"id", playlistId}, {"limit", QString::number(limit)}}, [this](const QJsonObject &root) {
        QVariantMap playlist = parsePlaylist(root.value("playlist").toObject());
        QVariantList tracks;
        for (const auto &row : root.value("tracks").toArray()) tracks << parseTrack(row.toObject());
        emit playlistReady(playlist, tracks);
    });
}

void JsonGatewayProvider::resolveSongUrl(const QString &songId, const QString &level)
{
    if (m_streamReply && m_streamReply->isRunning()) m_streamReply->abort();
    m_streamReply = get("/stream", {{"id", songId}, {"quality", level}}, [this, songId](const QJsonObject &root) {
        emit streamUrlReady(songId, root.value("url").toString());
    });
}

void JsonGatewayProvider::resolveDownloadUrl(const QString &songId, const QString &level)
{
    get("/download", {{"id", songId}, {"quality", level}}, [this, songId](const QJsonObject &root) {
        emit downloadUrlReady(songId, root.value("url").toString());
    });
}

void JsonGatewayProvider::loadLyrics(const QString &songId)
{
    if (m_lyricsReply && m_lyricsReply->isRunning()) m_lyricsReply->abort();
    m_lyricsReply = get("/lyrics", {{"id", songId}}, [this, songId](const QJsonObject &root) {
        emit lyricsReady(songId, root.value("lyric").toString(), root.value("translatedLyric").toString());
    });
}

void JsonGatewayProvider::ping()
{
    get("/ping", {}, [this](const QJsonObject &root) {
        const bool ok = root.value("ok").toBool(true);
        emit pingReady(ok, root.value("message").toString(ok ? (m_name + " 连接正常") : (m_name + " 连接异常")));
    }, [this](const QString &message) {
        emit pingReady(false, message);
    });
}

void JsonGatewayProvider::importWebSession(const QString &cookieHeader, const QString &userAgent,
                                           const QString &storageJson, const QString &pageUrl)
{
    if (!sensitiveTransportAllowed(m_baseUrl)) {
        m_loggedIn = false;
        emit authStateReady(false, QString(), m_name + " 网关不是 HTTPS/本机地址，为保护账号 Cookie 已拒绝同步");
        return;
    }
    if (cookieHeader.trimmed().isEmpty() && storageJson.trimmed().isEmpty()) {
        m_loggedIn = false;
        emit authStateReady(false, QString(), m_name + " 登录页没有读取到可同步的会话信息");
        return;
    }
    post("/auth/import",
         {{"cookie", cookieHeader},
          {"userAgent", userAgent},
          {"storage", storageJson},
          {"pageUrl", pageUrl}},
         [this](const QJsonObject &root) {
        applyAuthResponse(root, m_name + " 网页登录已同步");
    }, [this](const QString &message) {
        m_loggedIn = false;
        emit authStateReady(false, QString(), "网页登录已保存在内嵌浏览器，但 " + message + "；需要可用的授权网关才能同步到播放器。" );
    });
}

void JsonGatewayProvider::checkAuth()
{
    get("/auth/status", {}, [this](const QJsonObject &root) {
        applyAuthResponse(root, m_name + " 登录状态已刷新");
    }, [this](const QString &message) {
        m_loggedIn = false;
        emit authStateReady(false, QString(), "账号状态不可用：" + message);
    });
}

void JsonGatewayProvider::logout()
{
    post("/auth/logout", {}, [this](const QJsonObject &) {
        m_loggedIn = false;
        emit authStateReady(false, QString(), m_name + " 已退出登录");
    }, [this](const QString &) {
        m_loggedIn = false;
        emit authStateReady(false, QString(), m_name + " 本地登录状态已清除");
    });
}

void JsonGatewayProvider::applyAuthResponse(const QJsonObject &root, const QString &fallbackMessage)
{
    const bool loggedIn = root.value("loggedIn").toBool(root.value("ok").toBool(false));
    m_loggedIn = loggedIn;
    const QString displayName = root.value("displayName").toString(root.value("nickname").toString());
    const QString message = root.value("message").toString(fallbackMessage);
    emit authStateReady(loggedIn, displayName, message);
}
