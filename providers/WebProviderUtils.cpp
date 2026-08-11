#include "WebProviderUtils.h"

#include <QRegularExpression>
#include <QStringDecoder>

namespace WebProviderUtils {

QString browserUserAgent()
{
    return QStringLiteral(
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/131.0.0.0 Safari/537.36");
}

QString decodePageBytes(const QByteArray &bytes, const QString &contentType)
{
    if (bytes.isEmpty())
        return {};

    QString charset;

    const QRegularExpression headerCharset(
        QStringLiteral(R"(charset\s*=\s*["']?\s*([A-Za-z0-9._-]+))"),
        QRegularExpression::CaseInsensitiveOption);
    const auto headerMatch = headerCharset.match(contentType);
    if (headerMatch.hasMatch())
        charset = headerMatch.captured(1).trimmed().toLower();

    if (charset.isEmpty()) {
        const QByteArray head = bytes.left(8192);
        const QString latin = QString::fromLatin1(head);

        const QRegularExpression metaCharset(
            QStringLiteral(R"(<meta[^>]+charset\s*=\s*["']?\s*([A-Za-z0-9._-]+))"),
            QRegularExpression::CaseInsensitiveOption);
        const auto metaMatch = metaCharset.match(latin);
        if (metaMatch.hasMatch())
            charset = metaMatch.captured(1).trimmed().toLower();

        if (charset.isEmpty()) {
            const QRegularExpression metaContent(
                QStringLiteral(R"(<meta[^>]+content\s*=\s*["'][^"']*charset\s*=\s*([A-Za-z0-9._-]+))"),
                QRegularExpression::CaseInsensitiveOption);
            const auto contentMatch = metaContent.match(latin);
            if (contentMatch.hasMatch())
                charset = contentMatch.captured(1).trimmed().toLower();
        }
    }

    auto decodeWith = [&](const char *name) -> QString {
        QStringDecoder decoder(name);
        if (!decoder.isValid())
            return {};
        return decoder.decode(bytes);
    };

    if (charset.contains(QStringLiteral("gb18030")))
        return decodeWith("GB18030");
    if (charset.contains(QStringLiteral("gb2312"))
        || charset.contains(QStringLiteral("gbk"))
        || charset.contains(QStringLiteral("cp936"))) {
        QString decoded = decodeWith("GB18030");
        if (!decoded.isEmpty())
            return decoded;
    }

    QString utf8 = QString::fromUtf8(bytes);
    if (utf8.count(QChar::ReplacementCharacter) <= 8)
        return utf8;

    QString gb = decodeWith("GB18030");
    if (!gb.isEmpty() && gb.count(QChar::ReplacementCharacter) < utf8.count(QChar::ReplacementCharacter))
        return gb;

    return QString::fromLocal8Bit(bytes);
}

QString decodeHtml(QString value)
{
    value.replace(QStringLiteral("&amp;"), QStringLiteral("&"), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&quot;"), QStringLiteral("\""), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&#39;"), QStringLiteral("'"), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&apos;"), QStringLiteral("'"), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&lt;"), QStringLiteral("<"), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&gt;"), QStringLiteral(">"), Qt::CaseInsensitive);
    value.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "), Qt::CaseInsensitive);

    static const QRegularExpression numeric(QStringLiteral(R"(&#(x?[0-9A-Fa-f]+);)"));
    qsizetype offset = 0;
    for (;;) {
        const QRegularExpressionMatch match = numeric.match(value, offset);
        if (!match.hasMatch())
            break;

        bool ok = false;
        uint code = 0;
        const QString token = match.captured(1);
        if (token.startsWith(QLatin1Char('x'), Qt::CaseInsensitive))
            code = token.mid(1).toUInt(&ok, 16);
        else
            code = token.toUInt(&ok, 10);

        QString replacement;
        if (ok && code <= 0x10FFFF) {
            const char32_t cp = static_cast<char32_t>(code);
            replacement = QString::fromUcs4(&cp, 1);
        }

        value.replace(match.capturedStart(), match.capturedLength(), replacement);
        offset = match.capturedStart() + replacement.size();
    }
    return value;
}

QString stripTags(QString value)
{
    value.replace(QRegularExpression(
                      QStringLiteral(R"(<script\b[^>]*>[\s\S]*?</script>)"),
                      QRegularExpression::CaseInsensitiveOption),
                  QString());
    value.replace(QRegularExpression(
                      QStringLiteral(R"(<style\b[^>]*>[\s\S]*?</style>)"),
                      QRegularExpression::CaseInsensitiveOption),
                  QString());
    value.replace(QRegularExpression(QStringLiteral(R"(<br\s*/?>)"),
                                     QRegularExpression::CaseInsensitiveOption),
                  QStringLiteral(" "));
    value.replace(QRegularExpression(QStringLiteral(R"(<[^>]+>)")),
                  QStringLiteral(" "));
    return decodeHtml(value).simplified();
}

QString normalizeText(const QString &value)
{
    QString out = value.toCaseFolded().simplified();
    out.replace(QChar(0x2013), QLatin1Char('-'));
    out.replace(QChar(0x2014), QLatin1Char('-'));
    out.remove(QRegularExpression(QStringLiteral(R"([\s\p{P}\p{S}]+)")));
    return out;
}

QString absoluteUrl(const QString &raw, const QUrl &baseUrl)
{
    QString value = decodeHtml(raw.trimmed());
    value.replace(QStringLiteral("\\/"), QStringLiteral("/"));
    value.replace(QStringLiteral("\\u002F"), QStringLiteral("/"),
                  Qt::CaseInsensitive);
    value.replace(QStringLiteral("\\x2F"), QStringLiteral("/"),
                  Qt::CaseInsensitive);

    if (value.startsWith(QStringLiteral("//")))
        return baseUrl.scheme() + QStringLiteral(":") + value;

    const QUrl url(value);
    if (!url.isValid())
        return {};

    return url.isRelative() ? baseUrl.resolved(url).toString() : url.toString();
}

bool looksLikeAudioUrl(const QString &url)
{
    return QRegularExpression(
               QStringLiteral(R"(\.(?:mp3|m4a|aac|flac|ogg|opus|wav)(?:[?#]|$))"),
               QRegularExpression::CaseInsensitiveOption)
        .match(url)
        .hasMatch();
}

QStringList extractAudioUrls(const QString &text, const QUrl &baseUrl)
{
    QStringList out;

    auto add = [&](const QString &raw) {
        const QString resolved = absoluteUrl(raw, baseUrl);
        if (resolved.isEmpty() || !looksLikeAudioUrl(resolved))
            return;
        if (!out.contains(resolved))
            out << resolved;
    };

    const QList<QRegularExpression> patterns{
        QRegularExpression(
            QStringLiteral(R"((https?:\/\/[^"'<>\\\s]+?\.(?:mp3|m4a|aac|flac|ogg|opus|wav)(?:\?[^"'<>\\\s]*)?))"),
            QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(
            QStringLiteral(R"((?:src|href|data-src|data-url|data-mp3|data-file)\s*=\s*["']([^"']+\.(?:mp3|m4a|aac|flac|ogg|opus|wav)(?:\?[^"']*)?)["'])"),
            QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(
            QStringLiteral(R"((?:url|src|file|mp3|musicUrl|playUrl|audioUrl)\s*[:=]\s*["']([^"']+\.(?:mp3|m4a|aac|flac|ogg|opus|wav)(?:\?[^"']*)?)["'])"),
            QRegularExpression::CaseInsensitiveOption)
    };

    for (const QRegularExpression &pattern : patterns) {
        auto it = pattern.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            add(match.captured(1));
        }
    }

    return out;
}

int trackMatchScore(const QString &wantedTitle,
                    const QString &wantedArtist,
                    const QString &candidateTitle,
                    const QString &candidateArtist)
{
    const QString wt = normalizeText(wantedTitle);
    const QString wa = normalizeText(wantedArtist);
    const QString ct = normalizeText(candidateTitle);
    const QString ca = normalizeText(candidateArtist);

    if (wt.isEmpty() || ct.isEmpty())
        return 0;

    int score = 0;
    if (wt == ct)
        score += 70;
    else if (ct.contains(wt) || wt.contains(ct))
        score += 48;

    if (!wa.isEmpty() && !ca.isEmpty()) {
        if (wa == ca)
            score += 30;
        else if (ca.contains(wa) || wa.contains(ca))
            score += 18;
    } else if (wa.isEmpty()) {
        score += 6;
    }

    return score;
}

}
