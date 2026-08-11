#include "PublicSiteIndex.h"
#include "HttpFallbackFetcher.h"
#include "WebProviderUtils.h"

#include <QRegularExpression>
#include <QSet>
#include <QSharedPointer>
#include <QUrl>
#include <QUrlQuery>
#include <QXmlStreamReader>

namespace PublicSiteIndex {

namespace {

QString normalizedDomain(QString value)
{
    value = value.trimmed().toLower();
    if (value.startsWith(QStringLiteral("www.")))
        value.remove(0, 4);
    return value;
}

bool hostMatches(QString host, const QString &wanted)
{
    host = normalizedDomain(host);
    const QString target = normalizedDomain(wanted);
    return host == target || host.endsWith(QStringLiteral(".") + target);
}

QString sourceIdFromUrl(const QUrl &url, const QString &pathPrefix)
{
    QString path = url.path();
    if (!path.startsWith(pathPrefix, Qt::CaseInsensitive))
        return {};

    QString tail = path.mid(pathPrefix.size());
    while (tail.startsWith(QLatin1Char('/')))
        tail.remove(0, 1);

    const int slash = tail.indexOf(QLatin1Char('/'));
    if (slash >= 0)
        tail = tail.left(slash);

    if (tail.endsWith(QStringLiteral(".html"), Qt::CaseInsensitive))
        tail.chop(5);

    return tail.trimmed();
}

QVariantList parseRss(const QString &text,
                      const QString &domain,
                      const QString &pathPrefix,
                      int limit)
{
    QVariantList rows;
    QSet<QString> seen;
    QXmlStreamReader xml(text);

    while (!xml.atEnd() && rows.size() < limit) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QStringLiteral("item"))
            continue;

        QString title;
        QString link;
        QString description;

        while (!(xml.isEndElement() && xml.name() == QStringLiteral("item"))
               && !xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement())
                continue;

            if (xml.name() == QStringLiteral("title"))
                title = xml.readElementText(QXmlStreamReader::IncludeChildElements);
            else if (xml.name() == QStringLiteral("link"))
                link = xml.readElementText(QXmlStreamReader::IncludeChildElements);
            else if (xml.name() == QStringLiteral("description"))
                description = xml.readElementText(QXmlStreamReader::IncludeChildElements);
        }

        const QUrl url(WebProviderUtils::decodeHtml(link.trimmed()));
        if (!url.isValid() || !hostMatches(url.host(), domain))
            continue;

        const QString sourceId = sourceIdFromUrl(url, pathPrefix);
        if (sourceId.isEmpty() || seen.contains(sourceId))
            continue;

        seen.insert(sourceId);
        rows << QVariantMap{
            {QStringLiteral("url"), url.toString()},
            {QStringLiteral("sourceId"), sourceId},
            {QStringLiteral("title"), WebProviderUtils::stripTags(title)},
            {QStringLiteral("description"), WebProviderUtils::stripTags(description)}
        };
    }

    return rows;
}

QVariantList parseHtmlFallback(const QString &html,
                               const QString &domain,
                               const QString &pathPrefix,
                               int limit)
{
    QVariantList rows;
    QSet<QString> seen;

    const QString domainPattern =
        QRegularExpression::escape(normalizedDomain(domain));
    const QString pathPattern =
        QRegularExpression::escape(pathPrefix);

    const QRegularExpression absolute(
        QStringLiteral(
            R"((https?://(?:[A-Za-z0-9.-]+\.)?%1%2[A-Za-z0-9_-]+(?:\.html)?))")
            .arg(domainPattern, pathPattern),
        QRegularExpression::CaseInsensitiveOption);

    auto it = absolute.globalMatch(WebProviderUtils::decodeHtml(html));
    while (it.hasNext() && rows.size() < limit) {
        const QUrl url(it.next().captured(1));
        if (!url.isValid() || !hostMatches(url.host(), domain))
            continue;

        const QString sourceId = sourceIdFromUrl(url, pathPrefix);
        if (sourceId.isEmpty() || seen.contains(sourceId))
            continue;

        seen.insert(sourceId);
        rows << QVariantMap{
            {QStringLiteral("url"), url.toString()},
            {QStringLiteral("sourceId"), sourceId},
            {QStringLiteral("title"), QString()},
            {QStringLiteral("description"), QString()}
        };
    }

    return rows;
}

QVariantList parseIndex(const QString &text,
                        const QString &domain,
                        const QString &pathPrefix,
                        int limit)
{
    QVariantList rows = parseRss(text, domain, pathPrefix, limit);
    if (!rows.isEmpty())
        return rows;
    return parseHtmlFallback(text, domain, pathPrefix, limit);
}

QUrl bingUrl(const QString &host,
             const QString &domain,
             const QString &pathPrefix,
             const QString &keywords)
{
    QUrl url(QStringLiteral("https://") + host + QStringLiteral("/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("rss"));

    Q_UNUSED(pathPrefix)

    // Broad site query. The old `site:domain/song/` filter was too strict:
    // search engines often return a singer/listing page that contains the
    // desired song rather than the song page itself.
    QString scope = QStringLiteral("site:")
                  + normalizedDomain(domain);
    QString q = scope + QStringLiteral(" \"")
              + keywords.trimmed().simplified()
              + QStringLiteral("\"");

    query.addQueryItem(QStringLiteral("q"), q);
    url.setQuery(query);
    return url;
}

} // namespace

void search(QObject *owner,
            const QString &domain,
            const QString &pathPrefix,
            const QString &keywords,
            int limit,
            ResultsCallback callback)
{
    const QString key = keywords.trimmed().simplified();
    if (!owner || key.isEmpty() || domain.trimmed().isEmpty()) {
        callback({});
        return;
    }

    struct State {
        int pending = 2;
        bool finished = false;
    };
    auto state = QSharedPointer<State>::create();

    auto accept =
        [state, callback](const QVariantList &rows) {
            if (state->finished)
                return;

            if (!rows.isEmpty()) {
                state->finished = true;
                callback(rows);
                return;
            }

            --state->pending;
            if (state->pending <= 0 && !state->finished) {
                state->finished = true;
                callback({});
            }
        };

    const QList<QUrl> urls{
        bingUrl(QStringLiteral("www.bing.com"), domain, pathPrefix, key),
        bingUrl(QStringLiteral("cn.bing.com"), domain, pathPrefix, key)
    };

    for (const QUrl &url : urls) {
        HttpFallbackFetcher::fetch(
            owner,
            url,
            [domain, pathPrefix, limit, accept]
            (const QString &page, const QUrl &) {
                accept(parseIndex(page, domain, pathPrefix, limit));
            },
            [accept](const QString &) {
                accept({});
            },
            QString(),
            3000);
    }
}

} // namespace PublicSiteIndex
