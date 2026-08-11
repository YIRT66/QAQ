#pragma once

#include <QObject>
#include <QUrl>
#include <QMap>
#include <functional>

namespace HttpFallbackFetcher {

using Success = std::function<void(const QString &, const QUrl &)>;
using Failure = std::function<void(const QString &)>;

bool available();

void fetch(QObject *owner,
           const QUrl &url,
           Success success,
           Failure failure = {},
           const QString &referer = QString(),
           int timeoutMs = 3200,
           const QMap<QString, QString> &extraHeaders = {});

}
