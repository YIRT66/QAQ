#include "NeteaseProvider.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTimer>
#include <QSharedPointer>
#include <QUrl>
#include <QUrlQuery>

namespace {
int bitrateForLevel(const QString &level)
{
    const QString value = level.toLower();
    if (value == QStringLiteral("standard")) return 128000;
    if (value == QStringLiteral("higher")) return 192000;
    if (value == QStringLiteral("exhigh")) return 320000;
    return 999000;
}

QStringList playbackLevelFallbacks(const QString &requested)
{
    const QString value = requested.toLower();
    if (value == QStringLiteral("standard"))
        return {QStringLiteral("standard")};
    if (value == QStringLiteral("higher"))
        return {QStringLiteral("higher"), QStringLiteral("standard")};
    if (value == QStringLiteral("exhigh"))
        return {QStringLiteral("exhigh"), QStringLiteral("higher"), QStringLiteral("standard")};
    return {value.isEmpty() ? QStringLiteral("lossless") : value,
            QStringLiteral("exhigh"), QStringLiteral("higher"), QStringLiteral("standard")};
}

bool sessionTransportAllowed(const QString &baseUrl)
{
    const QUrl url(baseUrl);
    const QString host = url.host().toLower();
    return url.scheme().toLower() == "https" || host == "localhost" || host == "127.0.0.1" || host == "::1";
}
}

NeteaseProvider::NeteaseProvider(QObject *parent) : IMusicProvider(parent) {}

void NeteaseProvider::setBaseUrl(const QString &url)
{
    QString normalized = url.trimmed();
    while (normalized.endsWith('/')) normalized.chop(1);
    if (normalized.isEmpty() || normalized == m_baseUrl) return;
    m_baseUrl = normalized;
    emit baseUrlChanged();
}

bool NeteaseProvider::endpointNeedsAuthenticatedSession(
    const QString &path) const
{
    return path == QStringLiteral("/song/url/v1")
        || path == QStringLiteral("/song/url")
        || path == QStringLiteral("/login/status");
}

bool NeteaseProvider::isReadOnlyEndpoint(const QString &path) const
{
    static const QSet<QString> allowed{
        QStringLiteral("/cloudsearch"),
        QStringLiteral("/personalized"),
        QStringLiteral("/personalized/newsong"),
        QStringLiteral("/playlist/detail"),
        QStringLiteral("/playlist/track/all"),
        QStringLiteral("/song/detail"),
        QStringLiteral("/song/url/v1"),
        QStringLiteral("/song/url"),
        QStringLiteral("/lyric"),
        QStringLiteral("/login/status")
    };
    return allowed.contains(path);
}

QNetworkReply *NeteaseProvider::get(const QString &path,
                          const QList<QPair<QString, QString>> &query,
                          std::function<void(const QJsonObject &)> success,
                          std::function<void(const QString &)> failure,
                          bool useAuthenticatedSession)
{
    if (!isReadOnlyEndpoint(path)) {
        const QString message =
            QStringLiteral("网易云仅播放模式已拦截非只读接口：") + path;
        if (failure)
            failure(message);
        else
            emit errorOccurred(message);
        return nullptr;
    }

    QUrl url(m_baseUrl + path);
    QUrlQuery q;
    for (const auto &pair : query) q.addQueryItem(pair.first, pair.second);
    // v0.9 privacy mode:
    // The user's authenticated NetEase session is attached only to account
    // validation and stream URL resolution. Search, home, playlist metadata,
    // song details and lyrics remain anonymous.
    if (useAuthenticatedSession
        && endpointNeedsAuthenticatedSession(path)
        && !m_cookieHeader.isEmpty()
        && sessionTransportAllowed(m_baseUrl)) {
        q.addQueryItem(
            QStringLiteral("cookie"),
            m_cookieHeader);
    }
    q.addQueryItem("timestamp", QString::number(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  m_browserUserAgent.isEmpty() ? QStringLiteral("EvolveMusic/0.3 Qt6") : m_browserUserAgent);
    req.setRawHeader("Accept", "application/json");
    req.setTransferTimeout(8000);

    QNetworkReply *reply = m_network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, success = std::move(success), failure = std::move(failure)] {
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            const QString message = QString("音乐源请求失败：%1").arg(reply->errorString());
            if (failure) failure(message); else emit errorOccurred(message);
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            const QString message = QString("音乐源返回了无效 JSON：%1").arg(parseError.errorString());
            if (failure) failure(message); else emit errorOccurred(message);
            reply->deleteLater();
            return;
        }
        success(doc.object());
        reply->deleteLater();
    });
    return reply;
}

QVariantMap NeteaseProvider::parseTrack(const QJsonObject &song) const
{
    QVariantMap out;
    const qint64 id = static_cast<qint64>(song.value("id").toDouble());
    out["id"] = QString::number(id);
    out["title"] = song.value("name").toString("未知歌曲");

    QJsonArray artists = song.value("ar").toArray();
    if (artists.isEmpty()) artists = song.value("artists").toArray();
    QStringList artistNames;
    for (const auto &a : artists) artistNames << a.toObject().value("name").toString();
    out["artist"] = artistNames.join(" / ");

    QJsonObject album = song.value("al").toObject();
    if (album.isEmpty()) album = song.value("album").toObject();
    out["album"] = album.value("name").toString();
    out["cover"] = album.value("picUrl").toString();

    qint64 duration = static_cast<qint64>(song.value("dt").toDouble());
    if (duration <= 0) duration = static_cast<qint64>(song.value("duration").toDouble());
    out["duration"] = duration;
    out["mvId"] = QString::number(static_cast<qint64>(song.value("mv").toDouble()));

    if (song.contains("fee")) {
        const int fee = song.value("fee").toInt(-1);
        out["access"] = (fee == 0) ? "free" : "restricted";
    } else {
        out["access"] = "unknown";
    }
    out["playable"] = true;
    return out;
}

QVariantMap NeteaseProvider::parsePlaylist(const QJsonObject &item) const
{
    QVariantMap out;
    out["id"] = QString::number(static_cast<qint64>(item.value("id").toDouble()));
    out["name"] = item.value("name").toString();
    out["cover"] = item.value("picUrl").toString(item.value("coverImgUrl").toString());
    out["description"] = item.value("copywriter").toString(item.value("description").toString());
    out["playCount"] = static_cast<qint64>(item.value("playCount").toDouble());
    out["trackCount"] = item.value("trackCount").toInt();
    return out;
}

void NeteaseProvider::search(const QString &keywords, int limit)
{
    if (keywords.trimmed().isEmpty()) { emit searchReady({}); return; }
    if (m_searchReply && m_searchReply->isRunning()) m_searchReply->abort();
    m_searchReply = get("/cloudsearch", {{"keywords", keywords}, {"type", "1"}, {"limit", QString::number(limit)}},
        [this](const QJsonObject &root) {
            const QJsonArray songs = root.value("result").toObject().value("songs").toArray();
            QVariantList tracks;
            tracks.reserve(songs.size());
            for (const auto &v : songs) tracks << parseTrack(v.toObject());
            emit searchReady(tracks);
        });
}

void NeteaseProvider::loadHome(int limit)
{
    get("/personalized", {{"limit", QString::number(limit)}},
        [this](const QJsonObject &root) {
            const QJsonArray result = root.value("result").toArray();
            QVariantList playlists;
            playlists.reserve(result.size());
            for (const auto &v : result) playlists << parsePlaylist(v.toObject());
            emit homeReady(playlists);
        });
}

void NeteaseProvider::loadNewSongs(int limit)
{
    get("/personalized/newsong", {{"limit", QString::number(limit)}},
        [this](const QJsonObject &root) {
            const QJsonArray result = root.value("result").toArray();
            QVariantList tracks;
            tracks.reserve(result.size());
            for (const auto &value : result) {
                const QJsonObject item = value.toObject();
                QJsonObject song = item.value("song").toObject();
                if (song.isEmpty()) song = item;
                QVariantMap track = parseTrack(song);
                if (track.value("cover").toString().isEmpty())
                    track["cover"] = item.value("picUrl").toString();
                const QString sourceId = track.value("id").toString();
                track["sourceId"] = sourceId;
                track["providerId"] = QStringLiteral("netease");
                track["providerName"] = QStringLiteral("网易云音乐");
                track["sources"] = QVariantList{QVariantMap{
                    {"providerId", QStringLiteral("netease")},
                    {"providerName", QStringLiteral("网易云音乐")},
                    {"sourceId", sourceId},
                    {"access", track.value("access", "unknown")},
                    {"playable", true}
                }};
                track["sourceCount"] = 1;
                track["sourceSummary"] = QStringLiteral("网易云音乐");
                // Keep a stable provider-qualified id for discovery queues.
                track["id"] = QStringLiteral("discovery:netease:") + sourceId;
                tracks << track;
            }
            emit newSongsReady(tracks);
        }, [this](const QString &) {
            // Home discovery should fail quietly; recommendations and playback
            // remain usable if this optional endpoint is unavailable.
            emit newSongsReady({});
        });
}

void NeteaseProvider::loadPlaylist(const QString &playlistId, int limit)
{
    get("/playlist/detail", {{"id", playlistId}},
        [this, playlistId, limit](const QJsonObject &detailRoot) {
            const QJsonObject playlistObj = detailRoot.value("playlist").toObject();
            QVariantMap playlist = parsePlaylist(playlistObj);

            auto parseSongs = [this](const QJsonArray &songs) {
                QVariantList tracks;
                tracks.reserve(songs.size());
                for (const auto &value : songs) {
                    const QJsonObject song = value.toObject();
                    if (!song.isEmpty())
                        tracks << parseTrack(song);
                }
                return tracks;
            };

            // v0.6.2: show whatever /playlist/detail already contains first.
            // Some API/account combinations provide a usable `tracks` array
            // here even when /playlist/track/all later returns an empty body.
            const QVariantList detailTracks = parseSongs(playlistObj.value("tracks").toArray());
            if (!detailTracks.isEmpty())
                emit playlistReady(playlist, detailTracks);

            // Keep the IDs from playlist/detail as a reliable fallback.
            QStringList fallbackIds;
            const QJsonArray trackIds = playlistObj.value("trackIds").toArray();
            const int wanted = qMin(limit, trackIds.size());
            fallbackIds.reserve(wanted);
            for (int i = 0; i < wanted; ++i) {
                const QJsonObject item = trackIds.at(i).toObject();
                const qint64 id = static_cast<qint64>(item.value("id").toDouble());
                if (id > 0)
                    fallbackIds << QString::number(id);
            }

            auto requestFallbackDetails =
                [this, playlist, detailTracks, fallbackIds]() {
                    if (fallbackIds.isEmpty()) {
                        if (detailTracks.isEmpty())
                            emit errorOccurred(QStringLiteral("歌单信息已加载，但歌曲 ID 列表为空"));
                        return;
                    }

                    // /song/detail supports a comma-separated list of song IDs.
                    // The playlist loader already caps this request to `limit`.
                    get("/song/detail", {{"ids", fallbackIds.join(',')}},
                        [this, playlist, detailTracks](const QJsonObject &root) {
                            QJsonArray songs = root.value("songs").toArray();
                            if (songs.isEmpty())
                                songs = root.value("data").toObject().value("songs").toArray();
                            if (songs.isEmpty())
                                songs = root.value("result").toObject().value("songs").toArray();

                            QVariantList tracks;
                            tracks.reserve(songs.size());
                            for (const auto &value : songs) {
                                const QJsonObject song = value.toObject();
                                if (!song.isEmpty())
                                    tracks << parseTrack(song);
                            }

                            if (!tracks.isEmpty()) {
                                emit playlistReady(playlist, tracks);
                            } else if (detailTracks.isEmpty()) {
                                emit errorOccurred(QStringLiteral("歌单歌曲详情返回为空，请稍后重试"));
                            }
                        },
                        [this, detailTracks](const QString &message) {
                            if (detailTracks.isEmpty())
                                emit errorOccurred(QStringLiteral("歌单歌曲加载失败：") + message);
                        });
                };

            get("/playlist/track/all",
                {{"id", playlistId},
                 {"limit", QString::number(limit)},
                 {"offset", "0"}},
                [this, playlist, detailTracks, requestFallbackDetails](const QJsonObject &tracksRoot) {
                    // Be tolerant of API wrappers/variants. The current
                    // api-enhanced route normally returns `songs` at the root,
                    // but accepting nested forms prevents a blank playlist.
                    QJsonArray songs = tracksRoot.value("songs").toArray();
                    if (songs.isEmpty())
                        songs = tracksRoot.value("data").toObject().value("songs").toArray();
                    if (songs.isEmpty())
                        songs = tracksRoot.value("result").toObject().value("songs").toArray();

                    QVariantList tracks;
                    tracks.reserve(songs.size());
                    for (const auto &value : songs) {
                        const QJsonObject song = value.toObject();
                        if (!song.isEmpty())
                            tracks << parseTrack(song);
                    }

                    if (!tracks.isEmpty()) {
                        emit playlistReady(playlist, tracks);
                        return;
                    }

                    // Never overwrite a usable progressive list with an empty
                    // response. Fall back to playlist.trackIds -> song/detail.
                    requestFallbackDetails();
                },
                [requestFallbackDetails](const QString &) {
                    requestFallbackDetails();
                });
        },
        [this](const QString &message) {
            emit errorOccurred(QStringLiteral("歌单详情加载失败：") + message);
        });
}

void NeteaseProvider::resolveSongUrl(const QString &songId, const QString &level)
{
    if (m_streamReply && m_streamReply->isRunning())
        m_streamReply->abort();

    const quint64 generation = ++m_streamGeneration;
    const QStringList levels = playbackLevelFallbacks(level);

    struct ResolveRace {
        int pending = 0;
        bool done = false;
    };
    const auto state = QSharedPointer<ResolveRace>::create();
    state->pending = levels.size() * 2;

    auto finishFailure = [this, songId, generation, state]() {
        if (generation != m_streamGeneration || state->done)
            return;
        --state->pending;
        if (state->pending <= 0) {
            state->done = true;
            emit streamUrlReady(songId, QString());
        }
    };

    for (int levelIndex = 0; levelIndex < levels.size(); ++levelIndex) {
        const QString candidateLevel = levels.at(levelIndex);

        for (int routeIndex = 0; routeIndex < 2; ++routeIndex) {
            const int staggerMs = levelIndex == 0 ? 0 : 90 * levelIndex;
            QTimer::singleShot(staggerMs, this,
                [this, songId, candidateLevel, routeIndex, generation, state, finishFailure]() {
                    if (generation != m_streamGeneration || state->done) {
                        finishFailure();
                        return;
                    }

                    const bool modernRoute = routeIndex == 0;
                    const QString path = modernRoute
                        ? QStringLiteral("/song/url/v1")
                        : QStringLiteral("/song/url");
                    QList<QPair<QString, QString>> query{
                        {QStringLiteral("id"), songId}
                    };
                    if (modernRoute)
                        query.append({QStringLiteral("level"), candidateLevel});
                    else
                        query.append({QStringLiteral("br"),
                                      QString::number(bitrateForLevel(candidateLevel))});

                    QNetworkReply *reply = get(
                        path,
                        query,
                        [this, songId, generation, state, finishFailure](const QJsonObject &root) {
                            if (generation != m_streamGeneration || state->done)
                                return;
                            const QJsonArray data = root.value(QStringLiteral("data")).toArray();
                            QString url;
                            if (!data.isEmpty())
                                url = data.first().toObject()
                                          .value(QStringLiteral("url"))
                                          .toString().trimmed();
                            if (!url.isEmpty()) {
                                state->done = true;
                                emit streamUrlReady(songId, url);
                                return;
                            }
                            finishFailure();
                        },
                        [generation, this, state, finishFailure](const QString &) {
                            if (generation != m_streamGeneration || state->done)
                                return;
                            finishFailure();
                        });
                    if (reply)
                        m_streamReply = reply;
                    else
                        finishFailure();
                });
        }
    }
}

void NeteaseProvider::resolveSongUrlAttempt(const QString &songId,
                                            const QStringList &levels,
                                            int levelIndex,
                                            int routeIndex,
                                            quint64 generation)
{
    if (generation != m_streamGeneration)
        return;

    if (levelIndex >= levels.size()) {
        emit streamUrlReady(songId, QString());
        return;
    }

    if (routeIndex >= 2) {
        resolveSongUrlAttempt(songId, levels, levelIndex + 1, 0, generation);
        return;
    }

    const QString level = levels.at(levelIndex);
    const bool modernRoute = routeIndex == 0;
    const QString path = modernRoute
        ? QStringLiteral("/song/url/v1")
        : QStringLiteral("/song/url");

    QList<QPair<QString, QString>> query{
        {QStringLiteral("id"), songId}
    };
    if (modernRoute) {
        query.append({QStringLiteral("level"), level});
    } else {
        query.append({QStringLiteral("br"),
                      QString::number(bitrateForLevel(level))});
    }

    m_streamReply = get(
        path,
        query,
        [this, songId, levels, levelIndex, routeIndex, generation](const QJsonObject &root) {
            if (generation != m_streamGeneration)
                return;

            const QJsonArray data = root.value(QStringLiteral("data")).toArray();
            QString url;
            if (!data.isEmpty())
                url = data.first().toObject().value(QStringLiteral("url")).toString().trimmed();

            if (!url.isEmpty()) {
                emit streamUrlReady(songId, url);
                return;
            }

            resolveSongUrlAttempt(songId, levels, levelIndex, routeIndex + 1, generation);
        },
        [this, songId, levels, levelIndex, routeIndex, generation](const QString &) {
            if (generation != m_streamGeneration)
                return;
            resolveSongUrlAttempt(songId, levels, levelIndex, routeIndex + 1, generation);
        });
}

void NeteaseProvider::resolveDownloadUrl(const QString &songId, const QString &level)
{
    // Authenticated NetEase credentials are playback-only. Downloads may use
    // an anonymous/free URL, but a membership session is never attached here.
    get("/song/url/v1", {{"id", songId}, {"level", level}},
        [this, songId](const QJsonObject &root) {
            const QJsonArray data = root.value("data").toArray();
            QString url;
            if (!data.isEmpty())
                url = data.first().toObject().value("url").toString();
            emit downloadUrlReady(songId, url);
        },
        {},
        false);
}

void NeteaseProvider::loadLyrics(const QString &songId)
{
    if (m_lyricsReply && m_lyricsReply->isRunning()) m_lyricsReply->abort();
    m_lyricsReply = get("/lyric", {{"id", songId}},
        [this, songId](const QJsonObject &root) {
            const QString lyric = root.value("lrc").toObject().value("lyric").toString();
            const QString translated = root.value("tlyric").toObject().value("lyric").toString();
            emit lyricsReady(songId, lyric, translated);
        });
}

void NeteaseProvider::ping()
{
    get("/personalized", {{"limit", "1"}}, [this](const QJsonObject &root) {
        const bool ok = root.value("code").toInt(200) == 200 || root.contains("result");
        emit pingReady(ok, ok ? "网易云音乐源连接正常" : "音乐源响应异常");
    }, [this](const QString &message) {
        emit pingReady(false, message);
    });
}

void NeteaseProvider::importWebSession(const QString &cookieHeader, const QString &userAgent,
                                       const QString &storageJson, const QString &pageUrl)
{
    Q_UNUSED(pageUrl)
    if (!sessionTransportAllowed(m_baseUrl)) {
        emit authStateReady(false, QString(), QStringLiteral("为保护账号 Cookie，登录同步只允许 HTTPS 或本机回环地址"));
        return;
    }

    QString resolvedCookie = cookieHeader.trimmed();

    // v0.3.1 NetEase QR login is rendered inside the embedded WebView but talks
    // only to the user's local Netease API service. The successful QR-check
    // response is stored in localStorage and transferred through AccountManager.
    // This avoids depending on the full music.163.com desktop page rendering
    // correctly inside every WebView2 installation.
    if (resolvedCookie.isEmpty() && !storageJson.trimmed().isEmpty()) {
        QJsonParseError storageError{};
        const QJsonDocument storageDoc = QJsonDocument::fromJson(storageJson.toUtf8(), &storageError);
        if (storageError.error == QJsonParseError::NoError && storageDoc.isObject()) {
            const QJsonObject localStorage = storageDoc.object().value("localStorage").toObject();
            resolvedCookie = localStorage.value("evolve_netease_cookie").toString().trimmed();
        }
    }

    m_cookieHeader = resolvedCookie;
    m_browserUserAgent = userAgent.trimmed();
    if (m_cookieHeader.isEmpty()) {
        m_loggedIn = false;
        emit authStateReady(false, QString(), QStringLiteral("没有读取到网易云登录会话，请重新扫码登录"));
        return;
    }
    validateCurrentSession(QStringLiteral("登录状态已同步 · 仅播放授权"));
}

void NeteaseProvider::checkAuth()
{
    if (m_cookieHeader.isEmpty()) {
        m_loggedIn = false;
        emit authStateReady(false, QString(), QStringLiteral("未同步登录状态"));
        return;
    }
    validateCurrentSession();
}

void NeteaseProvider::logout()
{
    m_cookieHeader.clear();
    m_browserUserAgent.clear();
    m_loggedIn = false;
    emit authStateReady(false, QString(), QStringLiteral("已退出网易云登录"));
}

void NeteaseProvider::validateCurrentSession(const QString &successMessage)
{
    get("/login/status", {}, [this, successMessage](const QJsonObject &root) {
        const QJsonObject data = root.value("data").toObject();
        const QJsonObject account = data.value("account").toObject();
        const QJsonObject profile = data.value("profile").toObject();
        const bool ok = !account.isEmpty() && account.value("id").toVariant().toLongLong() > 0;
        m_loggedIn = ok;
        const QString nickname = profile.value("nickname").toString();
        if (ok) {
            emit authStateReady(true, nickname, successMessage.isEmpty() ? QStringLiteral("网易云账号已登录 · 仅播放授权") : successMessage);
        } else {
            emit authStateReady(false, QString(), QStringLiteral("网易云登录会话未通过账号验证"));
        }
    }, [this](const QString &message) {
        m_loggedIn = false;
        emit authStateReady(false, QString(), QStringLiteral("登录状态验证失败：") + message);
    });
}
