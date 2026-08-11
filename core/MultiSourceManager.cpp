#include "MultiSourceManager.h"

#include <QCryptographicHash>
#include <QRegularExpression>
#include <algorithm>
#include <utility>

MultiSourceManager::MultiSourceManager(QObject *parent) : QObject(parent)
{
    m_searchDeadline.setSingleShot(true);
    m_searchDeadline.setInterval(650);

    connect(&m_searchDeadline, &QTimer::timeout, this, [this] {
        if (m_searchInitialReadyEmitted)
            return;

        // Stop the UI spinner quickly. Web providers are deliberately allowed
        // to continue in the background and can still emit searchProgress later.
        m_searchInitialReadyEmitted = true;
        emit searchReady(rankSearchResults(m_searchMerged));
    });

    m_streamPreferenceTimer.setSingleShot(true);
    m_streamPreferenceTimer.setInterval(180);
    connect(&m_streamPreferenceTimer, &QTimer::timeout, this, [this] {
        if (!m_resolve.parallelStream)
            return;
        if (!m_resolve.provisionalResult.isEmpty())
            finishParallelStreamResult(m_resolve.provisionalResult);
        else
            maybeFinishParallelStream();
    });
}

void MultiSourceManager::addProvider(IMusicProvider *p, bool enabled, int priority)
{
    if (!p || p->providerId().isEmpty()) return;
    const QString id = p->providerId();
    ProviderEntry entry;
    entry.provider = p;
    entry.enabled = enabled;
    entry.priority = priority;
    entry.bundled = (id == "netease" || id == "yueting" || id == "gequhai");
    entry.online = entry.bundled;
    entry.status = entry.bundled ? QStringLiteral("内置源待连接") : QStringLiteral("网关未检测");
    m_providers[id] = entry;
    if (!m_providerOrder.contains(id)) m_providerOrder << id;

    connect(p, &IMusicProvider::searchReady, this, [this, id](const QVariantList &tracks) {
        if (!m_searchWaiting.contains(id))
            return;

        if (!tracks.isEmpty()) {
            m_searchMerged = mergeTracks(m_searchMerged, id, tracks);
            m_searchMerged = rankSearchResults(m_searchMerged);
            emit searchProgress(m_searchMerged);

            // The first useful provider result is enough to make search feel
            // immediate. Slower public-web sources keep enriching afterwards.
            if (!m_searchInitialReadyEmitted) {
                m_searchInitialReadyEmitted = true;
                m_searchDeadline.stop();
                emit searchReady(rankSearchResults(m_searchMerged));
            }
        }

        m_searchWaiting.remove(id);

        if (m_searchWaiting.isEmpty()) {
            m_searchDeadline.stop();
            if (!m_searchInitialReadyEmitted)
                m_searchInitialReadyEmitted = true;

            // Emit again with the final merged set so AppController refreshes
            // its cache after late webpage-source enrichment.
            emit searchReady(rankSearchResults(m_searchMerged));
        }
    });
    connect(p, &IMusicProvider::homeReady, this, [this, id](const QVariantList &playlists) {
        if (!m_homeWaiting.contains(id)) return;
        for (const QVariant &v : playlists) {
            QVariantMap row = v.toMap();
            row["sourceId"] = row.value("id").toString();
            row["providerId"] = id;
            row["providerName"] = providerName(id);
            row["id"] = id + ":" + row.value("sourceId").toString();
            m_homeMerged << row;
        }
        m_homeWaiting.remove(id);
        if (m_homeWaiting.isEmpty()) emit homeReady(m_homeMerged);
    });
    connect(p, &IMusicProvider::playlistReady, this, [this, id](const QVariantMap &playlist, const QVariantList &tracks) {
        QVariantMap pmap = playlist;
        pmap["sourceId"] = pmap.value("id").toString();
        pmap["providerId"] = id;
        pmap["providerName"] = providerName(id);
        pmap["id"] = id + ":" + pmap.value("sourceId").toString();
        QVariantList annotated;
        annotated.reserve(tracks.size());
        for (const QVariant &v : tracks) annotated << annotateTrack(id, v.toMap());
        emit playlistReady(pmap, annotated);
    });
    connect(p, &IMusicProvider::streamUrlReady, this,
            [this, id](const QString &songId, const QString &url) {
        if (m_resolve.download)
            return;

        if (m_resolve.parallelStream) {
            if (!m_resolve.pendingProviderIds.contains(id))
                return;
            const QString expected = m_resolve.expectedSourceIds.value(id);
            if (!expected.isEmpty() && songId != expected)
                return;

            m_resolve.pendingProviderIds.remove(id);
            if (url.trimmed().isEmpty()) {
                maybeFinishParallelStream();
                return;
            }

            QVariantMap result{
                {QStringLiteral("url"), url.trimmed()},
                {QStringLiteral("providerId"), id},
                {QStringLiteral("providerName"), providerName(id)},
                {QStringLiteral("access"),
                 m_resolve.accessByProvider.value(id, QStringLiteral("free"))}
            };

            // If the official NetEase source wins, use it immediately. If a
            // fallback source wins first, keep only a tiny 180 ms grace window
            // for NetEase so a near-tie still prefers the official route.
            if (id == QStringLiteral("netease")) {
                finishParallelStreamResult(result);
                return;
            }

            if (m_resolve.provisionalResult.isEmpty())
                m_resolve.provisionalResult = result;

            if (m_resolve.pendingProviderIds.contains(QStringLiteral("netease"))) {
                if (!m_streamPreferenceTimer.isActive())
                    m_streamPreferenceTimer.start();
                return;
            }

            finishParallelStreamResult(m_resolve.provisionalResult);
            return;
        }

        if (id != m_resolve.activeProviderId
            || songId != m_resolve.activeSourceId)
            return;

        if (url.isEmpty()) {
            startNextResolve();
            return;
        }

        QString access = QStringLiteral("free");
        if (!m_resolve.lookupPhase) {
            const QVariantMap source = m_resolve.candidates.value(m_resolve.index).toMap();
            access = source.value("access", "unknown").toString();
        }

        emit streamReady(m_resolve.canonicalId, url, id, providerName(id), access);
        m_resolve = {};
    });
    connect(p, &IMusicProvider::downloadUrlReady, this, [this, id](const QString &songId, const QString &url) {
        if (!m_resolve.download || id != m_resolve.activeProviderId || songId != m_resolve.activeSourceId) return;
        if (url.isEmpty()) { startNextResolve(); return; }
        emit downloadReady(m_resolve.canonicalId, url, id, providerName(id));
        m_resolve = {};
    });
    connect(p, &IMusicProvider::lyricsReady, this, [this, id](const QString &songId, const QString &lyric, const QString &translated) {
        if (m_lyricsIndex < 0 || m_lyricsIndex >= m_lyricsCandidates.size()) return;
        const QVariantMap candidate = m_lyricsCandidates.at(m_lyricsIndex).toMap();
        if (candidate.value("providerId").toString() != id || candidate.value("sourceId").toString() != songId) return;
        if (lyric.trimmed().isEmpty()) { startNextLyricsCandidate(); return; }
        emit lyricsReady(m_lyricsTrack.value("id").toString(), lyric, translated);
        m_lyricsIndex = -1;
        m_lyricsCandidates.clear();
        m_lyricsTrack.clear();
    });
    connect(p, &IMusicProvider::pingReady, this, [this, id](bool ok, const QString &message) {
        if (m_providers.contains(id)) {
            m_providers[id].online = ok;
            m_providers[id].status = message;
            emit providerStateChanged();
        }
        emit pingReady(id, ok, message);
    });
    connect(p, &IMusicProvider::authStateReady, this, [this](bool, const QString &, const QString &) {
        emit providerStateChanged();
    });
    connect(p, &IMusicProvider::errorOccurred, this, [this, id](const QString &message) {
        // During automatic stream/download fallback, a provider failure should not
        // interrupt playback if another legal source may still work.
        if (!m_resolve.canonicalId.isEmpty() && m_resolve.parallelStream
            && m_resolve.pendingProviderIds.contains(id)) {
            m_resolve.lastError = message;
            m_resolve.pendingProviderIds.remove(id);
            maybeFinishParallelStream();
            return;
        }
        if (!m_resolve.canonicalId.isEmpty() && m_resolve.activeProviderId == id) {
            m_resolve.lastError = message;
            startNextResolve();
            return;
        }
        if (m_lyricsIndex >= 0 && m_lyricsIndex < m_lyricsCandidates.size()
            && m_lyricsCandidates.at(m_lyricsIndex).toMap().value("providerId").toString() == id) {
            startNextLyricsCandidate();
            return;
        }
        if (m_searchWaiting.contains(id)) {
            m_searchWaiting.remove(id);
            if (m_searchWaiting.isEmpty()) {
                m_searchDeadline.stop();
                if (!m_searchInitialReadyEmitted)
                    m_searchInitialReadyEmitted = true;
                emit searchReady(rankSearchResults(m_searchMerged));
            }
            return;
        }
        if (m_homeWaiting.contains(id)) {
            m_homeWaiting.remove(id);
            if (m_homeWaiting.isEmpty()) emit homeReady(m_homeMerged);
            return;
        }
        emit errorOccurred(message);
    });
}

QVariantList MultiSourceManager::providerStates() const
{
    QVariantList out;
    QList<QString> ids = m_providerOrder;
    std::sort(ids.begin(), ids.end(), [this](const QString &a, const QString &b) {
        return m_providers.value(a).priority < m_providers.value(b).priority;
    });
    for (const QString &id : ids) {
        const auto e = m_providers.value(id);
        if (!e.provider) continue;
        out << QVariantMap{
            {"id", id},
            {"name", e.provider->displayName()},
            {"enabled", e.enabled},
            {"priority", e.priority},
            {"baseUrl", e.provider->baseUrl()},
            {"online", e.online},
            {"status", e.status},
            {"bundled", e.bundled},
            {"usable", providerUsable(id)},
            {"loggedIn", e.provider->isAuthenticated()},
            {"webLoginSupported", e.provider->supportsWebLogin()},
            {"onDemandResolve", e.provider->supportsOnDemandResolve()},
            {"requiresPing", e.provider->requiresSuccessfulPing()}
        };
    }
    return out;
}

QString MultiSourceManager::providerBaseUrl(const QString &providerId) const
{
    if (auto *p = provider(providerId)) return p->baseUrl();
    return {};
}

bool MultiSourceManager::providerEnabled(const QString &providerId) const
{
    return m_providers.contains(providerId) && m_providers.value(providerId).enabled;
}

bool MultiSourceManager::providerUsable(const QString &providerId) const
{
    if (!m_providers.contains(providerId)) return false;
    const auto e = m_providers.value(providerId);
    if (!e.enabled || !e.provider) return false;

    // Public web providers use short request-level timeouts and can still work
    // when a lightweight home-page ping is blocked by the site/CDN. Gateways
    // and the local NetEase service still require a successful health check.
    return e.online || !e.provider->requiresSuccessfulPing();
}

void MultiSourceManager::setProviderEnabled(const QString &providerId, bool enabled)
{
    if (!m_providers.contains(providerId) || m_providers[providerId].enabled == enabled) return;
    m_providers[providerId].enabled = enabled;
    if (enabled) {
        m_providers[providerId].status = QStringLiteral("正在检测连接…");
        m_providers[providerId].provider->ping();
    }
    emit providerStateChanged();
}

void MultiSourceManager::setProviderBaseUrl(const QString &providerId, const QString &url)
{
    if (auto *p = provider(providerId)) {
        p->setBaseUrl(url);
        if (m_providers.contains(providerId) && !m_providers[providerId].bundled) {
            m_providers[providerId].online = false;
            m_providers[providerId].status = QStringLiteral("地址已修改，等待测试");
        }
        emit providerStateChanged();
    }
}

void MultiSourceManager::setProviderPriority(const QString &providerId, int priority)
{
    if (!m_providers.contains(providerId)) return;
    m_providers[providerId].priority = priority;
    emit providerStateChanged();
}

void MultiSourceManager::search(const QString &keywords, int limit)
{
    searchProviders(keywords, {}, limit);
}

void MultiSourceManager::searchProviders(const QString &keywords,
                                         const QStringList &providerIds,
                                         int limit)
{
    m_searchDeadline.stop();
    m_searchMerged.clear();
    m_searchWaiting.clear();
    m_searchInitialReadyEmitted = false;
    m_searchQuery = keywords.trimmed().simplified();

    for (const QString &id : m_providerOrder) {
        if (!providerIds.isEmpty() && !providerIds.contains(id))
            continue;

        const auto e = m_providers.value(id);
        if (!providerUsable(id))
            continue;

        m_searchWaiting.insert(id);
        e.provider->search(keywords, limit);
    }

    if (m_searchWaiting.isEmpty()) {
        m_searchInitialReadyEmitted = true;
        emit searchReady({});
        return;
    }

    // Slower HTTP providers are background enrichment and must not hold the
    // visible search spinner hostage.
    m_searchDeadline.start();
}

void MultiSourceManager::loadHome(int limit)
{
    m_homeMerged.clear();
    m_homeWaiting.clear();
    for (const QString &id : m_providerOrder) {
        const auto e = m_providers.value(id);
        if (!providerUsable(id)) continue;
        m_homeWaiting.insert(id);
        e.provider->loadHome(limit);
    }
    if (m_homeWaiting.isEmpty()) emit homeReady({});
}

void MultiSourceManager::loadPlaylist(const QString &providerId, const QString &playlistId, int limit)
{
    if (!providerUsable(providerId)) {
        emit errorOccurred("歌单来源当前不可用：" + providerId);
        return;
    }
    if (auto *p = provider(providerId)) p->loadPlaylist(playlistId, limit);
    else emit errorOccurred("歌单来源不可用：" + providerId);
}

QString MultiSourceManager::normalize(const QString &text)
{
    QString s = text.toLower().simplified();
    s.remove(QRegularExpression(R"([\s\p{P}\p{S}]+)"));
    return s;
}

QString MultiSourceManager::canonicalIdFor(const QVariantMap &track)
{
    const QString basis = normalize(track.value("title").toString()) + "|" + normalize(track.value("artist").toString());
    return QString::fromLatin1(QCryptographicHash::hash(basis.toUtf8(), QCryptographicHash::Sha1).toHex().left(20));
}

int MultiSourceManager::accessRank(const QString &access)
{
    const QString a = access.toLower();
    if (a == "free") return 0;
    if (a == "account") return 1;
    if (a == "unknown") return 2;
    if (a == "restricted") return 3;
    if (a == "vip") return 4;
    return 2;
}

QVariantMap MultiSourceManager::annotateTrack(const QString &providerId, const QVariantMap &track) const
{
    QVariantMap out = track;
    const QString sourceId = track.value("sourceId", track.value("id")).toString();
    const QString access = track.value("access", "unknown").toString().toLower();
    QVariantMap source{
        {"providerId", providerId},
        {"providerName", providerName(providerId)},
        {"sourceId", sourceId},
        {"access", access},
        {"playable", track.value("playable", true).toBool()}
    };
    out["sourceId"] = sourceId;
    out["providerId"] = providerId;
    out["providerName"] = providerName(providerId);
    out["access"] = access;
    out["sources"] = QVariantList{source};
    out["sourceCount"] = 1;
    out["sourceSummary"] = providerName(providerId);
    out["id"] = canonicalIdFor(out);
    return out;
}

QVariantList MultiSourceManager::mergeTracks(const QVariantList &existing, const QString &providerId, const QVariantList &incoming) const
{
    QVariantList merged = existing;
    for (const QVariant &raw : incoming) {
        const QVariantMap candidate = annotateTrack(providerId, raw.toMap());
        const QString titleKey = normalize(candidate.value("title").toString());
        const QString artistKey = normalize(candidate.value("artist").toString());
        const qint64 duration = candidate.value("duration").toLongLong();
        int matchIndex = -1;
        for (int i = 0; i < merged.size(); ++i) {
            const QVariantMap current = merged.at(i).toMap();
            const QString currentTitle = normalize(current.value("title").toString());
            const QString currentArtist = normalize(current.value("artist").toString());
            if (currentTitle != titleKey)
                continue;

            // Public website result pages sometimes omit the singer from the
            // anchor itself.  Treat an empty singer as "unknown" instead of a
            // different recording, but remain conservative when both sides
            // contain singer metadata.
            const bool artistCompatible = artistKey.isEmpty() || currentArtist.isEmpty()
                || artistKey == currentArtist;
            if (!artistCompatible)
                continue;

            const qint64 currentDuration = current.value("duration").toLongLong();
            if (duration <= 0 || currentDuration <= 0 || qAbs(duration - currentDuration) <= 2500) {
                matchIndex = i;
                break;
            }
        }
        if (matchIndex < 0) {
            merged << candidate;
            continue;
        }

        QVariantMap current = merged.at(matchIndex).toMap();
        QVariantList sources = current.value("sources").toList();
        const QVariantMap newSource = candidate.value("sources").toList().first().toMap();
        bool duplicate = false;
        for (const QVariant &v : sources) {
            const QVariantMap s = v.toMap();
            if (s.value("providerId") == newSource.value("providerId") && s.value("sourceId") == newSource.value("sourceId")) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            sources << newSource;

        std::sort(
            sources.begin(),
            sources.end(),
            [this](const QVariant &a,
                   const QVariant &b) {
                const QVariantMap aa = a.toMap();
                const QVariantMap bb = b.toMap();

                const int ap =
                    m_providers
                        .value(
                            aa.value(
                                "providerId")
                                .toString())
                        .priority;

                const int bp =
                    m_providers
                        .value(
                            bb.value(
                                "providerId")
                                .toString())
                        .priority;

                if (ap != bp)
                    return ap < bp;

                return accessRank(
                           aa.value(
                               "access",
                               "unknown")
                               .toString())
                    < accessRank(
                           bb.value(
                               "access",
                               "unknown")
                               .toString());
            });

        current["sources"] = sources;
        current["sourceCount"] = sources.size();

        const QString currentProviderId =
            current.value("providerId").toString();

        const int currentPriority =
            m_providers.contains(currentProviderId)
                ? m_providers
                      .value(currentProviderId)
                      .priority
                : std::numeric_limits<int>::max();

        const int incomingPriority =
            m_providers.contains(providerId)
                ? m_providers
                      .value(providerId)
                      .priority
                : std::numeric_limits<int>::max();

        // Search providers return asynchronously. If NetEase arrives after a
        // public fallback source, promote the higher-priority NetEase metadata
        // instead of leaving the first responder as the visible primary source.
        if (incomingPriority < currentPriority) {
            const QStringList textKeys{
                QStringLiteral("title"),
                QStringLiteral("artist"),
                QStringLiteral("album"),
                QStringLiteral("cover"),
                QStringLiteral("mvId")
            };

            for (const QString &key : textKeys) {
                const QString value =
                    candidate.value(key).toString();

                if (!value.isEmpty())
                    current[key] = value;
            }

            if (candidate.value("duration")
                    .toLongLong() > 0) {
                current["duration"] =
                    candidate.value("duration");
            }

            current["providerId"] =
                candidate.value("providerId");
            current["providerName"] =
                candidate.value("providerName");
            current["sourceId"] =
                candidate.value("sourceId");
            current["access"] =
                candidate.value("access");
            current["playable"] =
                candidate.value("playable", true);
        }

        QStringList names;

        for (const QVariant &v : sources) {
            const QString name =
                v.toMap()
                    .value("providerName")
                    .toString();

            if (!names.contains(name))
                names << name;
        }

        current["sourceSummary"] =
            names.join(QStringLiteral(" · "));

        merged[matchIndex] = current;
    }
    return merged;
}


static int sourceVersionPriority(const QVariantMap &track, const QString &query)
{
    const QString title = track.value(QStringLiteral("title")).toString().toCaseFolded();
    const QString artist = track.value(QStringLiteral("artist")).toString().toCaseFolded();
    const QString q = query.toCaseFolded();

    const bool dj = title.contains(QRegularExpression(QStringLiteral(R"((^|[\s(\[【-])(dj|remix|mix|bootleg|club|电音)([\s)\]】_-]|$))"),
                                                      QRegularExpression::CaseInsensitiveOption))
                    || artist.contains(QStringLiteral("dj"));
    const bool cover = title.contains(QStringLiteral("翻唱"))
                       || title.contains(QStringLiteral("cover"))
                       || title.contains(QStringLiteral("网友改编"));
    const bool accompaniment = title.contains(QStringLiteral("伴奏"))
                               || title.contains(QStringLiteral("纯音乐"))
                               || title.contains(QStringLiteral("instrumental"))
                               || title.contains(QStringLiteral("ktv"));
    const bool medley = title.contains(QLatin1Char('+'))
                        || title.contains(QStringLiteral("串烧"))
                        || title.contains(QStringLiteral("medley"));
    const bool live = title.contains(QStringLiteral("live"))
                      || title.contains(QStringLiteral("现场"))
                      || title.contains(QStringLiteral("演唱会"));

    const bool wantsDj = q.contains(QStringLiteral("dj")) || q.contains(QStringLiteral("remix"));
    const bool wantsCover = q.contains(QStringLiteral("翻唱")) || q.contains(QStringLiteral("cover"));
    const bool wantsAccompaniment = q.contains(QStringLiteral("伴奏")) || q.contains(QStringLiteral("纯音乐"));
    const bool wantsLive = q.contains(QStringLiteral("live")) || q.contains(QStringLiteral("现场")) || q.contains(QStringLiteral("演唱会"));

    if (wantsDj) return dj ? 1300 : 250;
    if (wantsCover) return cover ? 1300 : 250;
    if (wantsAccompaniment) return accompaniment ? 1300 : 150;
    if (wantsLive) return live ? 1300 : 220;
    if (accompaniment) return -850;
    if (medley) return -620;
    if (cover) return 120;
    if (live) return 260;
    if (dj) return 430;
    return 900;
}

QVariantList MultiSourceManager::rankSearchResults(const QVariantList &rows) const
{
    QVariantList ranked = rows;
    const QString normalizedQuery = normalize(m_searchQuery);
    const QStringList queryWords = m_searchQuery.toCaseFolded().split(
        QRegularExpression(QStringLiteral(R"([\s\p{P}\p{S}]+)")),
        Qt::SkipEmptyParts);

    auto score = [this, &normalizedQuery, &queryWords](const QVariantMap &track) {
        const QString title = normalize(track.value(QStringLiteral("title")).toString());
        const QString artist = normalize(track.value(QStringLiteral("artist")).toString());
        const QString album = normalize(track.value(QStringLiteral("album")).toString());
        int value = 0;

        if (!normalizedQuery.isEmpty()) {
            if (title == normalizedQuery) value += 10000;
            else if (title.startsWith(normalizedQuery)) value += 7000;
            else if (title.contains(normalizedQuery)) value += 5200;

            if (artist == normalizedQuery) value += 4200;
            else if (artist.contains(normalizedQuery)) value += 2500;

            if (album == normalizedQuery) value += 1400;
            else if (album.contains(normalizedQuery)) value += 700;
        }

        for (const QString &rawWord : queryWords) {
            const QString word = normalize(rawWord);
            if (word.size() < 2) continue;
            if (title.contains(word)) value += 420;
            if (artist.contains(word)) value += 260;
            if (album.contains(word)) value += 90;
        }

        value += sourceVersionPriority(track, m_searchQuery);
        value += qMin(4, track.value(QStringLiteral("sourceCount"), 1).toInt()) * 15;
        const QString providerId = track.value(QStringLiteral("providerId")).toString();
        if (providerId == QStringLiteral("netease")) value += 30;
        return value;
    };

    std::stable_sort(ranked.begin(), ranked.end(), [this, &score](const QVariant &a, const QVariant &b) {
        const QVariantMap aa = a.toMap();
        const QVariantMap bb = b.toMap();
        const int as = score(aa);
        const int bs = score(bb);
        if (as != bs) return as > bs;
        const int ap = m_providers.value(aa.value(QStringLiteral("providerId")).toString()).priority;
        const int bp = m_providers.value(bb.value(QStringLiteral("providerId")).toString()).priority;
        return ap < bp;
    });
    return ranked;
}

QVariantList MultiSourceManager::sortedCandidates(const QVariantMap &track) const
{
    QVariantList candidates = track.value("sources").toList();
    if (candidates.isEmpty() && track.contains("providerId")) {
        candidates << QVariantMap{
            {"providerId", track.value("providerId")},
            {"providerName", track.value("providerName")},
            {"sourceId", track.value("sourceId", track.value("id"))},
            {"access", track.value("access", "unknown")},
            {"playable", track.value("playable", true)}
        };
    }
    std::sort(
        candidates.begin(),
        candidates.end(),
        [this](const QVariant &a,
               const QVariant &b) {
            const QVariantMap aa = a.toMap();
            const QVariantMap bb = b.toMap();

            const int ap =
                m_providers
                    .value(
                        aa.value("providerId")
                            .toString())
                    .priority;

            const int bp =
                m_providers
                    .value(
                        bb.value("providerId")
                            .toString())
                    .priority;

            // Provider priority wins first. NetEase is locked to priority 0
            // in AppController, so the current local profile's own NetEase
            // entitlement is tried before fallback sources.
            if (ap != bp)
                return ap < bp;

            const int ar =
                accessRank(
                    aa.value(
                        "access",
                        "unknown")
                        .toString());

            const int br =
                accessRank(
                    bb.value(
                        "access",
                        "unknown")
                        .toString());

            return ar < br;
        });
    return candidates;
}

IMusicProvider *MultiSourceManager::provider(const QString &providerId) const
{
    return m_providers.contains(providerId) ? m_providers.value(providerId).provider : nullptr;
}

QString MultiSourceManager::providerName(const QString &providerId) const
{
    if (auto *p = provider(providerId)) return p->displayName();
    return providerId;
}

void MultiSourceManager::resolveBestStream(const QVariantMap &track,
                                           const QString &quality,
                                           const QStringList &excludedProviderIds)
{
    m_streamPreferenceTimer.stop();
    m_resolve = {};
    m_resolve.canonicalId = track.value("id").toString();
    m_resolve.quality = quality;
    m_resolve.track = track;
    m_resolve.candidates = sortedCandidates(track);
    m_resolve.excludedProviderIds = excludedProviderIds;
    m_resolve.download = false;
    m_resolve.parallelStream = true;
    startParallelStreamResolve();
}

void MultiSourceManager::startParallelStreamResolve()
{
    if (m_resolve.canonicalId.isEmpty()) {
        emit streamResolveFailed(QString(), QStringLiteral("歌曲标识为空，无法解析音源"));
        m_resolve = {};
        return;
    }

    // Keep at most one concrete candidate per provider. The sorted list already
    // reflects provider/access preference, so the first candidate for a provider
    // is the best one to race.
    QHash<QString, QVariantMap> concrete;
    for (const QVariant &value : std::as_const(m_resolve.candidates)) {
        const QVariantMap source = value.toMap();
        const QString providerId = source.value("providerId").toString();
        if (providerId.isEmpty() || concrete.contains(providerId)
            || m_resolve.excludedProviderIds.contains(providerId)
            || !source.value("playable", true).toBool()
            || !providerUsable(providerId)) {
            continue;
        }
        IMusicProvider *p = provider(providerId);
        if (!p)
            continue;
        const QString access = source.value("access", "unknown").toString().toLower();
        if ((access == "account" || access == "vip"
             || (providerId == QStringLiteral("netease") && access == "restricted"))
            && !p->isAuthenticated()) {
            continue;
        }
        concrete.insert(providerId, source);
    }

    struct DispatchItem {
        QString providerId;
        QString sourceId;
        bool lookup = false;
    };
    QList<DispatchItem> dispatch;

    // Register every pending provider BEFORE dispatching anything. Some web
    // providers can resolve an already-known page synchronously; without this
    // pre-registration the first synchronous fallback could win before NetEase
    // had even been marked as pending.
    for (auto it = concrete.cbegin(); it != concrete.cend(); ++it) {
        const QString providerId = it.key();
        const QVariantMap source = it.value();
        const QString sourceId = source.value("sourceId").toString();
        if (sourceId.isEmpty())
            continue;
        m_resolve.pendingProviderIds.insert(providerId);
        m_resolve.expectedSourceIds.insert(providerId, sourceId);
        m_resolve.accessByProvider.insert(
            providerId, source.value("access", "unknown").toString());
        dispatch.append({providerId, sourceId, false});
    }

    for (const QString &providerId : std::as_const(m_providerOrder)) {
        if (concrete.contains(providerId)
            || m_resolve.excludedProviderIds.contains(providerId)
            || !providerUsable(providerId)) {
            continue;
        }
        IMusicProvider *p = provider(providerId);
        if (!p || !p->supportsOnDemandResolve())
            continue;
        m_resolve.pendingProviderIds.insert(providerId);
        m_resolve.expectedSourceIds.insert(providerId, m_resolve.canonicalId);
        m_resolve.accessByProvider.insert(providerId, QStringLiteral("free"));
        dispatch.append({providerId, m_resolve.canonicalId, true});
    }

    if (m_resolve.pendingProviderIds.isEmpty()) {
        const QString canonicalId = m_resolve.canonicalId;
        m_resolve = {};
        emit streamResolveFailed(canonicalId, QStringLiteral("当前没有可用的播放来源。"));
        return;
    }

    // Queue each start to the event loop to avoid re-entrant provider signals
    // while this setup function is still building the race state.
    for (const DispatchItem &item : std::as_const(dispatch)) {
        QTimer::singleShot(0, this, [this, item] {
            if (!m_resolve.parallelStream
                || !m_resolve.pendingProviderIds.contains(item.providerId)) {
                return;
            }
            IMusicProvider *p = provider(item.providerId);
            if (!p) {
                m_resolve.pendingProviderIds.remove(item.providerId);
                maybeFinishParallelStream();
                return;
            }
            if (item.lookup)
                p->resolveTrack(m_resolve.track, m_resolve.quality);
            else
                p->resolveSongUrl(item.sourceId, m_resolve.quality);
        });
    }
}

void MultiSourceManager::finishParallelStreamResult(const QVariantMap &result)
{
    if (!m_resolve.parallelStream || result.isEmpty())
        return;

    const QString canonicalId = m_resolve.canonicalId;
    const QString url = result.value(QStringLiteral("url")).toString();
    const QString providerId = result.value(QStringLiteral("providerId")).toString();
    const QString providerNameValue = result.value(
        QStringLiteral("providerName"), providerName(providerId)).toString();
    const QString access = result.value(
        QStringLiteral("access"), QStringLiteral("unknown")).toString();

    m_streamPreferenceTimer.stop();
    m_resolve = {};
    emit streamReady(canonicalId, url, providerId, providerNameValue, access);
}

void MultiSourceManager::maybeFinishParallelStream()
{
    if (!m_resolve.parallelStream)
        return;

    if (!m_resolve.provisionalResult.isEmpty()
        && !m_resolve.pendingProviderIds.contains(QStringLiteral("netease"))) {
        finishParallelStreamResult(m_resolve.provisionalResult);
        return;
    }

    if (!m_resolve.pendingProviderIds.isEmpty())
        return;

    if (!m_resolve.provisionalResult.isEmpty()) {
        finishParallelStreamResult(m_resolve.provisionalResult);
        return;
    }

    const QString canonicalId = m_resolve.canonicalId;
    const QString reason = m_resolve.lastError;
    m_streamPreferenceTimer.stop();
    m_resolve = {};
    emit streamResolveFailed(
        canonicalId,
        reason.isEmpty()
            ? QStringLiteral("所有已启用的音源都没有返回可播放地址。")
            : QStringLiteral("所有音源并发解析均失败：") + reason);
}

void MultiSourceManager::resolveBestDownload(const QVariantMap &track, const QString &quality)
{
    m_resolve = {};
    m_resolve.canonicalId = track.value("id").toString();
    m_resolve.quality = quality;
    m_resolve.track = track;
    m_resolve.candidates = sortedCandidates(track);
    m_resolve.download = true;
    startNextResolve();
}

void MultiSourceManager::startNextResolve()
{
    if (m_resolve.lookupPhase) {
        startNextLookupResolve();
        return;
    }

    while (++m_resolve.index < m_resolve.candidates.size()) {
        const QVariantMap source =
            m_resolve.candidates.at(m_resolve.index).toMap();
        const QString providerId =
            source.value("providerId").toString();

        if (m_resolve.excludedProviderIds.contains(providerId)
            || !source.value("playable", true).toBool()
            || !providerUsable(providerId))
            continue;

        IMusicProvider *p = provider(providerId);
        if (!p)
            continue;

        const QString access =
            source.value("access", "unknown").toString().toLower();

        // Account-only candidates are skipped until that provider has a valid
        // login session. This avoids requests that cannot legally succeed.
        if ((access == "account"
             || access == "vip"
             || (providerId == QStringLiteral("netease")
                 && access == "restricted"))
            && !p->isAuthenticated()) {
            continue;
        }

        m_resolve.activeProviderId = providerId;
        m_resolve.activeSourceId =
            source.value("sourceId").toString();

        if (m_resolve.download)
            p->resolveDownloadUrl(m_resolve.activeSourceId,
                                  m_resolve.quality);
        else
            p->resolveSongUrl(m_resolve.activeSourceId,
                              m_resolve.quality);
        return;
    }

    // Stream-only fallback: ask lookup-capable providers to perform a fresh
    // title/artist lookup even when they were absent from the original search
    // result. This decouples playback fallback from fragile HTML search merging.
    if (!m_resolve.download) {
        m_resolve.lookupPhase = true;
        m_resolve.lookupProviderIds.clear();
        m_resolve.lookupIndex = -1;

        QStringList ids = m_providerOrder;
        std::sort(ids.begin(), ids.end(),
                  [this](const QString &a, const QString &b) {
            return m_providers.value(a).priority
                < m_providers.value(b).priority;
        });

        for (const QString &id : ids) {
            IMusicProvider *p = provider(id);
            if (m_resolve.excludedProviderIds.contains(id)
                || !p
                || !providerUsable(id)
                || !p->supportsOnDemandResolve())
                continue;
            m_resolve.lookupProviderIds << id;
        }

        startNextLookupResolve();
        return;
    }

    const QString reason = m_resolve.lastError;
    m_resolve = {};
    emit errorOccurred(
        reason.isEmpty()
            ? QStringLiteral("没有找到当前账号可合法下载的来源。")
            : QStringLiteral("已尝试所有下载来源：") + reason);
}

void MultiSourceManager::startNextLookupResolve()
{
    while (++m_resolve.lookupIndex
           < m_resolve.lookupProviderIds.size()) {
        const QString providerId =
            m_resolve.lookupProviderIds.at(m_resolve.lookupIndex);
        IMusicProvider *p = provider(providerId);

        if (!p
            || !providerUsable(providerId)
            || !p->supportsOnDemandResolve())
            continue;

        m_resolve.activeProviderId = providerId;

        // lookup-capable providers emit streamUrlReady() using the canonical
        // track id as the correlation id.
        m_resolve.activeSourceId = m_resolve.canonicalId;
        p->resolveTrack(m_resolve.track, m_resolve.quality);
        return;
    }

    const QString canonicalId = m_resolve.canonicalId;
    const QString reason = m_resolve.lastError;
    m_resolve = {};
    emit streamResolveFailed(
        canonicalId,
        reason.isEmpty()
            ? QStringLiteral("所有已启用的备用音源都没有返回可播放地址。")
            : QStringLiteral("已尝试所有备用音源，仍无法播放：")
                  + reason);
}

void MultiSourceManager::loadBestLyrics(const QVariantMap &track)
{
    m_lyricsTrack = track;
    m_lyricsCandidates = sortedCandidates(track);
    m_lyricsIndex = -1;
    startNextLyricsCandidate();
}

void MultiSourceManager::startNextLyricsCandidate()
{
    while (++m_lyricsIndex < m_lyricsCandidates.size()) {
        const QVariantMap source = m_lyricsCandidates.at(m_lyricsIndex).toMap();
        const QString providerId = source.value("providerId").toString();
        if (!providerUsable(providerId)) continue;
        if (auto *p = provider(providerId)) {
            p->loadLyrics(source.value("sourceId").toString());
            return;
        }
    }
    emit lyricsReady(m_lyricsTrack.value("id").toString(), QString(), QString());
    m_lyricsIndex = -1;
    m_lyricsCandidates.clear();
    m_lyricsTrack.clear();
}

void MultiSourceManager::ping(const QString &providerId)
{
    if (auto *p = provider(providerId)) {
        if (m_providers.contains(providerId)) {
            m_providers[providerId].status = QStringLiteral("正在检测…");
            emit providerStateChanged();
        }
        p->ping();
    } else {
        emit pingReady(providerId, false, "未找到该音乐源");
    }
}

void MultiSourceManager::pingAll()
{
    for (const QString &id : m_providerOrder) {
        if (!m_providers.value(id).enabled) continue;
        ping(id);
    }
}
