#include "OurcraftProvider.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <QtGlobal>
#include <memory>

namespace {
QString firstNonEmpty(const QStringList &values)
{
    for (const QString &value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) return trimmed;
    }
    return {};
}

QJsonValue firstValue(const QJsonObject &object, const QStringList &keys)
{
    for (const QString &key : keys) {
        if (object.contains(key) && !object.value(key).isNull()
            && !object.value(key).isUndefined()) {
            return object.value(key);
        }
    }
    return {};
}

QJsonArray arrayFromObject(const QJsonObject &root)
{
    // ourcraft-music-api is MetingJS compatible. Deployments in the wild use
    // both a bare array and wrapped result/data/tracks/songs forms, so accept
    // all of them instead of binding EvolveMusic to one minor server version.
    const QStringList directKeys{
        QStringLiteral("tracks"), QStringLiteral("songs"),
        QStringLiteral("data"), QStringLiteral("result"),
        QStringLiteral("playlist"), QStringLiteral("list"),
        QStringLiteral("items")};

    for (const QString &key : directKeys) {
        const QJsonValue value = root.value(key);
        if (value.isArray()) return value.toArray();
        if (!value.isObject()) continue;
        const QJsonObject nested = value.toObject();
        for (const QString &nestedKey : directKeys) {
            const QJsonValue nestedValue = nested.value(nestedKey);
            if (nestedValue.isArray()) return nestedValue.toArray();
        }
    }
    return {};
}

QString sourceIdFromUrl(const QString &rawUrl)
{
    const QUrl url(rawUrl.trimmed());
    if (!url.isValid()) return {};

    const QUrlQuery query(url);
    return query.queryItemValue(
        QStringLiteral("id"),
        QUrl::FullyDecoded).trimmed();
}

bool isOurcraftResolverUrl(const QString &rawUrl)
{
    const QUrl url(rawUrl.trimmed());
    if (!url.isValid()) return false;

    const QUrlQuery query(url);
    return url.path().endsWith(QStringLiteral("/api"), Qt::CaseInsensitive)
        && query.queryItemValue(QStringLiteral("type"), QUrl::FullyDecoded)
               .compare(QStringLiteral("url"), Qt::CaseInsensitive) == 0;
}
}

OurcraftProvider::OurcraftProvider(const QString &id,
                                   const QString &name,
                                   const QString &server,
                                   const QString &defaultBaseUrl,
                                   QObject *parent)
    : IMusicProvider(parent),
      m_id(id),
      m_name(name),
      m_server(server),
      m_baseUrl(defaultBaseUrl)
{
    setBaseUrl(defaultBaseUrl);
}

void OurcraftProvider::setBaseUrl(const QString &url)
{
    QString normalized = url.trimmed();
    if (normalized.isEmpty()) normalized = QStringLiteral("https://music.yuncan.xyz");
    while (normalized.endsWith(QLatin1Char('/'))) normalized.chop(1);
    if (normalized.endsWith(QStringLiteral("/api"), Qt::CaseInsensitive))
        normalized.chop(4);
    while (normalized.endsWith(QLatin1Char('/'))) normalized.chop(1);
    m_baseUrl = normalized;
}

QNetworkReply *OurcraftProvider::get(
    const QList<QPair<QString, QString>> &query,
    std::function<void(const QJsonDocument &, const QByteArray &)> success,
    std::function<void(const QString &)> failure,
    int timeoutMs)
{
    if (m_baseUrl.trimmed().isEmpty()) {
        const QString message = QStringLiteral("%1 未配置 Ourcraft Music API 地址").arg(m_name);
        if (failure) failure(message); else emit errorOccurred(message);
        return nullptr;
    }

    QUrl url(m_baseUrl + QStringLiteral("/api"));
    QUrlQuery q;
    for (const auto &pair : query) q.addQueryItem(pair.first, pair.second);
    q.addQueryItem(QStringLiteral("_t"), QString::number(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(q);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("EvolveMusic/0.18.1 Qt6 OurcraftClient"));
    request.setRawHeader("Accept", "application/json,text/plain,*/*");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(timeoutMs);

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, success = std::move(success), failure = std::move(failure)] {
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QString message = QStringLiteral("%1 请求失败：%2").arg(m_name, reply->errorString());
            if (http > 0) message += QStringLiteral(" (HTTP %1)").arg(http);
            if (failure) failure(message); else emit errorOccurred(message);
            reply->deleteLater();
            return;
        }

        const QByteArray raw = reply->readAll();
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
        // lrc endpoints are allowed to return plain text. The caller can use
        // raw even when parsing fails.
        success(parseError.error == QJsonParseError::NoError ? doc : QJsonDocument{}, raw);
        reply->deleteLater();
    });
    return reply;
}

QString OurcraftProvider::jsonString(const QJsonValue &value)
{
    if (value.isString()) return value.toString();
    if (value.isDouble()) return QString::number(value.toVariant().toLongLong());
    if (value.isBool()) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return value.toVariant().toString();
}

QString OurcraftProvider::artistText(const QJsonValue &value)
{
    if (value.isString()) return value.toString();
    if (value.isArray()) {
        QStringList names;
        for (const QJsonValue &entry : value.toArray()) {
            if (entry.isString()) names << entry.toString();
            else if (entry.isObject()) {
                const QJsonObject object = entry.toObject();
                const QString name = firstNonEmpty({object.value(QStringLiteral("name")).toString(),
                                                    object.value(QStringLiteral("artist")).toString()});
                if (!name.isEmpty()) names << name;
            }
        }
        return names.join(QStringLiteral(" / "));
    }
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        return firstNonEmpty({object.value(QStringLiteral("name")).toString(),
                              object.value(QStringLiteral("artist")).toString()});
    }
    return {};
}

qint64 OurcraftProvider::durationMs(const QJsonValue &value)
{
    if (value.isDouble()) {
        qint64 n = value.toVariant().toLongLong();
        // Some Meting implementations expose seconds while NetEase style
        // payloads expose milliseconds.
        if (n > 0 && n < 10000) n *= 1000;
        return n;
    }
    const QString text = value.toString().trimmed();
    if (text.contains(QLatin1Char(':'))) {
        const QStringList parts = text.split(QLatin1Char(':'));
        if (parts.size() == 2) {
            bool okMin = false, okSec = false;
            const int minutes = parts.at(0).toInt(&okMin);
            const double seconds = parts.at(1).toDouble(&okSec);
            if (okMin && okSec) return qRound64((minutes * 60.0 + seconds) * 1000.0);
        }
    }
    bool ok = false;
    qint64 n = text.toLongLong(&ok);
    if (ok && n > 0 && n < 10000) n *= 1000;
    return ok ? n : 0;
}

QVariantMap OurcraftProvider::parseTrack(const QJsonObject &track) const
{
    QVariantMap out;
    QString id = jsonString(firstValue(track, {QStringLiteral("id"),
                                                QStringLiteral("songId"),
                                                QStringLiteral("rid"),
                                                QStringLiteral("hash")})).trimmed();
    // Meting-compatible playlist responses commonly omit the standalone id
    // and expose only an API resolver URL such as
    // /api?server=netease&type=url&id=1973665667.  Dropping those rows made
    // every Ourcraft home chart and playlist appear empty.
    if (id.isEmpty()) {
        for (const QString &candidate : {
                 track.value(QStringLiteral("url")).toString(),
                 track.value(QStringLiteral("playUrl")).toString(),
                 track.value(QStringLiteral("lrc")).toString()}) {
            id = sourceIdFromUrl(candidate);
            if (!id.isEmpty()) break;
        }
    }
    const QString title = firstNonEmpty({track.value(QStringLiteral("name")).toString(),
                                         track.value(QStringLiteral("title")).toString(),
                                         track.value(QStringLiteral("songname")).toString()});

    QString artist = artistText(firstValue(track, {QStringLiteral("artist"),
                                                    QStringLiteral("artists"),
                                                    QStringLiteral("ar"),
                                                    QStringLiteral("singer")}));
    if (artist.isEmpty()) artist = QStringLiteral("未知歌手");

    QString album;
    const QJsonValue albumValue = firstValue(track, {QStringLiteral("album"), QStringLiteral("al")});
    if (albumValue.isString()) album = albumValue.toString();
    else if (albumValue.isObject()) album = albumValue.toObject().value(QStringLiteral("name")).toString();

    QString cover = firstNonEmpty({track.value(QStringLiteral("pic")).toString(),
                                   track.value(QStringLiteral("cover")).toString(),
                                   track.value(QStringLiteral("picUrl")).toString(),
                                   track.value(QStringLiteral("albumPic")).toString(),
                                   track.value(QStringLiteral("img")).toString()});
    if (cover.isEmpty() && albumValue.isObject()) {
        const QJsonObject albumObject = albumValue.toObject();
        cover = firstNonEmpty({albumObject.value(QStringLiteral("picUrl")).toString(),
                               albumObject.value(QStringLiteral("cover")).toString()});
    }

    const qint64 duration = durationMs(firstValue(track, {QStringLiteral("duration"),
                                                           QStringLiteral("time"),
                                                           QStringLiteral("dt")}));
    const QString directUrl = firstNonEmpty({track.value(QStringLiteral("url")).toString(),
                                             track.value(QStringLiteral("playUrl")).toString()});

    out[QStringLiteral("id")] = id;
    out[QStringLiteral("sourceId")] = id;
    out[QStringLiteral("title")] = title.isEmpty() ? QStringLiteral("未知歌曲") : title;
    out[QStringLiteral("artist")] = artist;
    out[QStringLiteral("album")] = album;
    out[QStringLiteral("cover")] = cover;
    out[QStringLiteral("duration")] = duration;
    out[QStringLiteral("access")] = QStringLiteral("free");
    out[QStringLiteral("playable")] = !id.isEmpty();
    out[QStringLiteral("platform")] = m_server;
    // Search/playlist results are re-annotated by MultiSourceManager. Home
    // discovery tracks are emitted directly, so include enough provider
    // metadata for replay, lyrics and fallback resolution there as well.
    out[QStringLiteral("providerId")] = m_id;
    out[QStringLiteral("providerName")] = m_name;
    out[QStringLiteral("sourceCount")] = 1;
    out[QStringLiteral("sourceSummary")] = m_name;
    out[QStringLiteral("sources")] = QVariantList{
        QVariantMap{
            {QStringLiteral("providerId"), m_id},
            {QStringLiteral("providerName"), m_name},
            {QStringLiteral("sourceId"), id},
            {QStringLiteral("access"), QStringLiteral("free")},
            {QStringLiteral("playable"), !id.isEmpty()}
        }
    };
    // A Meting /api?type=url address is a resolver (JSON when json=1, a 302
    // otherwise), not an audio file.  Never persist it as a direct media URL.
    if (!directUrl.isEmpty() && !isOurcraftResolverUrl(directUrl))
        out[QStringLiteral("directUrl")] = directUrl;
    return out;
}

QVariantList OurcraftProvider::extractRows(const QJsonDocument &doc) const
{
    QJsonArray rows;
    if (doc.isArray()) rows = doc.array();
    else if (doc.isObject()) rows = arrayFromObject(doc.object());

    QVariantList tracks;
    tracks.reserve(rows.size());
    for (const QJsonValue &entry : rows) {
        if (!entry.isObject()) continue;
        QVariantMap track = parseTrack(entry.toObject());
        if (track.value(QStringLiteral("sourceId")).toString().isEmpty()) continue;
        tracks << track;
    }
    return tracks;
}

QNetworkReply *OurcraftProvider::enrichNeteaseTracks(
    const QVariantList &tracks,
    std::function<void(const QVariantList &)> completion,
    int timeoutMs)
{
    if (m_server != QStringLiteral("netease") || tracks.isEmpty()) {
        completion(tracks);
        return nullptr;
    }

    QStringList ids;
    ids.reserve(tracks.size());
    for (const QVariant &entry : tracks) {
        const QString id = entry.toMap()
                               .value(QStringLiteral("sourceId"))
                               .toString()
                               .trimmed();
        if (!id.isEmpty() && !ids.contains(id)) ids << id;
    }
    if (ids.isEmpty()) {
        completion(tracks);
        return nullptr;
    }

    // The compact search/playlist responses intentionally omit album, artwork
    // and duration.  The enhanced song_detail module accepts a comma-separated
    // batch and returns all three without resolving or downloading audio.
    auto failureCompletion = completion;
    return get(
        {{QStringLiteral("action"), QStringLiteral("enhanced")},
         {QStringLiteral("path"), QStringLiteral("/song_detail")},
         {QStringLiteral("ids"), ids.join(QLatin1Char(','))}},
        [this, tracks, completion = std::move(completion)](
            const QJsonDocument &doc,
            const QByteArray &) mutable {
            QJsonArray details;
            if (doc.isObject()) {
                const QJsonValue body = doc.object().value(QStringLiteral("body"));
                if (body.isObject())
                    details = body.toObject().value(QStringLiteral("songs")).toArray();
            }

            QHash<QString, QVariantMap> byId;
            for (const QJsonValue &entry : details) {
                if (!entry.isObject()) continue;
                const QVariantMap detail = parseTrack(entry.toObject());
                const QString id = detail.value(QStringLiteral("sourceId")).toString();
                if (!id.isEmpty()) byId.insert(id, detail);
            }

            QVariantList enriched;
            enriched.reserve(tracks.size());
            for (const QVariant &entry : tracks) {
                QVariantMap track = entry.toMap();
                const QVariantMap detail = byId.value(
                    track.value(QStringLiteral("sourceId")).toString());

                for (const QString &key : {
                         QStringLiteral("title"),
                         QStringLiteral("artist"),
                         QStringLiteral("album"),
                         QStringLiteral("cover")}) {
                    const QString value = detail.value(key).toString().trimmed();
                    if (!value.isEmpty()) track[key] = value;
                }
                if (detail.value(QStringLiteral("duration")).toLongLong() > 0)
                    track[QStringLiteral("duration")] =
                        detail.value(QStringLiteral("duration"));
                enriched << track;
            }
            completion(enriched);
        },
        [tracks, completion = std::move(failureCompletion)](const QString &) mutable {
            // Metadata is an enhancement. Keep the usable compact results if
            // the optional batch-detail request is temporarily unavailable.
            completion(tracks);
        },
        timeoutMs);
}

QVariantMap OurcraftProvider::parsePlaylistMeta(const QString &playlistId,
                                                 const QVariantList &tracks) const
{
    QVariantMap playlist;
    playlist[QStringLiteral("id")] = playlistId;
    playlist[QStringLiteral("providerId")] = m_id;
    playlist[QStringLiteral("name")] = QStringLiteral("%1 歌单").arg(m_name);
    playlist[QStringLiteral("description")] = QStringLiteral("由 Ourcraft Music API 加载");
    playlist[QStringLiteral("trackCount")] = tracks.size();
    if (!tracks.isEmpty()) playlist[QStringLiteral("cover")] = tracks.first().toMap().value(QStringLiteral("cover"));
    return playlist;
}

void OurcraftProvider::search(const QString &keywords, int limit)
{
    const QString q = keywords.trimmed().simplified();
    if (q.isEmpty()) { emit searchReady({}); return; }
    if (m_searchReply && m_searchReply->isRunning()) m_searchReply->abort();
    const quint64 generation = ++m_searchGeneration;

    m_searchReply = get({{QStringLiteral("server"), m_server},
                         {QStringLiteral("type"), QStringLiteral("search")},
                         {QStringLiteral("id"), q},
                         {QStringLiteral("limit"), QString::number(qBound(1, limit, 80))}},
                        [this, generation](const QJsonDocument &doc, const QByteArray &) {
        if (generation != m_searchGeneration) return;
        const QVariantList tracks = extractRows(doc);
        m_searchReply = enrichNeteaseTracks(
            tracks,
            [this, generation](const QVariantList &enriched) {
                if (generation == m_searchGeneration)
                    emit searchReady(enriched);
            });
    });
}

void OurcraftProvider::searchCatalog(const QString &keywords, int limit)
{
    if (m_server != QStringLiteral("netease")) {
        emit catalogSearchReady({}, {});
        return;
    }

    const QString q = keywords.trimmed().simplified();
    if (q.isEmpty()) {
        emit catalogSearchReady({}, {});
        return;
    }

    if (m_playlistSearchReply && m_playlistSearchReply->isRunning())
        m_playlistSearchReply->abort();
    if (m_artistSearchReply && m_artistSearchReply->isRunning())
        m_artistSearchReply->abort();

    const quint64 generation = ++m_catalogSearchGeneration;
    auto playlists = std::make_shared<QVariantList>();
    auto artists = std::make_shared<QVariantList>();
    auto remaining = std::make_shared<int>(2);
    auto finish = [this, generation, playlists, artists, remaining] {
        if (--(*remaining) == 0 && generation == m_catalogSearchGeneration)
            emit catalogSearchReady(*playlists, *artists);
    };

    const QString boundedLimit = QString::number(qBound(1, limit, 30));
    m_playlistSearchReply = get(
        {{QStringLiteral("action"), QStringLiteral("enhanced")},
         {QStringLiteral("path"), QStringLiteral("/search")},
         {QStringLiteral("keywords"), q},
         {QStringLiteral("type"), QStringLiteral("1000")},
         {QStringLiteral("limit"), boundedLimit}},
        [generation, playlists, finish](const QJsonDocument &doc, const QByteArray &) {
            if (!doc.isObject()) { finish(); return; }
            const QJsonArray rows = doc.object().value(QStringLiteral("body")).toObject()
                                        .value(QStringLiteral("result")).toObject()
                                        .value(QStringLiteral("playlists")).toArray();
            for (const QJsonValue &entry : rows) {
                const QJsonObject row = entry.toObject();
                const QJsonObject creator = row.value(QStringLiteral("creator")).toObject();
                playlists->append(QVariantMap{
                    {QStringLiteral("id"), jsonString(row.value(QStringLiteral("id")))},
                    {QStringLiteral("sourceId"), jsonString(row.value(QStringLiteral("id")))},
                    {QStringLiteral("providerId"), QStringLiteral("netease")},
                    {QStringLiteral("name"), row.value(QStringLiteral("name")).toString()},
                    {QStringLiteral("cover"), row.value(QStringLiteral("coverImgUrl")).toString()},
                    {QStringLiteral("ownerName"), creator.value(QStringLiteral("nickname")).toString()},
                    {QStringLiteral("trackCount"), row.value(QStringLiteral("trackCount")).toInt()},
                    {QStringLiteral("playCount"), row.value(QStringLiteral("playCount")).toVariant().toLongLong()}
                });
            }
            Q_UNUSED(generation)
            finish();
        },
        [finish](const QString &) { finish(); }, 9000);

    m_artistSearchReply = get(
        {{QStringLiteral("action"), QStringLiteral("enhanced")},
         {QStringLiteral("path"), QStringLiteral("/search")},
         {QStringLiteral("keywords"), q},
         {QStringLiteral("type"), QStringLiteral("100")},
         {QStringLiteral("limit"), boundedLimit}},
        [generation, artists, finish](const QJsonDocument &doc, const QByteArray &) {
            if (!doc.isObject()) { finish(); return; }
            const QJsonArray rows = doc.object().value(QStringLiteral("body")).toObject()
                                        .value(QStringLiteral("result")).toObject()
                                        .value(QStringLiteral("artists")).toArray();
            for (const QJsonValue &entry : rows) {
                const QJsonObject row = entry.toObject();
                QStringList aliases;
                for (const QJsonValue &alias : row.value(QStringLiteral("alias")).toArray())
                    if (!alias.toString().trimmed().isEmpty()) aliases << alias.toString();
                artists->append(QVariantMap{
                    {QStringLiteral("id"), jsonString(row.value(QStringLiteral("id")))},
                    {QStringLiteral("providerId"), QStringLiteral("netease")},
                    {QStringLiteral("name"), row.value(QStringLiteral("name")).toString()},
                    {QStringLiteral("cover"), firstNonEmpty({row.value(QStringLiteral("picUrl")).toString(),
                                                             row.value(QStringLiteral("img1v1Url")).toString()})},
                    {QStringLiteral("aliases"), aliases},
                    {QStringLiteral("albumCount"), row.value(QStringLiteral("albumSize")).toInt()},
                    {QStringLiteral("trackCount"), row.value(QStringLiteral("musicSize")).toInt()},
                    {QStringLiteral("followers"), row.value(QStringLiteral("fansSize")).toVariant().toLongLong()}
                });
            }
            Q_UNUSED(generation)
            finish();
        },
        [finish](const QString &) { finish(); }, 9000);
}

void OurcraftProvider::loadHome(int limit)
{
    if (m_server != QStringLiteral("netease")) {
        emit homeReady({});
        return;
    }

    const QVariantList fallbackPlaylists{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("3778678")},
                    {QStringLiteral("providerId"), m_id},
                    {QStringLiteral("name"), QStringLiteral("网易云热歌榜")},
                    {QStringLiteral("description"), QStringLiteral("Ourcraft Music API · 热门歌曲")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("19723756")},
                    {QStringLiteral("providerId"), m_id},
                    {QStringLiteral("name"), QStringLiteral("网易云飙升榜")},
                    {QStringLiteral("description"), QStringLiteral("Ourcraft Music API · 热度上升")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("3779629")},
                    {QStringLiteral("providerId"), m_id},
                    {QStringLiteral("name"), QStringLiteral("网易云新歌榜")},
                    {QStringLiteral("description"), QStringLiteral("Ourcraft Music API · 新歌")}}
    };

    // Discovery must come from the configured aggregate API, not from Evolve
    // Cloud's community catalog. The enhanced NetEase playlist search gives us
    // real covers, creators and enough cards for a multi-row shelf. Keep the
    // three stable charts only as an offline/upstream-error fallback.
    if (m_homePlaylistReply && m_homePlaylistReply->isRunning())
        m_homePlaylistReply->abort();
    const quint64 generation = ++m_homeGeneration;
    m_homePlaylistReply = get(
        {{QStringLiteral("action"), QStringLiteral("enhanced")},
         {QStringLiteral("path"), QStringLiteral("/search")},
         {QStringLiteral("keywords"), QStringLiteral("华语流行")},
         {QStringLiteral("type"), QStringLiteral("1000")},
         {QStringLiteral("limit"), QString::number(qBound(12, limit, 24))}},
        [this, generation, fallbackPlaylists](const QJsonDocument &doc, const QByteArray &) {
            if (generation != m_homeGeneration)
                return;
            QVariantList playlists;
            if (doc.isObject()) {
                const QJsonArray rows = doc.object().value(QStringLiteral("body")).toObject()
                                            .value(QStringLiteral("result")).toObject()
                                            .value(QStringLiteral("playlists")).toArray();
                playlists.reserve(rows.size());
                for (const QJsonValue &entry : rows) {
                    const QJsonObject row = entry.toObject();
                    const QString id = jsonString(row.value(QStringLiteral("id")));
                    if (id.isEmpty())
                        continue;
                    const QJsonObject creator = row.value(QStringLiteral("creator")).toObject();
                    playlists.append(QVariantMap{
                        {QStringLiteral("id"), id},
                        {QStringLiteral("sourceId"), id},
                        {QStringLiteral("providerId"), m_id},
                        {QStringLiteral("name"), row.value(QStringLiteral("name")).toString()},
                        {QStringLiteral("cover"), row.value(QStringLiteral("coverImgUrl")).toString()},
                        {QStringLiteral("ownerName"), creator.value(QStringLiteral("nickname")).toString()},
                        {QStringLiteral("description"), QStringLiteral("Ourcraft Music API · 网易云歌单")},
                        {QStringLiteral("trackCount"), row.value(QStringLiteral("trackCount")).toInt()},
                        {QStringLiteral("playCount"), row.value(QStringLiteral("playCount")).toVariant().toLongLong()}
                    });
                }
            }
            emit homeReady(playlists.isEmpty() ? fallbackPlaylists : playlists);
        },
        [this, generation, fallbackPlaylists](const QString &) {
            if (generation == m_homeGeneration)
                emit homeReady(fallbackPlaylists);
        }, 9000);

    get({{QStringLiteral("server"), m_server},
         {QStringLiteral("type"), QStringLiteral("playlist")},
         {QStringLiteral("id"), QStringLiteral("3778678")},
         {QStringLiteral("limit"), QString::number(qBound(12, limit * 2, 32))}},
        [this](const QJsonDocument &doc, const QByteArray &) {
            const QVariantList tracks = extractRows(doc);
            enrichNeteaseTracks(
                tracks,
                [this](const QVariantList &enriched) {
                    emit discoveryTracksReady(enriched);
                });
        }, {}, 9000);
}

void OurcraftProvider::loadPlaylist(const QString &playlistId, int limit)
{
    const QString id = playlistId.trimmed();
    if (id.isEmpty()) { emit errorOccurred(QStringLiteral("歌单 ID 为空")); return; }
    const quint64 generation = ++m_playlistGeneration;
    get({{QStringLiteral("server"), m_server},
         {QStringLiteral("type"), QStringLiteral("playlist")},
         {QStringLiteral("id"), id},
         {QStringLiteral("limit"), QString::number(qBound(1, limit, 500))}},
        [this, id, generation](const QJsonDocument &doc, const QByteArray &) {
            if (generation != m_playlistGeneration) return;
            const QVariantList tracks = extractRows(doc);
            enrichNeteaseTracks(
                tracks,
                [this, id, generation](const QVariantList &enriched) {
                    if (generation != m_playlistGeneration) return;
                    emit playlistReady(parsePlaylistMeta(id, enriched), enriched);
                },
                12000);
        }, {}, 10000);
}

QString OurcraftProvider::extractUrl(const QJsonDocument &doc, const QByteArray &raw) const
{
    std::function<QString(const QJsonValue &, int)> visit =
        [&](const QJsonValue &value, int depth) -> QString {
        if (depth > 4 || value.isNull() || value.isUndefined())
            return {};

        if (value.isString()) {
            const QString text = value.toString().trimmed();
            if (text.startsWith(QStringLiteral("http://"))
                || text.startsWith(QStringLiteral("https://"))) {
                return text;
            }
            return {};
        }

        if (value.isArray()) {
            const QJsonArray rows = value.toArray();
            for (const QJsonValue &entry : rows) {
                const QString url = visit(entry, depth + 1);
                if (!url.isEmpty()) return url;
            }
            return {};
        }

        if (!value.isObject())
            return {};

        const QJsonObject object = value.toObject();
        const QString direct = firstNonEmpty({
            object.value(QStringLiteral("url")).toString(),
            object.value(QStringLiteral("playUrl")).toString(),
            object.value(QStringLiteral("location")).toString(),
            object.value(QStringLiteral("src")).toString()
        });
        if (direct.startsWith(QStringLiteral("http://"))
            || direct.startsWith(QStringLiteral("https://"))) {
            return direct;
        }

        for (const QString &key : {
                 QStringLiteral("data"), QStringLiteral("result"),
                 QStringLiteral("song"), QStringLiteral("item"),
                 QStringLiteral("items")}) {
            if (!object.contains(key)) continue;
            const QString nested = visit(object.value(key), depth + 1);
            if (!nested.isEmpty()) return nested;
        }
        return {};
    };

    if (doc.isObject()) {
        const QString url = visit(QJsonValue(doc.object()), 0);
        if (!url.isEmpty()) return url;
    } else if (doc.isArray()) {
        const QString url = visit(QJsonValue(doc.array()), 0);
        if (!url.isEmpty()) return url;
    }

    const QString plain = QString::fromUtf8(raw).trimmed();
    if (plain.startsWith(QStringLiteral("http://"))
        || plain.startsWith(QStringLiteral("https://"))) {
        return plain;
    }
    return {};
}

void OurcraftProvider::resolveSongUrl(const QString &songId, const QString &)
{
    const QString id = songId.trimmed();
    if (id.isEmpty()) { emit errorOccurred(QStringLiteral("歌曲 ID 为空")); return; }
    if (m_streamReply && m_streamReply->isRunning()) m_streamReply->abort();
    m_streamReply = get({{QStringLiteral("server"), m_server},
                         {QStringLiteral("type"), QStringLiteral("url")},
                         {QStringLiteral("id"), id},
                         {QStringLiteral("json"), QStringLiteral("1")}},
                        [this, id](const QJsonDocument &doc, const QByteArray &raw) {
        const QString url = extractUrl(doc, raw);
        if (url.isEmpty()) {
            emit errorOccurred(QStringLiteral("%1 没有返回可播放地址（可能为会员/无版权歌曲）").arg(m_name));
            return;
        }
        emit streamUrlReady(id, url);
    }, {}, 8000);
}

void OurcraftProvider::resolveDownloadUrl(const QString &songId, const QString &level)
{
    Q_UNUSED(level)
    const QString id = songId.trimmed();
    if (id.isEmpty()) { emit errorOccurred(QStringLiteral("歌曲 ID 为空")); return; }
    get({{QStringLiteral("server"), m_server},
         {QStringLiteral("type"), QStringLiteral("url")},
         {QStringLiteral("id"), id},
         {QStringLiteral("json"), QStringLiteral("1")}},
        [this, id](const QJsonDocument &doc, const QByteArray &raw) {
            const QString url = extractUrl(doc, raw);
            if (url.isEmpty()) {
                emit errorOccurred(QStringLiteral("%1 没有返回可下载的免费音频地址").arg(m_name));
                return;
            }
            emit downloadUrlReady(id, url);
        }, {}, 8000);
}

void OurcraftProvider::resolveTrack(const QVariantMap &track, const QString &level)
{
    Q_UNUSED(level)

    const QString canonicalId = track.value(QStringLiteral("id")).toString().trimmed();
    const QString title = track.value(QStringLiteral("title")).toString().trimmed();
    const QString artist = track.value(QStringLiteral("artist")).toString().trimmed();
    const qint64 wantedDuration = track.value(QStringLiteral("duration")).toLongLong();
    if (canonicalId.isEmpty() || title.isEmpty()) {
        emit errorOccurred(QStringLiteral("%1 无法按歌曲信息查找备用来源").arg(m_name));
        return;
    }

    if (m_lookupReply && m_lookupReply->isRunning())
        m_lookupReply->abort();

    const QString keywords = artist.isEmpty()
        ? title
        : title + QStringLiteral(" ") + artist;

    m_lookupReply = get(
        {{QStringLiteral("server"), m_server},
         {QStringLiteral("type"), QStringLiteral("search")},
         {QStringLiteral("id"), keywords},
         {QStringLiteral("limit"), QStringLiteral("8")}},
        [this, canonicalId, title, artist, wantedDuration](const QJsonDocument &doc, const QByteArray &) {
            const QVariantList rows = extractRows(doc);
            if (rows.isEmpty()) {
                emit errorOccurred(QStringLiteral("%1 没找到同名备用歌曲").arg(m_name));
                return;
            }

            auto normalizeText = [](QString value) {
                value = value.toCaseFolded().simplified();
                value.remove(QRegularExpression(QStringLiteral("[\\s\\-_/·•.,，。()（）\\[\\]【】]+")));
                return value;
            };

            const QString wantedTitle = normalizeText(title);
            const QString wantedArtist = normalizeText(artist);
            QVariantMap best;
            int bestScore = -1;

            for (const QVariant &entry : rows) {
                const QVariantMap candidate = entry.toMap();
                const QString candidateTitle =
                    normalizeText(candidate.value(QStringLiteral("title")).toString());
                const QString candidateArtist =
                    normalizeText(candidate.value(QStringLiteral("artist")).toString());

                const bool artistCompatible = wantedArtist.isEmpty()
                    || candidateArtist == wantedArtist
                    || (!candidateArtist.isEmpty()
                        && (candidateArtist.contains(wantedArtist)
                            || wantedArtist.contains(candidateArtist)));
                if (!artistCompatible)
                    continue;

                const qint64 candidateDuration =
                    candidate.value(QStringLiteral("duration")).toLongLong();
                if (wantedDuration > 0 && candidateDuration > 0
                    && qAbs(wantedDuration - candidateDuration) > 3500) {
                    continue;
                }

                int score = 0;
                if (candidateTitle == wantedTitle) score += 100;
                else if (!wantedTitle.isEmpty()
                         && (candidateTitle.contains(wantedTitle)
                             || wantedTitle.contains(candidateTitle))) {
                    score += 55;
                }

                if (wantedArtist.isEmpty()) score += 10;
                else if (candidateArtist == wantedArtist) score += 45;
                else if (!candidateArtist.isEmpty()
                         && (candidateArtist.contains(wantedArtist)
                             || wantedArtist.contains(candidateArtist))) {
                    score += 25;
                }

                if (candidate.value(QStringLiteral("playable"), true).toBool())
                    score += 5;

                if (score > bestScore) {
                    bestScore = score;
                    best = candidate;
                }
            }

            const QString sourceId =
                best.value(QStringLiteral("sourceId")).toString().trimmed();
            if (sourceId.isEmpty() || bestScore < (wantedArtist.isEmpty() ? 95 : 100)) {
                emit errorOccurred(QStringLiteral("%1 没找到足够匹配的备用歌曲").arg(m_name));
                return;
            }

            m_lookupReply = get(
                {{QStringLiteral("server"), m_server},
                 {QStringLiteral("type"), QStringLiteral("url")},
                 {QStringLiteral("id"), sourceId},
                 {QStringLiteral("json"), QStringLiteral("1")}},
                [this, canonicalId](const QJsonDocument &urlDoc, const QByteArray &raw) {
                    const QString url = extractUrl(urlDoc, raw);
                    if (url.isEmpty()) {
                        emit errorOccurred(QStringLiteral("%1 的备用歌曲没有可播放地址").arg(m_name));
                        return;
                    }

                    // MultiSourceManager correlates on-demand lookups with the
                    // canonical Evolve track id rather than the platform id.
                    emit streamUrlReady(canonicalId, url);
                },
                [this](const QString &message) { emit errorOccurred(message); },
                8000);
        },
        [this](const QString &message) { emit errorOccurred(message); },
        8000);
}

QString OurcraftProvider::extractLyrics(const QJsonDocument &doc, const QByteArray &raw) const
{
    if (doc.isObject()) {
        const QJsonObject root = doc.object();
        const QString direct = firstNonEmpty({root.value(QStringLiteral("lyric")).toString(),
                                              root.value(QStringLiteral("lrc")).toString(),
                                              root.value(QStringLiteral("lyrics")).toString()});
        if (!direct.isEmpty()) return direct;
        const QJsonValue data = root.value(QStringLiteral("data"));
        if (data.isString()) return data.toString();
        if (data.isObject()) {
            const QJsonObject object = data.toObject();
            const QString nested = firstNonEmpty({object.value(QStringLiteral("lyric")).toString(),
                                                  object.value(QStringLiteral("lrc")).toString()});
            if (!nested.isEmpty()) return nested;
        }
    }
    if (doc.isArray() && !doc.array().isEmpty()) {
        const QJsonValue first = doc.array().first();
        if (first.isString()) return first.toString();
        if (first.isObject()) {
            const QJsonObject object = first.toObject();
            const QString nested = firstNonEmpty({object.value(QStringLiteral("lyric")).toString(),
                                                  object.value(QStringLiteral("lrc")).toString()});
            if (!nested.isEmpty()) return nested;
        }
    }
    return QString::fromUtf8(raw).trimmed();
}

void OurcraftProvider::loadLyrics(const QString &songId)
{
    const QString id = songId.trimmed();
    if (id.isEmpty()) return;
    if (m_lyricsReply && m_lyricsReply->isRunning()) m_lyricsReply->abort();
    m_lyricsReply = get({{QStringLiteral("server"), m_server},
                         {QStringLiteral("type"), QStringLiteral("lrc")},
                         {QStringLiteral("id"), id}},
                        [this, id](const QJsonDocument &doc, const QByteArray &raw) {
        emit lyricsReady(id, extractLyrics(doc, raw), QString());
    }, {}, 7000);
}

void OurcraftProvider::ping()
{
    get({{QStringLiteral("action"), QStringLiteral("health")}},
        [this](const QJsonDocument &doc, const QByteArray &) {
            if (!doc.isObject()) {
                emit pingReady(false, QStringLiteral("Ourcraft Music API 健康检查返回格式异常"));
                return;
            }
            const QJsonObject root = doc.object();
            const bool ok = root.value(QStringLiteral("ok")).toBool(false);
            QString version = root.value(QStringLiteral("version")).toString();
            if (version.isEmpty()) version = QStringLiteral("unknown");
            const QJsonObject provider = root.value(QStringLiteral("provider")).toObject();
            const bool hasCookie = root.value(QStringLiteral("hasCookie"))
                                       .toBool(provider.value(QStringLiteral("hasCookie")).toBool(false));
            QString level = root.value(QStringLiteral("ncmLevel")).toString().trimmed();
            if (level.isEmpty())
                level = provider.value(QStringLiteral("ncmLevel")).toString().trimmed();
            bool supported = true;
            const QJsonArray servers = root.value(QStringLiteral("supportedServers")).toArray();
            if (!servers.isEmpty()) {
                supported = false;
                for (const QJsonValue &entry : servers) {
                    if (entry.toString() == m_server) { supported = true; break; }
                }
            }
            emit pingReady(ok && supported,
                           ok && supported
                               ? (m_server == QStringLiteral("netease") && hasCookie
                                      ? QStringLiteral("Ourcraft API v%1 · 网易会员能力已接入 · %2")
                                            .arg(version, level.isEmpty() ? QStringLiteral("默认音质") : level)
                                      : QStringLiteral("Ourcraft API v%1 · %2 可用").arg(version, m_server))
                               : QStringLiteral("Ourcraft API 未启用 %1").arg(m_server));
        },
        [this](const QString &message) { emit pingReady(false, message); }, 5000);
}
