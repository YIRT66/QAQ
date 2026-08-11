#pragma once

#include <QObject>
#include <QVariantList>
#include <functional>

namespace PublicSiteIndex {

using ResultsCallback = std::function<void(const QVariantList &)>;

// Result rows:
// {
//   "url": "https://target/song/123.html",
//   "sourceId": "123",
//   "title": "search-index title",
//   "description": "..."
// }
void search(QObject *owner,
            const QString &domain,
            const QString &pathPrefix,
            const QString &keywords,
            int limit,
            ResultsCallback callback);

}
