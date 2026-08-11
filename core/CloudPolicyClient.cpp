#include "CloudPolicyClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

CloudPolicyClient::CloudPolicyClient(QObject *parent)
    : QObject(parent)
{
}

void CloudPolicyClient::setApiBaseUrl(const QString &url)
{
    QString normalized = url.trimmed();

    while (normalized.endsWith(QLatin1Char('/')))
        normalized.chop(1);

    m_apiBaseUrl = normalized;
}

bool CloudPolicyClient::transportAllowed(const QUrl &url) const
{
    const QString scheme = url.scheme().toLower();
    const QString host = url.host().toLower();

    return scheme == QStringLiteral("https")
        || host == QStringLiteral("localhost")
        || host == QStringLiteral("127.0.0.1")
        || host == QStringLiteral("::1");
}

void CloudPolicyClient::refresh()
{
    if (m_apiBaseUrl.isEmpty())
        return;

    const QUrl url(
        m_apiBaseUrl
        + QStringLiteral("/v1/config"));

    if (!url.isValid()
        || !transportAllowed(url)) {
        emit statusChanged(
            false,
            QStringLiteral(
                "云端策略地址无效或不是 HTTPS"));
        return;
    }

    QNetworkRequest request{url};
    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        QStringLiteral("EvolveMusic/0.9.1"));

    request.setRawHeader(
        "Accept",
        "application/json");

    request.setRawHeader(
        "Cache-Control",
        "no-cache");

    // Deliberately no local profile ID, provider account, cookies, favorites,
    // history or playback data are included in this request.
    request.setTransferTimeout(2600);

    QNetworkReply *reply =
        m_network.get(request);

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply] {
            if (reply->error()
                == QNetworkReply::OperationCanceledError) {
                reply->deleteLater();
                return;
            }

            if (reply->error()
                != QNetworkReply::NoError) {
                emit statusChanged(
                    false,
                    QStringLiteral(
                        "Cloudflare API 暂不可用：")
                    + reply->errorString());

                reply->deleteLater();
                return;
            }

            QJsonParseError parseError{};
            const QJsonDocument document =
                QJsonDocument::fromJson(
                    reply->readAll(),
                    &parseError);

            reply->deleteLater();

            if (parseError.error
                    != QJsonParseError::NoError
                || !document.isObject()) {
                emit statusChanged(
                    false,
                    QStringLiteral(
                        "Cloudflare API 返回无效 JSON"));
                return;
            }

            emit policyReady(
                document.object().toVariantMap());

            emit statusChanged(
                true,
                QStringLiteral(
                    "Cloudflare API 已连接"));
        });
}
