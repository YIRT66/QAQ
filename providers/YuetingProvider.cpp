#include "YuetingProvider.h"
#include "WebProviderUtils.h"

#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QSet>
#include <QTimer>
#include <memory>

namespace {
QString shellUrl(const QString &page)
{
    return QStringLiteral("evolve-web:") + QString::fromLatin1(QUrl::toPercentEncoding(page));
}

QString clean(QString s)
{
    return WebProviderUtils::stripTags(s).simplified();
}

QString browserSearchShell(const QString &baseUrl,
                           const QString &title,
                           const QString &artist)
{
    QUrl url(baseUrl + QStringLiteral("/so"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("nsid"), QStringLiteral("4"));
    query.addQueryItem(QStringLiteral("page"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("q"), (title + QLatin1Char(' ') + artist).trimmed());
    url.setQuery(query);

    QUrlQuery fragment;
    fragment.addQueryItem(QStringLiteral("evolve_title"), title);
    fragment.addQueryItem(QStringLiteral("evolve_artist"), artist);
    fragment.addQueryItem(QStringLiteral("evolve_provider"), QStringLiteral("yueting"));
    url.setFragment(fragment.query());
    return shellUrl(url.toString(QUrl::FullyEncoded));
}

QStringList splitTitleArtist(QString s)
{
    s = clean(s);

    // Current Yueting result cards commonly use an en/em dash without spaces
    // (e.g. “一千个伤心的理由–张学友”).  Keep ASCII hyphens inside English song
    // names intact, but treat a Unicode dash or a whitespace-adjacent '-' as
    // the title/artist separator.
    int splitPos = -1;
    int splitLen = 0;
    for (const QChar dash : {QChar(0x2013), QChar(0x2014)}) {
        const int pos = s.lastIndexOf(dash);
        if (pos > splitPos) { splitPos = pos; splitLen = 1; }
    }

    if (splitPos < 0) {
        const QRegularExpression separator(QStringLiteral(R"((?:\s+-\s*|\s*-\s+))"));
        auto it = separator.globalMatch(s);
        QRegularExpressionMatch last;
        while (it.hasNext()) last = it.next();
        if (last.hasMatch()) {
            splitPos = last.capturedStart();
            splitLen = last.capturedLength();
        }
    }

    if (splitPos <= 0)
        return {s, QString()};

    const QString title = s.left(splitPos).trimmed();
    const QString artist = s.mid(splitPos + splitLen).trimmed();
    if (title.isEmpty() || artist.isEmpty())
        return {s, QString()};
    return {title, artist};
}

QVariantMap bestMatch(const QVariantList &rows,
                      const QString &title,
                      const QString &artist,
                      int *outScore)
{
    QVariantMap best;
    int score = -1;
    for (const QVariant &value : rows) {
        const QVariantMap row = value.toMap();
        const int current = WebProviderUtils::trackMatchScore(
            title, artist,
            row.value(QStringLiteral("title")).toString(),
            row.value(QStringLiteral("artist")).toString());
        if (current > score) { score = current; best = row; }
    }
    if (outScore) *outScore = score;
    return best;
}

}

YuetingProvider::YuetingProvider(QObject *parent) : IMusicProvider(parent) {}

void YuetingProvider::setBaseUrl(const QString &url)
{
    QString v = url.trimmed();
    while (v.endsWith('/')) v.chop(1);
    if (!v.isEmpty()) m_baseUrl = v;
}

QNetworkReply *YuetingProvider::get(const QUrl &url,
                                    std::function<void(const QString &)> success,
                                    std::function<void(const QString &)> failure)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, WebProviderUtils::browserUserAgent());
    req.setRawHeader("Accept", "text/html,application/xhtml+xml;q=0.9,*/*;q=0.6");
    req.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.5");
    req.setRawHeader("Referer", "https://www.yueting.net/tool/search/");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(4800);
    auto *reply = m_network.get(req);
    connect(reply, &QNetworkReply::finished, this,
            [reply, success = std::move(success), failure = std::move(failure)] {
        if (reply->error() != QNetworkReply::NoError) {
            const QString e = reply->errorString();
            reply->deleteLater();
            if (failure) failure(e);
            return;
        }
        const QByteArray bytes = reply->readAll();
        const QString html = WebProviderUtils::decodePageBytes(
            bytes, reply->header(QNetworkRequest::ContentTypeHeader).toString());
        reply->deleteLater();
        success(html);
    });
    return reply;
}

QVariantList YuetingProvider::parseSearch(const QString &html, int limit) const
{
    QVariantList rows;
    QSet<QString> seen;

    // Some edge variants render links in script/template fragments with escaped
    // slashes. Normalize those first, then parse the same /song/<id>.html form.
    QString normalizedHtml = html;
    normalizedHtml.replace(QStringLiteral("\\/"), QStringLiteral("/"));
    normalizedHtml.replace(QStringLiteral("\\u002F"), QStringLiteral("/"), Qt::CaseInsensitive);
    normalizedHtml.replace(QStringLiteral("\\x2F"), QStringLiteral("/"), Qt::CaseInsensitive);

    const QRegularExpression linkRe(
        QStringLiteral(
            R"(<a\b([^>]*?)href\s*=\s*["'](?:https?://(?:www\.)?yueting\.net)?/?song/(\d+)\.html(?:[?#][^"']*)?["']([^>]*)>([\s\S]*?)</a>)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression titleAttr(
        QStringLiteral(R"(\btitle\s*=\s*["']([^"']+)["'])"),
        QRegularExpression::CaseInsensitiveOption);

    auto consume = [&](const QRegularExpression &re) {
        auto it = re.globalMatch(normalizedHtml);
        while (it.hasNext() && rows.size() < limit) {
            const auto m = it.next();
            const QString id = m.captured(2);
            if (id.isEmpty() || seen.contains(id)) continue;

            QString text = clean(m.captured(4));
            if (text.isEmpty()) {
                const auto tm = titleAttr.match(m.captured(1) + QLatin1Char(' ') + m.captured(3));
                if (tm.hasMatch()) text = WebProviderUtils::decodeHtml(tm.captured(1)).simplified();
            }
            if (text.isEmpty()) continue;

            const QStringList parts = splitTitleArtist(text);
            const QString title = parts.value(0);
            const QString artist = parts.value(1);
            if (title.isEmpty()) continue;

            seen.insert(id);
            rows << QVariantMap{
                {"id", id}, {"sourceId", id}, {"title", title}, {"artist", artist},
                {"album", QString()}, {"cover", QString()}, {"duration", 0},
                {"access", QStringLiteral("web")}, {"playable", true},
                {"pageUrl", pageUrl(id)}
            };
        }
    };

    consume(linkRe);

    // Fallback for edge-rendered search pages where a song URL appears in a
    // template/script fragment instead of a complete <a> element.  We only use
    // the public page URL and visible/title metadata; playback still happens in
    // the site's own WebView page.
    if (rows.size() < limit) {
        const QRegularExpression looseUrl(
            QStringLiteral(R"((?:https?://(?:www\.)?yueting\.net)?/?song/([0-9]+)\.html)"),
            QRegularExpression::CaseInsensitiveOption);
        auto it = looseUrl.globalMatch(normalizedHtml);
        while (it.hasNext() && rows.size() < limit) {
            const auto m = it.next();
            const QString id = m.captured(1);
            if (id.isEmpty() || seen.contains(id))
                continue;

            const int begin = qMax(0, m.capturedStart() - 220);
            const int end = qMin(normalizedHtml.size(), m.capturedEnd() + 320);
            const QString fragment = normalizedHtml.mid(begin, end - begin);

            QString text;
            const auto titleMatch = titleAttr.match(fragment);
            if (titleMatch.hasMatch())
                text = WebProviderUtils::decodeHtml(titleMatch.captured(1)).simplified();

            if (text.isEmpty())
                continue;

            const QStringList parts = splitTitleArtist(text);
            const QString title = parts.value(0).trimmed();
            if (title.isEmpty())
                continue;

            seen.insert(id);
            rows << QVariantMap{
                {"id", id}, {"sourceId", id}, {"title", title}, {"artist", parts.value(1)},
                {"album", QString()}, {"cover", QString()}, {"duration", 0},
                {"access", QStringLiteral("web")}, {"playable", true},
                {"pageUrl", pageUrl(id)}
            };
        }
    }

    return rows;
}

void YuetingProvider::fetchSearch(const QString &keywords, int limit, SearchCallback callback, ErrorCallback failure)
{
    const QString normalized = keywords.trimmed().simplified();
    if (normalized.isEmpty()) {
        callback({});
        return;
    }

    QUrl url(m_baseUrl + QStringLiteral("/so"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("nsid"), QStringLiteral("4"));
    query.addQueryItem(QStringLiteral("page"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("q"), normalized);
    url.setQuery(query);

    get(url,
        [this, limit, callback = std::move(callback)](const QString &html) mutable {
            callback(parseSearch(html, limit));
        },
        [this, failure = std::move(failure)](const QString &e) mutable {
            if (failure) failure(e);
            else emit errorOccurred(QStringLiteral("悦听搜索失败：") + e);
        });
}

void YuetingProvider::search(const QString &keywords, int limit)
{
    const quint64 generation = ++m_searchGeneration;
    fetchSearch(keywords, limit,
        [this, generation](const QVariantList &rows) {
            if (generation == m_searchGeneration)
                emit searchReady(rows);
        },
        [this, generation](const QString &e) {
            if (generation == m_searchGeneration)
                emit errorOccurred(QStringLiteral("悦听搜索失败：") + e);
        });
}

void YuetingProvider::resolveTrack(const QVariantMap &track, const QString &)
{
    // A result that came directly from this website already carries its exact
    // public song page.  Do not search the same website again before playback;
    // that old extra round trip made web-source playback slow and occasionally
    // selected a different cover/live version.
    const QString directPage = track.value(QStringLiteral("pageUrl")).toString().trimmed();
    if (!directPage.isEmpty()) {
        emit streamUrlReady(track.value(QStringLiteral("id")).toString(), shellUrl(directPage));
        return;
    }
    const QString title = track.value("title").toString().trimmed();
    const QString artist = track.value("artist").toString().trimmed();
    const QString emitId = track.value("id").toString();
    if (title.isEmpty()) { emit errorOccurred(QStringLiteral("悦听：缺少歌曲名")); return; }

    struct State {
        int pending = 0;
        bool finished = false;
        int bestScore = -1;
        QVariantMap best;
        QString lastError;
    };
    const auto state = std::make_shared<State>();

    QStringList queries{title};
    const QString combined = (title + QLatin1Char(' ') + artist).trimmed();
    if (!artist.isEmpty() && combined.compare(title, Qt::CaseInsensitive) != 0)
        queries << combined;
    queries.removeDuplicates();
    state->pending = queries.size();

    const auto finishOne = [this, state, title, artist, emitId](const QVariantList &rows, const QString &error) {
        if (state->finished) return;
        if (!error.isEmpty()) state->lastError = error;

        int score = -1;
        const QVariantMap match = bestMatch(rows, title, artist, &score);
        if (score > state->bestScore) { state->bestScore = score; state->best = match; }

        // 70 means exact normalized title.  Return immediately instead of
        // waiting for the other query variant; otherwise wait and compare both.
        if (score >= 70 && !match.isEmpty()) {
            state->finished = true;
            const QString page = match.value("pageUrl", pageUrl(match.value("sourceId").toString())).toString();
            emit streamUrlReady(emitId, shellUrl(page));
            return;
        }

        if (--state->pending > 0) return;
        state->finished = true;
        if (state->bestScore < 48 || state->best.isEmpty()) {
            emit errorOccurred(state->lastError.isEmpty()
                ? QStringLiteral("悦听没有匹配到同一首歌曲的播放页面")
                : QStringLiteral("悦听查找失败：") + state->lastError);
            return;
        }
        const QString page = state->best.value("pageUrl", pageUrl(state->best.value("sourceId").toString())).toString();
        emit streamUrlReady(emitId, shellUrl(page));
    };

    // Yueting can render its search results client-side on some edge nodes.
    // Give the fast HTML parser a short head start; if it has not found an
    // exact page quickly, let the embedded site-owned WebView perform the same
    // search with JavaScript enabled instead of waiting several seconds for
    // sequential fallbacks. No media URL is extracted here.
    QTimer::singleShot(850, this, [this, state, emitId, title, artist] {
        if (state->finished) return;
        state->finished = true;
        emit streamUrlReady(emitId, browserSearchShell(m_baseUrl, title, artist));
    });

    for (const QString &query : queries) {
        fetchSearch(query, 14,
            [finishOne](const QVariantList &rows) { finishOne(rows, QString()); },
            [finishOne](const QString &error) { finishOne({}, error); });
    }
}

QString YuetingProvider::pageUrl(const QString &id) const
{
    return m_baseUrl + QStringLiteral("/song/") + id + QStringLiteral(".html");
}
void YuetingProvider::resolveSongUrl(const QString &id, const QString &)
{
    emit streamUrlReady(id, shellUrl(pageUrl(id)));
}
void YuetingProvider::resolveDownloadUrl(const QString &, const QString &) { emit errorOccurred(QStringLiteral("悦听网页源不提供应用内下载")); }
void YuetingProvider::loadLyrics(const QString &songId) { emit lyricsReady(songId, QString(), QString()); }
void YuetingProvider::loadHome(int) { emit homeReady(QVariantList{}); }
void YuetingProvider::loadPlaylist(const QString &, int) { emit playlistReady(QVariantMap{}, QVariantList{}); }
void YuetingProvider::ping()
{
    get(QUrl(m_baseUrl), [this](const QString &) { emit pingReady(true, QStringLiteral("悦听网页播放源可访问")); },
        [this](const QString &e) { emit pingReady(false, QStringLiteral("悦听连接失败：") + e); });
}
