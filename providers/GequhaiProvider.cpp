#include "GequhaiProvider.h"
#include "WebProviderUtils.h"

#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <memory>

namespace {
QString shellUrl(const QString &page)
{
    return QStringLiteral("evolve-web:")
        + QString::fromLatin1(QUrl::toPercentEncoding(page));
}

QString clean(QString s)
{
    return WebProviderUtils::stripTags(s).simplified();
}

QString browserSearchShell(const QString &baseUrl,
                           const QString &title,
                           const QString &artist)
{
    const QString keywords = (title + QLatin1Char(' ') + artist).trimmed();
    QUrl url(baseUrl + QStringLiteral("/s/")
             + QString::fromLatin1(QUrl::toPercentEncoding(keywords)));
    QUrlQuery fragment;
    fragment.addQueryItem(QStringLiteral("evolve_title"), title);
    fragment.addQueryItem(QStringLiteral("evolve_artist"), artist);
    fragment.addQueryItem(QStringLiteral("evolve_provider"), QStringLiteral("gequhai"));
    url.setFragment(fragment.query());
    return shellUrl(url.toString(QUrl::FullyEncoded));
}

QString nearbyArtist(const QString &block, int anchorEnd)
{
    // Current Gequhai search pages are tables: 序号 | 歌曲 | 歌手.  Keep a
    // fallback that accepts either <td> or plain text after the song link so a
    // small template change does not make the provider disappear.
    const QRegularExpression tdRe(
        QStringLiteral(R"(<td\b[^>]*>([\s\S]*?)</td>)"),
        QRegularExpression::CaseInsensitiveOption);
    QStringList cells;
    auto tds = tdRe.globalMatch(block);
    while (tds.hasNext())
        cells << clean(tds.next().captured(1));
    if (cells.size() >= 3)
        return cells.at(2);

    const QString tail = block.mid(anchorEnd, 420);
    const QRegularExpression textCell(
        QStringLiteral(R"((?:</a>\s*</td>\s*<td\b[^>]*>|</a>\s*[-|·]\s*)([\s\S]{1,120}?)(?:</td>|<|$))"),
        QRegularExpression::CaseInsensitiveOption);
    const auto m = textCell.match(tail);
    return m.hasMatch() ? clean(m.captured(1)) : QString();
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

GequhaiProvider::GequhaiProvider(QObject *parent) : IMusicProvider(parent) {}

void GequhaiProvider::setBaseUrl(const QString &url)
{
    QString v = url.trimmed();
    while (v.endsWith('/')) v.chop(1);
    if (!v.isEmpty()) m_baseUrl = v;
}

QNetworkReply *GequhaiProvider::get(const QUrl &url,
                                    std::function<void(const QString &)> success,
                                    std::function<void(const QString &)> failure)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, WebProviderUtils::browserUserAgent());
    req.setRawHeader("Accept", "text/html,application/xhtml+xml;q=0.9,*/*;q=0.6");
    req.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.5");
    req.setRawHeader("Referer", "https://www.gequhai.com/");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(4500);
    auto *reply = m_network.get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, success = std::move(success), failure = std::move(failure)] {
        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            reply->deleteLater();
            if (failure) failure(err);
            return;
        }
        const QByteArray bytes = reply->readAll();
        const QString html = WebProviderUtils::decodePageBytes(bytes, reply->header(QNetworkRequest::ContentTypeHeader).toString());
        reply->deleteLater();
        success(html);
    });
    return reply;
}

QVariantList GequhaiProvider::parseSearch(const QString &html, int limit) const
{
    QVariantList rows;
    QSet<QString> seen;

    const QRegularExpression trRe(
        QStringLiteral(R"(<tr\b[^>]*>([\s\S]*?)</tr>)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression aRe(
        QStringLiteral(R"(<a\b[^>]*href\s*=\s*["'](?:https?://(?:www\.)?gequhai\.com)?/play/(\d+)(?:[/?#][^"']*)?["'][^>]*>([\s\S]*?)</a>)"),
        QRegularExpression::CaseInsensitiveOption);

    auto appendBlock = [&](const QString &block) {
        if (rows.size() >= limit) return;
        const auto a = aRe.match(block);
        if (!a.hasMatch()) return;
        const QString id = a.captured(1);
        if (id.isEmpty() || seen.contains(id)) return;

        const QString title = clean(a.captured(2));
        if (title.isEmpty()) return;

        const QString artist = nearbyArtist(block, a.capturedEnd());
        seen.insert(id);
        rows << QVariantMap{
            {"id", id}, {"sourceId", id}, {"title", title}, {"artist", artist},
            {"album", QString()}, {"cover", QString()}, {"duration", 0},
            {"access", QStringLiteral("web")}, {"playable", true},
            {"pageUrl", pageUrl(id)}
        };
    };

    auto trs = trRe.globalMatch(html);
    while (trs.hasNext() && rows.size() < limit)
        appendBlock(trs.next().captured(1));

    // Template fallback: some mobile/edge variants do not use <tr>. Parse the
    // song anchors directly and inspect the nearby fragment for the artist.
    if (rows.size() < limit) {
        auto anchors = aRe.globalMatch(html);
        while (anchors.hasNext() && rows.size() < limit) {
            const auto m = anchors.next();
            const int begin = qMax(0, m.capturedStart() - 160);
            const int end = qMin(html.size(), m.capturedEnd() + 360);
            appendBlock(html.mid(begin, end - begin));
        }
    }

    return rows;
}

void GequhaiProvider::fetchSearch(const QString &keywords, int limit, SearchCallback callback, ErrorCallback failure)
{
    const QString normalized = keywords.trimmed().simplified();
    if (normalized.isEmpty()) {
        callback({});
        return;
    }

    const QString path = m_baseUrl + QStringLiteral("/s/")
        + QString::fromLatin1(QUrl::toPercentEncoding(normalized));
    get(QUrl(path),
        [this, limit, callback = std::move(callback)](const QString &html) { callback(parseSearch(html, limit)); },
        [this, failure = std::move(failure)](const QString &err) mutable {
            if (failure) failure(err);
            else emit errorOccurred(QStringLiteral("歌曲海搜索失败：") + err);
        });
}

void GequhaiProvider::search(const QString &keywords, int limit)
{
    const quint64 generation = ++m_searchGeneration;
    fetchSearch(keywords, limit,
        [this, generation](const QVariantList &rows) {
            if (generation == m_searchGeneration)
                emit searchReady(rows);
        },
        [this, generation](const QString &err) {
            if (generation == m_searchGeneration)
                emit errorOccurred(QStringLiteral("歌曲海搜索失败：") + err);
        });
}

void GequhaiProvider::resolveTrack(const QVariantMap &track, const QString &)
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
    if (title.isEmpty()) { emit errorOccurred(QStringLiteral("歌曲海：缺少歌曲名")); return; }

    struct State {
        int pending = 0;
        bool finished = false;
        int bestScore = -1;
        QVariantMap best;
        QString lastError;
    };
    const auto state = std::make_shared<State>();

    QStringList queries;
    const QString combined = (title + QLatin1Char(' ') + artist).trimmed();
    if (!combined.isEmpty()) queries << combined;
    queries << title;
    queries.removeDuplicates();
    state->pending = queries.size();

    const auto finishOne = [this, state, title, artist, emitId](const QVariantList &rows, const QString &error) {
        if (state->finished) return;
        if (!error.isEmpty()) state->lastError = error;

        int score = -1;
        const QVariantMap match = bestMatch(rows, title, artist, &score);
        if (score > state->bestScore) { state->bestScore = score; state->best = match; }
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
                ? QStringLiteral("歌曲海没有匹配到同一首歌曲的播放页面")
                : QStringLiteral("歌曲海查找失败：") + state->lastError);
            return;
        }
        const QString page = state->best.value("pageUrl", pageUrl(state->best.value("sourceId").toString())).toString();
        emit streamUrlReady(emitId, shellUrl(page));
    };

    // Use the real browser as a fast fallback when an edge node returns a
    // template that the lightweight HTML parser cannot understand. The browser
    // stays on Gequhai and selects the matching public song page itself.
    QTimer::singleShot(850, this, [this, state, emitId, title, artist] {
        if (state->finished) return;
        state->finished = true;
        emit streamUrlReady(emitId, browserSearchShell(m_baseUrl, title, artist));
    });

    for (const QString &query : queries) {
        fetchSearch(query, 12,
            [finishOne](const QVariantList &rows) { finishOne(rows, QString()); },
            [finishOne](const QString &error) { finishOne({}, error); });
    }
}

QString GequhaiProvider::pageUrl(const QString &songId) const
{
    return m_baseUrl + QStringLiteral("/play/") + songId;
}

void GequhaiProvider::resolveSongUrl(const QString &songId, const QString &)
{
    emit streamUrlReady(songId, shellUrl(pageUrl(songId)));
}
void GequhaiProvider::resolveDownloadUrl(const QString &, const QString &) { emit errorOccurred(QStringLiteral("歌曲海网页源不提供应用内下载")); }
void GequhaiProvider::loadLyrics(const QString &songId) { emit lyricsReady(songId, QString(), QString()); }
void GequhaiProvider::loadHome(int) { emit homeReady(QVariantList{}); }
void GequhaiProvider::loadPlaylist(const QString &, int) { emit playlistReady(QVariantMap{}, QVariantList{}); }
void GequhaiProvider::ping()
{
    get(QUrl(m_baseUrl), [this](const QString &) { emit pingReady(true, QStringLiteral("歌曲海网页播放源可访问")); },
        [this](const QString &e) { emit pingReady(false, QStringLiteral("歌曲海连接失败：") + e); });
}
