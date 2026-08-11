#pragma once

#include <QQmlNetworkAccessManagerFactory>

class QNetworkAccessManager;

class CachedNetworkAccessManagerFactory final : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override;
};
