#include "TwoT58Provider.h"
#include "HttpFallbackFetcher.h"
#include "PublicSiteIndex.h"
#include "WebProviderUtils.h"

#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QSharedPointer>
#include <QTimer>
#include <QUrlQuery>
#include <utility>

namespace {

QStringList splitArtistTitle(const QString &text)
{
    QString normalized = WebProviderUtils::stripTags(text);
    normalized.replace(QChar(0x2013), QLatin1Char('-'));
    normalized.replace(QChar(0x2014), QLatin1Char('-'));

    int pos = normalized.indexOf(QStringLiteral(" - "));
    int sepLen = 3;
    if (pos < 0) {
        pos = normalized.indexOf(QLatin1Char('-'));
        sepLen = 1;
    }

    if (pos <= 0 || pos >= normalized.size() - sepLen)
        return {QString(), normalized.trimmed()};

    return {
        normalized.left(pos).trimmed(),
        normalized.mid(pos + sepLen).trimmed()
    };
}

QString responseContentType(QNetworkReply *reply)
{
    return reply->header(QNetworkRequest::ContentTypeHeader)
        .toString()
        .section(QLatin1Char(';'), 0, 0)
        .trimmed()
        .toLower();
}

QString normalizeEscapedHtml(QString html)
{
    html.replace(QStringLiteral("\\/"), QStringLiteral("/"));
    html.replace(QStringLiteral("\\u002F"), QStringLiteral("/"), Qt::CaseInsensitive);
    html.replace(QStringLiteral("\\x2F"), QStringLiteral("/"), Qt::CaseInsensitive);
    html.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    return html;
}

bool looksLikeResolverEndpoint(const QString &url)
{
    const QUrl parsed(url);
    const QString lower =
        (parsed.path() + QLatin1Char('?') + parsed.query()).toLower();

    if (lower.contains(QStringLiteral("login"))
        || lower.contains(QStringLiteral("register")))
        return false;

    // Do not use the password-oriented download route as a playback bypass.
    if (lower.contains(QStringLiteral("down.php"))
        && (lower.contains(QStringLiteral("lk="))
            || lower.contains(QStringLiteral("password"))
            || lower.contains(QStringLiteral("pwd"))))
        return false;

    return lower.contains(QStringLiteral("play"))
        || lower.contains(QStringLiteral("player"))
        || lower.contains(QStringLiteral("ajax"))
        || lower.contains(QStringLiteral("/api/"))
        || lower.contains(QStringLiteral("music"))
        || lower.contains(QStringLiteral("audio"));
}

}

TwoT58Provider::TwoT58Provider(QObject *parent)
    : IMusicProvider(parent)
{
    m_network.setCookieJar(new QNetworkCookieJar(&m_network));

    // Prime the site's server-side session cookie early. If the site requires
    // an additional browser-generated anti-bot cookie, the public index
    // fallback still allows discovery without blocking the app.
    QTimer::singleShot(0, this, [this] {
        getText(
            QUrl(m_baseUrl + QStringLiteral("/")),
            [](const QString &, const QUrl &) {},
            [](const QString &) {},
            QString(),
            1800);
    });
}

QString TwoT58Provider::searchCookie() const
{
    const QByteArray env = qgetenv("EVOLVE_2T58_COOKIE");
    if (!env.trimmed().isEmpty())
        return QString::fromUtf8(env.trimmed());

    const QString cookiePath =
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("runtime/client-data/2t58-cookie.txt"));

    QFile cookieFile(cookiePath);
    if (cookieFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString fromFile =
            QString::fromUtf8(cookieFile.readAll()).trimmed();
        if (!fromFile.isEmpty())
            return fromFile;
    }

    // Compatibility seed mirrored from the current maintained musicdl
    // TwoT58MusicClient search headers. This is a site/session compatibility
    // cookie, not a user account credential. It may rotate in the future;
    // the env/file override above avoids requiring a rebuild when that happens.
    return QStringLiteral(
        "Hm_tf_hx9umupwu8o=1766942296; "
        "9be49c0fcbd87e6a36f944af3f638e63=701e0362d41fe970a431ab4e7e0a8260; "
        "server_name_session=8e658a40df8491e40010dc3307caacde; "
        "Hm_lvt_hx9umupwu8o=1775811750,1776490879; "
        "Hm_lpvt_hx9umupwu8o=1776492031");
}

QMap<QString, QString> TwoT58Provider::searchCompatHeaders() const
{
    return {
        {QStringLiteral("Accept"),
         QStringLiteral("text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7")},
        {QStringLiteral("Accept-Language"),
         QStringLiteral("zh-CN,zh;q=0.9,en-US;q=0.8,en;q=0.7")},
        {QStringLiteral("Cookie"), searchCookie()},
        {QStringLiteral("Sec-CH-UA"),
         QStringLiteral("\"Google Chrome\";v=\"147\", \"Not.A/Brand\";v=\"8\", \"Chromium\";v=\"147\"")},
        {QStringLiteral("Sec-CH-UA-Mobile"), QStringLiteral("?0")},
        {QStringLiteral("Sec-CH-UA-Platform"), QStringLiteral("\"Windows\"")},
        {QStringLiteral("Sec-Fetch-Dest"), QStringLiteral("document")},
        {QStringLiteral("Sec-Fetch-Mode"), QStringLiteral("navigate")},
        {QStringLiteral("Sec-Fetch-Site"), QStringLiteral("same-origin")},
        {QStringLiteral("Sec-Fetch-User"), QStringLiteral("?1")},
        {QStringLiteral("Upgrade-Insecure-Requests"), QStringLiteral("1")},
        {QStringLiteral("User-Agent"),
         QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/147.0.0.0 Safari/537.36")}
    };
}

void TwoT58Provider::setBaseUrl(const QString &url)
{
    QString normalized = url.trimmed();
    while (normalized.endsWith(QLatin1Char('/')))
        normalized.chop(1);
    if (!normalized.isEmpty())
        m_baseUrl = normalized;
    m_searchCache.clear();
}

QNetworkReply *TwoT58Provider::getText(const QUrl &url,
                                       TextCallback success,
                                       std::function<void(const QString &)> failure,
                                       const QString &referer,
                                       int timeoutMs)
{
    QNetworkRequest request{url};

    const QMap<QString, QString> compat = searchCompatHeaders();
    for (auto it = compat.constBegin(); it != compat.constEnd(); ++it)
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("Pragma", "no-cache");
    if (!referer.isEmpty())
        request.setRawHeader("Referer", referer.toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(timeoutMs);

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this,
            [reply, success = std::move(success), failure = std::move(failure)] {
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            reply->deleteLater();
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            const QString message = reply->errorString();
            reply->deleteLater();
            if (failure)
                failure(message);
            return;
        }

        const QByteArray body = reply->readAll();
        const QString page =
            WebProviderUtils::decodePageBytes(
                body,
                reply->header(QNetworkRequest::ContentTypeHeader).toString());

        const QUrl finalUrl = reply->url();
        reply->deleteLater();
        success(page, finalUrl);
    });

    return reply;
}

QVariantList TwoT58Provider::parseSearchPage(const QString &rawHtml,
                                             int limit,
                                             const QString &fallbackTitle) const
{
    const QString html = normalizeEscapedHtml(rawHtml);
    QVariantList tracks;
    QSet<QString> seen;

    auto addTrack = [&](const QString &sourceId,
                        QString artist,
                        QString title) {
        if (sourceId.isEmpty() || seen.contains(sourceId)
            || tracks.size() >= limit)
            return;

        artist = WebProviderUtils::stripTags(artist).trimmed();
        title = WebProviderUtils::stripTags(title).trimmed();
        if (title.isEmpty())
            title = fallbackTitle.trimmed();

        const QString lower = title.toCaseFolded();
        if (title.isEmpty()
            || lower == QStringLiteral("播放")
            || lower == QStringLiteral("刷新")
            || lower == QStringLiteral("查看")
            || lower.contains(QStringLiteral("下载")))
            return;

        seen.insert(sourceId);
        tracks << QVariantMap{
            {QStringLiteral("id"), sourceId},
            {QStringLiteral("sourceId"), sourceId},
            {QStringLiteral("title"), title},
            {QStringLiteral("artist"), artist},
            {QStringLiteral("album"), QString()},
            {QStringLiteral("cover"), QString()},
            {QStringLiteral("duration"), 0},
            {QStringLiteral("access"), QStringLiteral("free")},
            {QStringLiteral("playable"), true}
        };
    };

    const QRegularExpression anchorTag(
        QStringLiteral(R"(<a\b([^>]*)>([\s\S]*?)</a>)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression hrefRe(
        QStringLiteral(R"(\bhref\s*=\s*["']([^"']+)["'])"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression titleRe(
        QStringLiteral(R"(\b(?:title|aria-label|data-title)\s*=\s*["']([^"']+)["'])"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression songPath(
        QStringLiteral(R"((?:^|/)song/([A-Za-z0-9_-]+)(?:\.html)?(?:[/?#]|$))"),
        QRegularExpression::CaseInsensitiveOption);

    auto anchors = anchorTag.globalMatch(html);
    while (anchors.hasNext() && tracks.size() < limit) {
        const auto anchor = anchors.next();
        const auto href = hrefRe.match(anchor.captured(1));
        if (!href.hasMatch())
            continue;

        const auto id = songPath.match(href.captured(1));
        if (!id.hasMatch())
            continue;

        QString label = WebProviderUtils::stripTags(anchor.captured(2));
        if (label.size() < 2) {
            const auto t = titleRe.match(anchor.captured(1));
            if (t.hasMatch())
                label = WebProviderUtils::decodeHtml(t.captured(1));
        }

        const QStringList parts = splitArtistTitle(label);
        addTrack(id.captured(1), parts.value(0), parts.value(1, label));
    }

    // Fallback for script/JSON-rendered result pages.
    const QRegularExpression everySong(
        QStringLiteral(R"((?:https?:)?//[^"'<>\\\s]*/song/([A-Za-z0-9_-]+)(?:\.html)?|/song/([A-Za-z0-9_-]+)(?:\.html)?)"),
        QRegularExpression::CaseInsensitiveOption);

    auto songs = everySong.globalMatch(html);
    while (songs.hasNext() && tracks.size() < limit) {
        const auto m = songs.next();
        const QString sourceId =
            !m.captured(1).isEmpty() ? m.captured(1) : m.captured(2);

        if (sourceId.isEmpty() || seen.contains(sourceId))
            continue;

        const qsizetype start = qMax<qsizetype>(0, m.capturedStart() - 450);
        const qsizetype length =
            qMin<qsizetype>(950, html.size() - start);
        const QString window = html.mid(start, length);

        QString title;
        QString artist;

        const QRegularExpression labelJson(
            QStringLiteral(R"(["'](?:title|name|song|songName)["']\s*[:=]\s*["']([^"']{1,180})["'])"),
            QRegularExpression::CaseInsensitiveOption);
        const auto lm = labelJson.match(window);
        if (lm.hasMatch())
            title = WebProviderUtils::decodeHtml(lm.captured(1));

        const QRegularExpression artistJson(
            QStringLiteral(R"(["'](?:artist|singer|author)["']\s*[:=]\s*["']([^"']{1,120})["'])"),
            QRegularExpression::CaseInsensitiveOption);
        const auto am = artistJson.match(window);
        if (am.hasMatch())
            artist = WebProviderUtils::decodeHtml(am.captured(1));

        if (title.isEmpty()) {
            const QString visible = WebProviderUtils::stripTags(window);
            const int dash = visible.indexOf(QStringLiteral(" - "));
            if (dash > 0 && dash < visible.size() - 3) {
                const QString maybeArtist = visible.left(dash).right(80).trimmed();
                const QString maybeTitle = visible.mid(dash + 3).left(140).trimmed();
                if (maybeTitle.contains(fallbackTitle, Qt::CaseInsensitive)
                    || fallbackTitle.contains(maybeTitle, Qt::CaseInsensitive)) {
                    artist = maybeArtist;
                    title = maybeTitle;
                }
            }
        }

        addTrack(sourceId, artist, title);
    }

    return tracks;
}

QVariantList TwoT58Provider::parseIndexRows(const QVariantList &rows,
                                            const QString &keywords,
                                            int limit) const
{
    QVariantList tracks;
    QSet<QString> seen;

    const QRegularExpression titlePattern(
        QStringLiteral(
            R"(^(.+?)\s*[-–—]\s*(.+?)(?:MP3|LRC|免费下载|在线试听|$))"),
        QRegularExpression::CaseInsensitiveOption);

    for (const QVariant &value : rows) {
        if (tracks.size() >= limit)
            break;

        const QVariantMap row = value.toMap();
        const QString sourceId =
            row.value(QStringLiteral("sourceId")).toString();

        if (sourceId.isEmpty() || seen.contains(sourceId))
            continue;

        QString rawTitle =
            WebProviderUtils::stripTags(
                row.value(QStringLiteral("title")).toString());

        rawTitle.remove(
            QRegularExpression(
                QStringLiteral(R"(\s*[-\|]\s*爱听音乐网.*$)"),
                QRegularExpression::CaseInsensitiveOption));

        QString artist;
        QString title;

        const auto match = titlePattern.match(rawTitle);
        if (match.hasMatch()) {
            artist = match.captured(1).trimmed();
            title = match.captured(2).trimmed();
        }

        if (title.isEmpty())
            title = keywords.trimmed().simplified();

        const QString wanted =
            WebProviderUtils::normalizeText(keywords);
        const QString candidate =
            WebProviderUtils::normalizeText(title + rawTitle);

        if (!wanted.isEmpty()
            && !candidate.contains(wanted)
            && !wanted.contains(WebProviderUtils::normalizeText(title)))
            continue;

        seen.insert(sourceId);
        tracks << QVariantMap{
            {QStringLiteral("id"), sourceId},
            {QStringLiteral("sourceId"), sourceId},
            {QStringLiteral("title"), title},
            {QStringLiteral("artist"), artist},
            {QStringLiteral("album"), QString()},
            {QStringLiteral("cover"), QString()},
            {QStringLiteral("duration"), 0},
            {QStringLiteral("access"), QStringLiteral("free")},
            {QStringLiteral("playable"), true}
        };
    }

    return tracks;
}

void TwoT58Provider::fetchSearch(const QString &keywords,
                                 int limit,
                                 TracksCallback callback,
                                 bool allowCache)
{
    const QString key = keywords.trimmed().simplified();
    if (key.isEmpty()) {
        callback({});
        return;
    }

    const QString cacheKey = WebProviderUtils::normalizeText(key);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (allowCache && m_searchCache.contains(cacheKey)) {
        const CachedSearch cached = m_searchCache.value(cacheKey);
        if (now - cached.timestampMs < 5 * 60 * 1000) {
            callback(cached.rows.mid(0, limit));
            return;
        }
    }

    const QString encoded =
        QString::fromLatin1(QUrl::toPercentEncoding(key));

    const QUrl www(
        m_baseUrl + QStringLiteral("/so/")
        + encoded + QStringLiteral(".html"));

    // music.2t58.com has historically been HTTP-only on some deployments.
    const QUrl mirror(
        QStringLiteral("http://music.2t58.com/so/")
        + encoded + QStringLiteral(".html"));

    struct State {
        int pending = 5;
        bool finished = false;
    };
    auto state = QSharedPointer<State>::create();

    auto accept =
        [this, state, callback, cacheKey, limit]
        (const QVariantList &rows) {
            if (state->finished)
                return;

            if (!rows.isEmpty()) {
                state->finished = true;
                m_searchCache.insert(
                    cacheKey,
                    CachedSearch{
                        QDateTime::currentMSecsSinceEpoch(),
                        rows
                    });
                callback(rows.mid(0, limit));
                return;
            }

            --state->pending;
            if (state->pending <= 0 && !state->finished) {
                state->finished = true;
                callback({});
            }
        };

    // Same search route used by the maintained 2t58 client implementation.
    getText(
        www,
        [this, key, limit, accept]
        (const QString &html, const QUrl &) {
            accept(parseSearchPage(html, limit, key));
        },
        [accept](const QString &) {
            accept({});
        },
        m_baseUrl,
        1800);

    HttpFallbackFetcher::fetch(
        this,
        www,
        [this, key, limit, accept]
        (const QString &html, const QUrl &) {
            accept(parseSearchPage(html, limit, key));
        },
        [accept](const QString &) {
            accept({});
        },
        m_baseUrl,
        2600,
        searchCompatHeaders());

    getText(
        mirror,
        [this, key, limit, accept]
        (const QString &html, const QUrl &) {
            accept(parseSearchPage(html, limit, key));
        },
        [accept](const QString &) {
            accept({});
        },
        QStringLiteral("http://music.2t58.com/"),
        1800);

    HttpFallbackFetcher::fetch(
        this,
        mirror,
        [this, key, limit, accept]
        (const QString &html, const QUrl &) {
            accept(parseSearchPage(html, limit, key));
        },
        [accept](const QString &) {
            accept({});
        },
        QStringLiteral("http://music.2t58.com/"),
        2600,
        searchCompatHeaders());

    // If the site's anti-bot/session cookie hides .play_list, discover the
    // public /song/... page through the search index instead.
    PublicSiteIndex::search(
        this,
        QStringLiteral("2t58.com"),
        QStringLiteral("/song/"),
        key,
        qMin(limit, 12),
        [this, key, limit, accept](const QVariantList &rows) {
            accept(parseIndexRows(rows, key, limit));
        });
}

void TwoT58Provider::search(const QString &keywords, int limit)
{
    const quint64 serial = ++m_searchSerial;
    fetchSearch(
        keywords,
        limit,
        [this, serial](const QVariantList &rows) {
            if (serial == m_searchSerial)
                emit searchReady(rows);
        });
}

QStringList TwoT58Provider::songPageCandidates(const QString &sourceId) const
{
    QStringList out;

    QString root = m_baseUrl;
    while (root.endsWith(QLatin1Char('/')))
        root.chop(1);

    out << (root + QStringLiteral("/song/")
            + sourceId + QStringLiteral(".html"));

    QUrl mirror(root);
    mirror.setHost(QStringLiteral("music.2t58.com"));
    QString mirrorRoot = mirror.toString();
    while (mirrorRoot.endsWith(QLatin1Char('/')))
        mirrorRoot.chop(1);

    const QString mirrorPage =
        mirrorRoot + QStringLiteral("/song/")
        + sourceId + QStringLiteral(".html");

    if (!out.contains(mirrorPage))
        out << mirrorPage;

    return out;
}

QStringList TwoT58Provider::extractPlayableCandidates(const QString &rawHtml,
                                                      const QUrl &pageUrl) const
{
    const QString html = normalizeEscapedHtml(rawHtml);
    QStringList out =
        WebProviderUtils::extractAudioUrls(html, pageUrl);

    auto add = [&](const QString &raw) {
        const QString url =
            WebProviderUtils::absoluteUrl(raw, pageUrl);
        if (url.isEmpty())
            return;

        const QString lower = url.toLower();
        if (lower.contains(QStringLiteral("down.php"))
            && (lower.contains(QStringLiteral("lk="))
                || lower.contains(QStringLiteral("password"))
                || lower.contains(QStringLiteral("pwd"))))
            return;

        if (!out.contains(url))
            out << url;
    };

    const QList<QRegularExpression> directPatterns{
        QRegularExpression(
            QStringLiteral(R"(<audio\b[^>]*\bsrc\s*=\s*["']([^"']+)["'])"),
            QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(
            QStringLiteral(R"(<source\b[^>]*\bsrc\s*=\s*["']([^"']+)["'])"),
            QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(
            QStringLiteral(R"((?:data-url|data-src|data-file|data-music|data-audio)\s*=\s*["']([^"']+)["'])"),
            QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(
            QStringLiteral(R"((?:playUrl|play_url|audioUrl|audio_url|musicUrl|music_url|mp3|file|src|url)\s*[:=]\s*["']([^"']+)["'])"),
            QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(
            QStringLiteral(R"(["'](?:playUrl|play_url|audioUrl|audio_url|musicUrl|music_url|mp3|file|src|url)["']\s*:\s*["']([^"']+)["'])"),
            QRegularExpression::CaseInsensitiveOption)
    };

    for (const auto &pattern : directPatterns) {
        auto it = pattern.globalMatch(html);
        while (it.hasNext())
            add(it.next().captured(1));
    }

    return out;
}

QStringList TwoT58Provider::extractResolverEndpoints(const QString &rawHtml,
                                                     const QUrl &pageUrl) const
{
    const QString html = normalizeEscapedHtml(rawHtml);
    QStringList out;

    auto add = [&](const QString &raw) {
        const QString url =
            WebProviderUtils::absoluteUrl(raw, pageUrl);
        if (url.isEmpty()
            || WebProviderUtils::looksLikeAudioUrl(url)
            || !looksLikeResolverEndpoint(url))
            return;

        if (!out.contains(url))
            out << url;
    };

    const QList<QRegularExpression> patterns{
        QRegularExpression(
            QStringLiteral(R"((?:src|href)\s*=\s*["']([^"']+)["'])"),
            QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(
            QStringLiteral(R"(["']((?:https?:)?//[^"']+|/[^"']+)["'])"),
            QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(
            QStringLiteral(R"((?:fetch|ajax|get|post)\s*\(\s*["']([^"']+)["'])"),
            QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(
            QStringLiteral(R"(\burl\s*:\s*["']([^"']+)["'])"),
            QRegularExpression::CaseInsensitiveOption)
    };

    for (const auto &pattern : patterns) {
        auto it = pattern.globalMatch(html);
        while (it.hasNext())
            add(it.next().captured(1));
    }

    return out;
}

void TwoT58Provider::probeCandidate(const QString &candidate,
                                    const QString &referer,
                                    const QString &emitId,
                                    int depth,
                                    QStringList visited)
{
    if (depth > 3 || visited.contains(candidate)) {
        emit errorOccurred(
            QStringLiteral("2t58 网页播放器没有公开可解析的音频地址"));
        return;
    }
    visited << candidate;

    QNetworkRequest request{QUrl(candidate)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      WebProviderUtils::browserUserAgent());
    request.setRawHeader("Referer", referer.toUtf8());
    request.setRawHeader("Accept",
                         "audio/*,application/json,javascript,text/plain,text/html;q=0.8,*/*;q=0.5");
    if (WebProviderUtils::looksLikeAudioUrl(candidate))
        request.setRawHeader("Range", "bytes=0-4095");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(4000);

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, candidate, referer, emitId, depth, visited] {
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            reply->deleteLater();
            return;
        }

        const QString finalUrl = reply->url().toString();
        const QString type = responseContentType(reply);
        const QByteArray bytes = reply->readAll();
        const bool networkOk = reply->error() == QNetworkReply::NoError;

        const QString text =
            WebProviderUtils::decodePageBytes(
                bytes,
                reply->header(QNetworkRequest::ContentTypeHeader).toString());

        reply->deleteLater();

        if (networkOk
            && (type.startsWith(QStringLiteral("audio/"))
                || WebProviderUtils::looksLikeAudioUrl(finalUrl)
                || WebProviderUtils::looksLikeAudioUrl(candidate))) {
            emit streamUrlReady(
                emitId,
                finalUrl.isEmpty() ? candidate : finalUrl);
            return;
        }

        if (!networkOk) {
            emit errorOccurred(
                QStringLiteral("2t58 播放器地址请求失败"));
            return;
        }

        QStringList nested =
            extractPlayableCandidates(text, QUrl(finalUrl));
        const QStringList resolver =
            extractResolverEndpoints(text, QUrl(finalUrl));

        for (const QString &url : resolver) {
            if (!nested.contains(url))
                nested << url;
        }

        if (nested.isEmpty()) {
            emit errorOccurred(
                QStringLiteral("2t58 网页播放器没有公开可解析的音频地址"));
            return;
        }

        probeCandidate(
            nested.first(),
            finalUrl,
            emitId,
            depth + 1,
            visited);
    });
}

void TwoT58Provider::tryCandidateList(const QStringList &candidates,
                                      const QString &referer,
                                      const QString &emitId,
                                      int index)
{
    if (index >= candidates.size()) {
        emit errorOccurred(
            QStringLiteral("2t58 当前页面未暴露可直接播放的地址"));
        return;
    }

    const QString candidate = candidates.at(index);
    QNetworkRequest request{QUrl(candidate)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      WebProviderUtils::browserUserAgent());
    request.setRawHeader("Referer", referer.toUtf8());
    request.setRawHeader("Accept",
                         "audio/*,application/json,text/plain,text/html;q=0.8,*/*;q=0.5");
    if (WebProviderUtils::looksLikeAudioUrl(candidate))
        request.setRawHeader("Range", "bytes=0-4095");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(3600);

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, candidate, candidates, referer, emitId, index] {
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            reply->deleteLater();
            return;
        }

        const QString finalUrl = reply->url().toString();
        const QString type = responseContentType(reply);
        const QByteArray bytes = reply->readAll();
        const bool networkOk = reply->error() == QNetworkReply::NoError;
        const QString text =
            WebProviderUtils::decodePageBytes(
                bytes,
                reply->header(QNetworkRequest::ContentTypeHeader).toString());

        reply->deleteLater();

        if (networkOk
            && (type.startsWith(QStringLiteral("audio/"))
                || WebProviderUtils::looksLikeAudioUrl(finalUrl)
                || WebProviderUtils::looksLikeAudioUrl(candidate))) {
            emit streamUrlReady(
                emitId,
                finalUrl.isEmpty() ? candidate : finalUrl);
            return;
        }

        if (networkOk && !text.isEmpty()) {
            QStringList nested =
                extractPlayableCandidates(text, QUrl(finalUrl));
            const QStringList resolver =
                extractResolverEndpoints(text, QUrl(finalUrl));
            for (const QString &url : resolver) {
                if (!nested.contains(url))
                    nested << url;
            }

            if (!nested.isEmpty()) {
                probeCandidate(
                    nested.first(),
                    finalUrl,
                    emitId,
                    1,
                    {});
                return;
            }
        }

        tryCandidateList(candidates,
                         referer,
                         emitId,
                         index + 1);
    });
}

void TwoT58Provider::resolveDirectMedia(const QString &sourceId,
                                        const QString &emitId,
                                        bool download,
                                        int qualityIndex)
{
    static const QStringList qualities{
        QStringLiteral("flac"),
        QStringLiteral("wav"),
        QStringLiteral("320")
    };

    if (qualityIndex >= qualities.size()) {
        resolvePageVariant(sourceId, emitId, 0);
        return;
    }

    QUrl url(m_baseUrl + QStringLiteral("/plug/down.php"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ac"), QStringLiteral("music"));
    query.addQueryItem(QStringLiteral("id"), sourceId);
    query.addQueryItem(QStringLiteral("k"), qualities.at(qualityIndex));
    url.setQuery(query);

    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      WebProviderUtils::browserUserAgent());
    request.setRawHeader("Referer", m_baseUrl.toUtf8());
    request.setRawHeader("Accept", "audio/*,*/*;q=0.5");
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(3600);

    QNetworkReply *reply = m_network.head(request);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, sourceId, emitId, download, qualityIndex] {
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            reply->deleteLater();
            return;
        }

        const bool networkOk =
            reply->error() == QNetworkReply::NoError;

        const QString finalUrl =
            reply->url().toString();

        const QString type =
            reply->header(QNetworkRequest::ContentTypeHeader)
                .toString()
                .section(QLatin1Char(';'), 0, 0)
                .trimmed()
                .toLower();

        const QString disposition =
            QString::fromUtf8(
                reply->rawHeader("Content-Disposition"))
                .toLower();

        const bool media =
            networkOk
            && (type.startsWith(QStringLiteral("audio/"))
                || WebProviderUtils::looksLikeAudioUrl(finalUrl)
                || disposition.contains(QStringLiteral(".mp3"))
                || disposition.contains(QStringLiteral(".flac"))
                || disposition.contains(QStringLiteral(".wav")));

        reply->deleteLater();

        if (media) {
            if (download)
                emit downloadUrlReady(emitId, finalUrl);
            else
                emit streamUrlReady(emitId, finalUrl);
            return;
        }

        resolveDirectMedia(
            sourceId,
            emitId,
            download,
            qualityIndex + 1);
    });
}

void TwoT58Provider::resolvePageVariant(const QString &sourceId,
                                        const QString &emitId,
                                        int variant)
{
    const QStringList pages =
        songPageCandidates(sourceId);

    if (variant >= pages.size()) {
        emit errorOccurred(
            QStringLiteral("2t58 歌曲页没有找到公开播放器地址"));
        return;
    }

    const QUrl page(pages.at(variant));

    struct State {
        int pending = 2;
        bool finished = false;
    };
    auto state = QSharedPointer<State>::create();

    auto handle =
        [this, state, sourceId, emitId, variant]
        (const QString &html, const QUrl &finalPage) {
            if (state->finished)
                return;

            QStringList candidates =
                extractPlayableCandidates(html, finalPage);
            const QStringList resolvers =
                extractResolverEndpoints(html, finalPage);

            for (const QString &url : resolvers) {
                if (!candidates.contains(url))
                    candidates << url;
            }

            if (!candidates.isEmpty()) {
                state->finished = true;
                tryCandidateList(
                    candidates,
                    finalPage.toString(),
                    emitId);
                return;
            }

            --state->pending;
            if (state->pending <= 0 && !state->finished) {
                state->finished = true;
                resolvePageVariant(
                    sourceId,
                    emitId,
                    variant + 1);
            }
        };

    getText(
        page,
        handle,
        [state, this, sourceId, emitId, variant](const QString &) {
            --state->pending;
            if (state->pending <= 0 && !state->finished) {
                state->finished = true;
                resolvePageVariant(
                    sourceId,
                    emitId,
                    variant + 1);
            }
        },
        m_baseUrl,
        2200);

    HttpFallbackFetcher::fetch(
        this,
        page,
        handle,
        [state, this, sourceId, emitId, variant](const QString &) {
            --state->pending;
            if (state->pending <= 0 && !state->finished) {
                state->finished = true;
                resolvePageVariant(
                    sourceId,
                    emitId,
                    variant + 1);
            }
        },
        m_baseUrl,
        3200);
}

void TwoT58Provider::resolveSongUrl(const QString &songId, const QString &)
{
    resolveDirectMedia(songId, songId, false);
}

void TwoT58Provider::resolveDownloadUrl(const QString &songId, const QString &)
{
    // Only accept this route when it directly resolves to a media response.
    // No password or protected download flow is automated.
    resolveDirectMedia(songId, songId, true);
}

QString TwoT58Provider::extractLrcText(const QString &text) const
{
    QStringList lines;
    const QRegularExpression lrc(
        QStringLiteral(
            R"(\[\d{1,2}:\d{1,2}(?:\.\d{1,3})?\][^\r\n<]+)"));

    auto it =
        lrc.globalMatch(WebProviderUtils::decodeHtml(text));
    while (it.hasNext())
        lines << it.next().captured(0).trimmed();

    return lines.join(QLatin1Char('\n'));
}

void TwoT58Provider::loadLyricsFromPage(const QString &sourceId)
{
    const QStringList pages =
        songPageCandidates(sourceId);

    if (pages.isEmpty()) {
        emit lyricsReady(sourceId, QString(), QString());
        return;
    }

    // Lyrics are a secondary feature; use the first host with a short timeout.
    getText(
        QUrl(pages.first()),
        [this, sourceId](const QString &html, const QUrl &pageUrl) {
            QString lrcUrl;

            const QRegularExpression lrcHref(
                QStringLiteral(
                    R"(href\s*=\s*["']([^"']*plug/down\.php\?[^"']*(?:lk=lrc|ac=lrc)[^"']*)["'])"),
                QRegularExpression::CaseInsensitiveOption);
            const auto match = lrcHref.match(html);
            if (match.hasMatch())
                lrcUrl =
                    WebProviderUtils::absoluteUrl(
                        match.captured(1),
                        pageUrl);

            if (lrcUrl.isEmpty()) {
                QUrl url(m_baseUrl + QStringLiteral("/plug/down.php"));
                QUrlQuery query;
                query.addQueryItem(QStringLiteral("ac"),
                                   QStringLiteral("music"));
                query.addQueryItem(QStringLiteral("id"),
                                   sourceId);
                query.addQueryItem(QStringLiteral("lk"),
                                   QStringLiteral("lrc"));
                url.setQuery(query);
                lrcUrl = url.toString();
            }

            getText(
                QUrl(lrcUrl),
                [this, sourceId](const QString &page, const QUrl &) {
                    emit lyricsReady(
                        sourceId,
                        extractLrcText(page),
                        QString());
                },
                [this, sourceId](const QString &) {
                    emit lyricsReady(
                        sourceId,
                        QString(),
                        QString());
                },
                pageUrl.toString(),
                3000);
        },
        [this, sourceId](const QString &) {
            emit lyricsReady(
                sourceId,
                QString(),
                QString());
        },
        m_baseUrl,
        2400);
}

void TwoT58Provider::loadLyrics(const QString &songId)
{
    loadLyricsFromPage(songId);
}

void TwoT58Provider::resolveTrack(const QVariantMap &track, const QString &)
{
    const QString canonicalId =
        track.value(QStringLiteral("id")).toString();
    const QString title =
        track.value(QStringLiteral("title")).toString();
    const QString artist =
        track.value(QStringLiteral("artist")).toString();

    auto resolveBest =
        [this, canonicalId, title, artist](const QVariantList &rows) -> bool {
            int bestScore = 0;
            QString bestSourceId;

            for (const QVariant &value : rows) {
                const QVariantMap candidate = value.toMap();
                const int score =
                    WebProviderUtils::trackMatchScore(
                        title,
                        artist,
                        candidate.value(QStringLiteral("title")).toString(),
                        candidate.value(QStringLiteral("artist")).toString());

                if (score > bestScore) {
                    bestScore = score;
                    bestSourceId =
                        candidate.value(QStringLiteral("sourceId")).toString();
                }
            }

            if (bestScore < 58 || bestSourceId.isEmpty())
                return false;

            resolvePageVariant(
                bestSourceId,
                canonicalId);
            return true;
        };

    fetchSearch(
        title,
        30,
        [this, title, artist, resolveBest](const QVariantList &rows) {
            if (resolveBest(rows))
                return;

            const QString expanded =
                (artist + QLatin1Char(' ') + title).trimmed();

            if (expanded == title) {
                emit errorOccurred(
                    QStringLiteral("2t58 没有匹配到同一首歌曲"));
                return;
            }

            fetchSearch(
                expanded,
                30,
                [this, resolveBest](const QVariantList &fallbackRows) {
                    if (!resolveBest(fallbackRows))
                        emit errorOccurred(
                            QStringLiteral("2t58 没有匹配到同一首歌曲"));
                });
        });
}

void TwoT58Provider::loadHome(int)
{
    emit homeReady({});
}

void TwoT58Provider::loadPlaylist(const QString &, int)
{
    emit errorOccurred(
        QStringLiteral("2t58 当前作为单曲补充源，不导入站内歌单"));
}

void TwoT58Provider::ping()
{
    fetchSearch(
        QStringLiteral("大壮"),
        3,
        [this](const QVariantList &rows) {
            emit pingReady(
                !rows.isEmpty(),
                !rows.isEmpty()
                    ? QStringLiteral("2t58 · HTTP 搜索正常")
                    : QStringLiteral("2t58 · 当前站点/CDN没有返回可解析结果"));
        },
        false);
}
