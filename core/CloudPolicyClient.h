#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QVariantMap>

class CloudPolicyClient final : public QObject
{
    Q_OBJECT

public:
    explicit CloudPolicyClient(QObject *parent = nullptr);

    void setApiBaseUrl(const QString &url);
    QString apiBaseUrl() const { return m_apiBaseUrl; }

    void refresh();

signals:
    void policyReady(const QVariantMap &policy);
    void statusChanged(bool online, const QString &message);

private:
    bool transportAllowed(const QUrl &url) const;

    QString m_apiBaseUrl;
    QNetworkAccessManager m_network;
};
