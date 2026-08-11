#pragma once

#include <QUrl>
#include <QByteArray>
#include <QString>
#include <QStringList>

namespace WebProviderUtils {

QString browserUserAgent();
QString decodePageBytes(const QByteArray &bytes,
                        const QString &contentType = QString());
QString decodeHtml(QString value);
QString stripTags(QString value);
QString normalizeText(const QString &value);
QString absoluteUrl(const QString &raw, const QUrl &baseUrl);
bool looksLikeAudioUrl(const QString &url);
QStringList extractAudioUrls(const QString &text, const QUrl &baseUrl);
int trackMatchScore(const QString &wantedTitle,
                    const QString &wantedArtist,
                    const QString &candidateTitle,
                    const QString &candidateArtist);

}
