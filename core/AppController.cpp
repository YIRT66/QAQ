#include "AppController.h"

#include <QCoreApplication>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QAudioSource>
#include <QBuffer>
#include <QGuiApplication>
#include <QImage>
#include <QFontDatabase>
#include <QFont>
#include <QDateTime>
#include <QDir>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaDevices>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>
#include <limits>
#include <utility>

namespace {
QString normalizedSearchPart(QString value)
{
    value = value.toCaseFolded().simplified();
    value.remove(QRegularExpression(QStringLiteral(R"([\s\p{P}\p{S}]+)")));
    return value;
}


int versionPriorityScore(const QVariantMap &track, const QString &query)
{
    const QString rawTitle = track.value(QStringLiteral("title")).toString().toCaseFolded();
    const QString rawArtist = track.value(QStringLiteral("artist")).toString().toCaseFolded();
    const QString rawAlbum = track.value(QStringLiteral("album")).toString().toCaseFolded();
    const QString q = query.toCaseFolded();

    const bool dj = rawTitle.contains(QRegularExpression(QStringLiteral(R"((^|[\s(\[【-])(dj|remix|mix|bootleg|club|电音)([\s)\]】_-]|$))"),
                                                        QRegularExpression::CaseInsensitiveOption))
                    || rawArtist.contains(QStringLiteral("dj"));
    const bool cover = rawTitle.contains(QStringLiteral("翻唱"))
                       || rawTitle.contains(QStringLiteral("cover"))
                       || rawTitle.contains(QStringLiteral("网友改编"))
                       || rawAlbum.contains(QStringLiteral("翻唱"));
    const bool accompaniment = rawTitle.contains(QStringLiteral("伴奏"))
                               || rawTitle.contains(QStringLiteral("纯音乐"))
                               || rawTitle.contains(QStringLiteral("instrumental"))
                               || rawTitle.contains(QStringLiteral("ktv"));
    const bool medley = rawTitle.contains(QLatin1Char('+'))
                        || rawTitle.contains(QStringLiteral("串烧"))
                        || rawTitle.contains(QStringLiteral("medley"));
    const bool live = rawTitle.contains(QStringLiteral("live"))
                      || rawTitle.contains(QStringLiteral("现场"))
                      || rawTitle.contains(QStringLiteral("演唱会"));

    const bool wantsDj = q.contains(QStringLiteral("dj")) || q.contains(QStringLiteral("remix"));
    const bool wantsCover = q.contains(QStringLiteral("翻唱")) || q.contains(QStringLiteral("cover"));
    const bool wantsAccompaniment = q.contains(QStringLiteral("伴奏")) || q.contains(QStringLiteral("纯音乐"));
    const bool wantsLive = q.contains(QStringLiteral("live")) || q.contains(QStringLiteral("现场")) || q.contains(QStringLiteral("演唱会"));

    if (wantsDj)
        return dj ? 1300 : (cover ? 100 : 350);
    if (wantsCover)
        return cover ? 1300 : (dj ? 180 : 350);
    if (wantsAccompaniment)
        return accompaniment ? 1300 : 150;
    if (wantsLive)
        return live ? 1300 : 220;

    // Default user intent: clean studio/original result first, then DJ/remix,
    // then live/alternate versions, then covers. Accompaniment/medleys stay low.
    if (accompaniment) return -850;
    if (medley) return -620;
    if (cover) return 120;
    if (live) return 260;
    if (dj) return 430;
    return 900;
}

QString searchIdentity(const QVariantMap &track)
{
    return normalizedSearchPart(track.value(QStringLiteral("title")).toString())
        + QLatin1Char('|')
        + normalizedSearchPart(track.value(QStringLiteral("artist")).toString());
}


bool searchRowsCompatible(const QVariantMap &a, const QVariantMap &b)
{
    const QString at = normalizedSearchPart(a.value(QStringLiteral("title")).toString());
    const QString bt = normalizedSearchPart(b.value(QStringLiteral("title")).toString());
    if (at.isEmpty() || bt.isEmpty() || at != bt)
        return false;

    const QString aa = normalizedSearchPart(a.value(QStringLiteral("artist")).toString());
    const QString ba = normalizedSearchPart(b.value(QStringLiteral("artist")).toString());
    if (!aa.isEmpty() && !ba.isEmpty()
        && aa != ba && !aa.contains(ba) && !ba.contains(aa)) {
        return false;
    }

    const qint64 ad = a.value(QStringLiteral("duration")).toLongLong();
    const qint64 bd = b.value(QStringLiteral("duration")).toLongLong();
    return ad <= 0 || bd <= 0 || qAbs(ad - bd) <= 5000;
}

int searchRelevance(const QVariantMap &track, const QString &query)
{
    const QString nq = normalizedSearchPart(query);
    const QString title = normalizedSearchPart(track.value(QStringLiteral("title")).toString());
    const QString artist = normalizedSearchPart(track.value(QStringLiteral("artist")).toString());
    const QString album = normalizedSearchPart(track.value(QStringLiteral("album")).toString());
    int score = 0;

    if (!nq.isEmpty()) {
        if (title == nq) score += 10000;
        else if (title.startsWith(nq)) score += 7000;
        else if (title.contains(nq)) score += 5200;
        if (artist == nq) score += 4200;
        else if (artist.contains(nq)) score += 2500;
        if (album == nq) score += 1400;
        else if (album.contains(nq)) score += 700;
    }

    const QStringList words = query.toCaseFolded().split(
        QRegularExpression(QStringLiteral(R"([\s\p{P}\p{S}]+)")), Qt::SkipEmptyParts);
    for (const QString &rawWord : words) {
        const QString word = normalizedSearchPart(rawWord);
        if (word.size() < 2) continue;
        if (title.contains(word)) score += 420;
        if (artist.contains(word)) score += 260;
        if (album.contains(word)) score += 90;
    }

    score += versionPriorityScore(track, query);
    score += qMin(4, track.value(QStringLiteral("sourceCount"), 1).toInt()) * 15;
    if (track.value(QStringLiteral("providerId")).toString() == QStringLiteral("netease"))
        score += 30;
    return score;
}

QVariantMap sourceDescriptor(const QVariantMap &track)
{
    return QVariantMap{
        {QStringLiteral("providerId"), track.value(QStringLiteral("providerId"))},
        {QStringLiteral("providerName"), track.value(QStringLiteral("providerName"))},
        {QStringLiteral("sourceId"), track.value(QStringLiteral("sourceId"), track.value(QStringLiteral("id")))},
        {QStringLiteral("access"), track.value(QStringLiteral("access"), QStringLiteral("unknown"))},
        {QStringLiteral("playable"), track.value(QStringLiteral("playable"), true)}
    };
}

QVariantList searchSources(const QVariantMap &track)
{
    QVariantList sources = track.value(QStringLiteral("sources")).toList();
    if (sources.isEmpty() && !track.value(QStringLiteral("providerId")).toString().isEmpty())
        sources << sourceDescriptor(track);
    return sources;
}

QVariantList mergeCloudAndWebSearch(const QVariantList &cloudRows,
                                    const QVariantList &webRows,
                                    const QString &query)
{
    QVariantList merged = cloudRows;

    for (const QVariant &raw : webRows) {
        const QVariantMap incoming = raw.toMap();
        const QString incomingKey = searchIdentity(incoming);
        if (incomingKey == QStringLiteral("|"))
            continue;

        int matchIndex = -1;
        for (int i = 0; i < merged.size(); ++i) {
            const QVariantMap current = merged.at(i).toMap();
            if (searchRowsCompatible(current, incoming)) {
                matchIndex = i;
                break;
            }
        }

        if (matchIndex < 0) {
            merged << incoming;
            continue;
        }

        QVariantMap current = merged.at(matchIndex).toMap();
        QVariantList sources = searchSources(current);
        const QVariantList incomingSources = searchSources(incoming);

        for (const QVariant &sourceValue : incomingSources) {
            const QVariantMap source = sourceValue.toMap();
            bool duplicate = false;
            for (const QVariant &existingValue : sources) {
                const QVariantMap existing = existingValue.toMap();
                if (existing.value(QStringLiteral("providerId")) == source.value(QStringLiteral("providerId"))
                    && existing.value(QStringLiteral("sourceId")) == source.value(QStringLiteral("sourceId"))) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
                sources << source;
        }

        if (current.value(QStringLiteral("cover")).toString().isEmpty()
            && !incoming.value(QStringLiteral("cover")).toString().isEmpty()) {
            current[QStringLiteral("cover")] = incoming.value(QStringLiteral("cover"));
        }

        QStringList names;
        for (const QVariant &sourceValue : sources) {
            const QString name = sourceValue.toMap().value(QStringLiteral("providerName")).toString();
            if (!name.isEmpty() && !names.contains(name))
                names << name;
        }

        current[QStringLiteral("sources")] = sources;
        current[QStringLiteral("sourceCount")] = sources.size();
        if (!names.isEmpty())
            current[QStringLiteral("sourceSummary")] = names.join(QStringLiteral(" · "));

        merged[matchIndex] = current;
    }

    std::stable_sort(merged.begin(), merged.end(), [&query](const QVariant &a, const QVariant &b) {
        return searchRelevance(a.toMap(), query) > searchRelevance(b.toMap(), query);
    });
    return merged;
}
}

AppController::AppController(QObject *parent)
    : QObject(parent),
      m_qq("qq", "QQ音乐", "http://127.0.0.1:3200", "https://y.qq.com/"),
      m_kugou("kugou", "酷狗音乐", "http://127.0.0.1:3300", "https://www.kugou.com/"),
      m_ourcraftNetease("netease", QStringLiteral("网易云 · Ourcraft"), QStringLiteral("netease"), QStringLiteral("https://music.yuncan.xyz")),
      m_ourcraftKugou("kugou", QStringLiteral("酷狗 · Ourcraft"), QStringLiteral("kugou"), QStringLiteral("https://music.yuncan.xyz")),
      m_ourcraftKuwo("kuwo", QStringLiteral("酷我 · Ourcraft"), QStringLiteral("kuwo"), QStringLiteral("https://music.yuncan.xyz")),
      m_yueting(),
      m_gequhai(),
      m_settings("EvolveMusic", "EvolveMusic")
{
    m_cloudRouteMode = m_settings.value(QStringLiteral("cloud/routeMode"),
                                        QStringLiteral("auto")).toString().trimmed().toLower();
    if (m_cloudRouteMode != QStringLiteral("primary")
        && m_cloudRouteMode != QStringLiteral("relay")) {
        m_cloudRouteMode = QStringLiteral("auto");
    }

    m_systemFontFamily = QGuiApplication::font().family();
    m_availableFonts = {QStringLiteral("系统默认")};
    const QStringList installed = QFontDatabase::families();
    const QStringList preferredFonts{
        QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei"),
        QStringLiteral("Segoe UI Variable"), QStringLiteral("Segoe UI"),
        QStringLiteral("Noto Sans CJK SC"), QStringLiteral("Noto Sans SC"),
        QStringLiteral("HarmonyOS Sans SC"), QStringLiteral("SimHei"),
        QStringLiteral("Arial")};
    // Put common Chinese/UI fonts first, then expose every installed Windows font.
    // This keeps the selector useful without artificially limiting the user's choice.
    for (const QString &family : preferredFonts) {
        if (installed.contains(family, Qt::CaseInsensitive) && !m_availableFonts.contains(family, Qt::CaseInsensitive))
            m_availableFonts << family;
    }
    QStringList remaining = installed;
    remaining.sort(Qt::CaseInsensitive);
    for (const QString &family : remaining) {
        if (!family.trimmed().isEmpty() && !m_availableFonts.contains(family, Qt::CaseInsensitive))
            m_availableFonts << family;
    }
    if (!m_systemFontFamily.isEmpty() && !m_availableFonts.contains(m_systemFontFamily, Qt::CaseInsensitive))
        m_availableFonts << m_systemFontFamily;

    m_pluginDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                            .filePath(QStringLiteral("plugins"));
    QDir().mkpath(m_pluginDirectory);

    configureProviders();

    m_accounts.setProfileId(
        m_profiles.currentProfileId());

    m_accounts.registerProvider(&m_netease);
    m_accounts.registerProvider(&m_qq);
    m_accounts.registerProvider(&m_kugou);

    openProfileSettings();
    migrateLegacySettingsIfNeeded();
    loadProfilePreferences();
    scanPlugins();

    // Apply the persisted application font once, before QQmlApplicationEngine
    // loads the QML tree. Qt documents QGuiApplication::setFont() as safe for
    // setting the application default before QML is loaded. Re-applying it
    // after a large QML tree exists can recursively invalidate thousands of
    // font bindings on Qt 6.10/Windows, which caused the v0.16.0 stack overflow.
    const QString startupFont = resolvedFontFamily();
    if (!startupFont.isEmpty()) {
        QFont font = QGuiApplication::font();
        font.setFamily(startupFont);
        QGuiApplication::setFont(font);
        qInfo() << "Startup UI font applied before QML:" << startupFont;
    }

    applyCacheEnvironment();
    QDir(togetherTempDirectory()).removeRecursively();
    loadPersistentLists();

    // History persistence is intentionally delayed. Writing an entire JSON list
    // synchronously on every automatic next-track transition can stall the GUI
    // during long listening sessions.
    m_historySaveTimer.setSingleShot(true);
    m_historySaveTimer.setInterval(900);
    connect(&m_historySaveTimer, &QTimer::timeout, this, [this] {
        saveList("library/history", m_history);
    });


    m_cloudStateSaveTimer.setSingleShot(true);
    m_cloudStateSaveTimer.setInterval(700);
    connect(
        &m_cloudStateSaveTimer,
        &QTimer::timeout,
        this,
        [this] {
            if (!m_cloudMusicMode)
                return;

            m_cloudMusic.saveState(
                m_favorites,
                m_history,
                cloudSettingsSnapshot());
        });

    m_togetherPollTimer.setInterval(1400);
    m_togetherPollTimer.setSingleShot(false);
    connect(&m_togetherPollTimer, &QTimer::timeout, this, &AppController::refreshTogetherRoom);

    m_togetherPublishTimer.setInterval(850);
    m_togetherPublishTimer.setSingleShot(false);
    connect(&m_togetherPublishTimer, &QTimer::timeout, this, &AppController::publishTogetherState);

    m_togetherChatPollTimer.setInterval(800);
    m_togetherChatPollTimer.setSingleShot(false);
    connect(&m_togetherChatPollTimer, &QTimer::timeout, this, &AppController::refreshTogetherMessages);

    // Voice uses short compressed PCM frames. HTTP relay is intentionally kept
    // independent from playback synchronization, so a temporary voice failure
    // can never stop or desync the song session.
    m_togetherVoicePollTimer.setInterval(360);
    m_togetherVoicePollTimer.setSingleShot(false);
    connect(&m_togetherVoicePollTimer, &QTimer::timeout, this, &AppController::refreshTogetherVoice);

    // Lightweight social refresh: invitations need to feel immediate, while community
    // playlists and presence can refresh less often to keep Worker/D1 traffic modest.
    m_socialPollTimer.setInterval(1600);
    m_socialPollTimer.setSingleShot(false);
    connect(&m_socialPollTimer, &QTimer::timeout, this, &AppController::refreshSocialState);

    m_presenceTimer.setInterval(10000);
    m_presenceTimer.setSingleShot(false);
    connect(&m_presenceTimer, &QTimer::timeout, this, &AppController::refreshPresence);

    m_communityRefreshTimer.setInterval(8000);
    m_communityRefreshTimer.setSingleShot(false);
    connect(&m_communityRefreshTimer, &QTimer::timeout, this, [this] {
        if (m_cloudMusic.loggedIn()) m_cloudMusic.loadCommunityPlaylists(28);
    });

    // Playback source selection keeps a tiny preference window so parallel
    // Ourcraft/platform candidates can arrive without making the first slow
    // fallback permanently win the race.
    m_fastStreamPreferenceTimer.setSingleShot(true);
    m_fastStreamPreferenceTimer.setInterval(120);
    connect(&m_fastStreamPreferenceTimer, &QTimer::timeout, this, [this] {
        if (!m_fastStreamSettled && !m_fastStreamProvisional.isEmpty())
            commitFastStreamResult(m_fastStreamProvisional);
        else
            maybeFinishFastStreamRace();
    });

    connect(&m_accounts, &AccountManager::toastRequested, this, &AppController::toastRequested);
    connect(&m_accounts, &AccountManager::accountsChanged, this, &AppController::providersChanged);


    // Ourcraft v0.18.0 is sessionless for music playback. Legacy provider
    // browser sessions, if the old local helper is ever opened, stay local and
    // are no longer uploaded to Evolve Cloud.

    connect(&m_profiles,
            &LocalProfileManager::toastRequested,
            this,
            &AppController::toastRequested);

    connect(&m_profiles,
            &LocalProfileManager::profileAboutToChange,
            this,
            [this](const QString &, const QString &) {
        m_cloudStateSaveTimer.stop();

        if (m_cloudMusicMode && m_cloudMusic.ready()) {
            // Start the old profile's cloud save before the client swaps to the
            // next profile token. CloudMusicClient also ignores late replies
            // that belong to a previous profile generation.
            m_cloudMusic.saveState(
                m_favorites,
                m_history,
                cloudSettingsSnapshot());
        }

        saveCurrentProfileState();

        // Do not let the previous user's current song/queue generate history
        // in the next local profile.
        m_player.clearQueue();

        m_searchCache.clear();
        m_pendingSearchKey.clear();
        m_pendingDownloads.clear();
    });

    connect(&m_profiles,
            &LocalProfileManager::currentProfileChanged,
            this,
            [this] {
        m_accounts.setProfileId(
            m_profiles.currentProfileId());

        openProfileSettings();
        migrateLegacySettingsIfNeeded();
        loadProfilePreferences();
        applyCacheEnvironment();
        loadPersistentLists();

        m_searchResults.clear();
        m_searchPlaylists.clear();
        m_searchArtists.clear();
        m_cloudSearchResults.clear();
        m_webSearchResults.clear();
        m_currentPlaylist.clear();
        m_playlistTracks.clear();
        m_lyrics.clear();
        m_activeSourceName.clear();
        m_activeSourceAccess.clear();

        emit searchResultsChanged();
        emit searchCatalogChanged();
        emit currentPlaylistChanged();
        emit playlistTracksChanged();
        emit lyricsChanged();
        emit favoritesChanged();
        emit historyChanged();
        emit activeSourceChanged();

        emit qualityLevelChanged();
        emit playerStyleChanged();
        emit darkModeChanged();
        emit accentColorChanged();
        emit animationsEnabledChanged();
        emit compactTrackRowsChanged();
        emit showSourceBadgesChanged();

        setLoading(false);
        setError(QString());

        if (m_cloudMusicMode) {
            m_cloudMusic.setProfileDataRoot(
                m_profiles.currentProfileDataRoot());
            m_cloudMusic.initialize();
        }
        refreshHome();
        m_sources.pingAll();
    });


    // v0.18.0: Evolve Cloud no longer distributes music-source policy.
    // Ourcraft source order and endpoint are controlled locally in Settings.

    connect(
        &m_cloudMusic,
        &CloudMusicClient::toastRequested,
        this,
        &AppController::toastRequested);
    connect(&m_cloudMusic, &CloudMusicClient::emailCodeSent,
            this, &AppController::emailCodeSent);
    connect(&m_cloudMusic, &CloudMusicClient::passwordResetCompleted,
            this, &AppController::passwordResetCompleted);

    connect(
        &m_cloudMusic,
        &CloudMusicClient::readyChanged,
        this,
        [this] {
            emit cloudStatusChanged();
            // Start realtime/social work only after /v1/auth/me completed.
            // Starting it merely because a persisted token exists can re-enter
            // CloudMusicClient::initialize() through runWhenReady().
            if (!m_cloudMusic.ready() || !m_cloudMusic.loggedIn())
                return;
            refreshSocialState();
            refreshPresence();
            m_cloudMusic.loadCommunityPlaylists(28);
            if (!m_socialPollTimer.isActive()) m_socialPollTimer.start();
            if (!m_presenceTimer.isActive()) m_presenceTimer.start();
            if (!m_communityRefreshTimer.isActive()) m_communityRefreshTimer.start();
        });
    connect(
        &m_cloudMusic,
        &CloudMusicClient::identityChanged,
        this,
        &AppController::cloudStatusChanged);
    connect(
        &m_cloudMusic,
        &CloudMusicClient::baseUrlChanged,
        this,
        &AppController::cloudStatusChanged);
    connect(
        &m_cloudMusic,
        &CloudMusicClient::authChanged,
        this,
        [this] {
            emit accountChanged();
            emit cloudStatusChanged();
            if (!m_cloudMusic.loggedIn()) {
                // Evolve account state is separate from the music backend.
                // Logging out must not destroy an Ourcraft browsing/playback
                // session; only cloud-owned social/account state is cleared.
                m_customPlaylists.clear();
                m_userProfile.clear();
                m_followingUsers.clear();
                m_userSearchResults.clear();
                m_communityPlaylists.clear();
                m_togetherRoom.clear();
                m_togetherInvites.clear();
                m_togetherPollTimer.stop();
                m_togetherPublishTimer.stop();
                clearTogetherEphemeral();
                m_socialPollTimer.stop();
                m_presenceTimer.stop();
                m_communityRefreshTimer.stop();
                m_seenInviteKeys.clear();
                m_onlineUsers = 0;
                emit onlineUsersChanged();
                emit customPlaylistsChanged();
                emit userProfileChanged();
                emit followingUsersChanged();
                emit userSearchResultsChanged();
                emit communityPlaylistsChanged();
                emit togetherRoomChanged();
                emit togetherInvitesChanged();
                setLoading(false);
            } else if (!m_cloudMusic.ready()) {
                // A persisted token means "logged in" before the async auth/me
                // validation has completed. Do not start any runWhenReady()
                // request from this synchronous auth notification. readyChanged
                // starts realtime/social refresh after bootstrap succeeds.
                qInfo() << "Cloud identity loaded; waiting for auth bootstrap before social refresh.";
            }
        });
    connect(&m_cloudMusic, &CloudMusicClient::authBusyChanged, this, &AppController::accountChanged);
    connect(&m_cloudMusic, &CloudMusicClient::authErrorChanged, this, &AppController::accountChanged);
    connect(&m_cloudMusic, &CloudMusicClient::customPlaylistsReady, this,
            [this](const QVariantList &playlists) {
        m_customPlaylists = playlists;
        emit customPlaylistsChanged();
        bool hasPublicPlaylist = false;
        for (const QVariant &entry : playlists) {
            if (entry.toMap().value(QStringLiteral("isPublic")).toBool()) {
                hasPublicPlaylist = true;
                break;
            }
        }
        if (hasPublicPlaylist)
            m_cloudMusic.loadCommunityPlaylists(28);
        if (m_currentPlaylist.value(QStringLiteral("custom")).toBool()) {
            const QString currentId = m_currentPlaylist.value(QStringLiteral("id")).toString();
            const int currentIndex = customPlaylistIndex(currentId);
            if (currentIndex >= 0) {
                m_currentPlaylist = m_customPlaylists.at(currentIndex).toMap();
                m_playlistTracks = m_currentPlaylist.value(QStringLiteral("tracks")).toList();
                emit currentPlaylistChanged();
                emit playlistTracksChanged();
            }
        }
    });
    connect(&m_cloudMusic, &CloudMusicClient::customPlaylistSaved, this,
            [this](const QVariantMap &playlist) {
        const QString id = playlist.value(QStringLiteral("id")).toString();
        const int index = customPlaylistIndex(id);
        if (index >= 0) m_customPlaylists[index] = playlist;
        else m_customPlaylists.prepend(playlist);
        emit customPlaylistsChanged();
        if (m_currentPlaylist.value(QStringLiteral("custom")).toBool()
            && m_currentPlaylist.value(QStringLiteral("id")).toString() == id) {
            m_currentPlaylist = playlist;
            m_playlistTracks = playlist.value(QStringLiteral("tracks")).toList();
            emit currentPlaylistChanged();
            emit playlistTracksChanged();
        }
        if (playlist.value(QStringLiteral("isPublic")).toBool())
            m_cloudMusic.loadCommunityPlaylists(28);
        emit toastRequested(QStringLiteral("歌单已同步到 Evolve Cloud"));
    });
    connect(&m_cloudMusic, &CloudMusicClient::customPlaylistDeleted, this,
            [this](const QString &playlistId) {
        const int index = customPlaylistIndex(playlistId);
        if (index >= 0) m_customPlaylists.removeAt(index);
        emit customPlaylistsChanged();
        if (m_currentPlaylist.value(QStringLiteral("custom")).toBool()
            && m_currentPlaylist.value(QStringLiteral("id")).toString() == playlistId) {
            m_currentPlaylist.clear();
            m_playlistTracks.clear();
            emit currentPlaylistChanged();
            emit playlistTracksChanged();
        }
        m_cloudMusic.loadCommunityPlaylists(28);
        emit toastRequested(QStringLiteral("歌单已删除"));
    });

    connect(&m_cloudMusic, &CloudMusicClient::profileReady, this,
            [this](const QVariantMap &profile) {
        m_userProfile = profile;
        emit userProfileChanged();
    });
    connect(&m_cloudMusic, &CloudMusicClient::profileSaved, this,
            [this](const QVariantMap &profile) {
        m_userProfile = profile;
        emit userProfileChanged();
        emit toastRequested(QStringLiteral("个人资料已保存"));
    });
    connect(&m_cloudMusic, &CloudMusicClient::userSearchReady, this,
            [this](const QVariantList &users) {
        m_userSearchResults = users;
        emit userSearchResultsChanged();
    });
    connect(&m_cloudMusic, &CloudMusicClient::followingReady, this,
            [this](const QVariantList &users) {
        m_followingUsers = users;
        emit followingUsersChanged();
    });
    connect(&m_cloudMusic, &CloudMusicClient::communityPlaylistsReady, this,
            [this](const QVariantList &playlists) {
        QVariantList normalizedPlaylists;
        normalizedPlaylists.reserve(playlists.size());
        for (const QVariant &entry : playlists) {
            QVariantMap playlist = entry.toMap();
            if (playlist.value(QStringLiteral("cover")).toString().trimmed().isEmpty()) {
                const QVariantList tracks = playlist.value(QStringLiteral("tracks")).toList();
                for (auto it = tracks.crbegin(); it != tracks.crend(); ++it) {
                    const QString cover = it->toMap().value(QStringLiteral("cover")).toString().trimmed();
                    if (!cover.isEmpty()) {
                        playlist[QStringLiteral("cover")] = cover;
                        break;
                    }
                }
            }
            normalizedPlaylists.append(playlist);
        }
        m_communityPlaylists = normalizedPlaylists;
        emit communityPlaylistsChanged();
        const QString currentId = m_currentPlaylist.value(QStringLiteral("id")).toString();
        if (!currentId.isEmpty() && m_currentPlaylist.value(QStringLiteral("providerId")).toString() == QStringLiteral("evolve")
            && !ownsPlaylist(m_currentPlaylist)) {
            for (const QVariant &entry : playlists) {
                const QVariantMap row = entry.toMap();
                if (row.value(QStringLiteral("id")).toString() == currentId
                    && row.value(QStringLiteral("updatedAt")).toLongLong() > m_currentPlaylist.value(QStringLiteral("updatedAt")).toLongLong()) {
                    m_cloudMusic.loadCommunityPlaylist(currentId);
                    break;
                }
            }
        }
    });
    connect(&m_cloudMusic, &CloudMusicClient::communityPlaylistReady, this,
            [this](const QVariantMap &playlist, const QVariantList &tracks) {
        QVariantMap normalized = playlist;
        const bool editable = ownsPlaylist(normalized);
        normalized[QStringLiteral("editable")] = editable;
        normalized[QStringLiteral("custom")] = editable;
        m_currentPlaylist = normalized;
        m_playlistTracks = tracks;
        setLoading(false);
        emit currentPlaylistChanged();
        emit playlistTracksChanged();
        emit playlistOpened();
    });
    connect(&m_cloudMusic, &CloudMusicClient::togetherInvitesReady, this,
            [this](const QVariantList &invites) {
        QVariantMap newestUnseen;
        // Worker returns newest first. Mark every returned invite as known but only
        // surface the newest one, so startup with several pending invitations does
        // not spam overlapping popups/tray notifications.
        for (const QVariant &entry : invites) {
            const QVariantMap invite = entry.toMap();
            const QString key = invite.value(QStringLiteral("id")).toString().isEmpty()
                ? invite.value(QStringLiteral("code")).toString() + QLatin1Char('|')
                  + invite.value(QStringLiteral("fromUsername")).toString() + QLatin1Char('|')
                  + QString::number(invite.value(QStringLiteral("createdAt")).toLongLong())
                : invite.value(QStringLiteral("id")).toString();
            if (!key.isEmpty() && !m_seenInviteKeys.contains(key)) {
                if (newestUnseen.isEmpty()) newestUnseen = invite;
                m_seenInviteKeys.insert(key);
            }
        }
        m_togetherInvites = invites;
        emit togetherInvitesChanged();
        if (!newestUnseen.isEmpty()) emit togetherInviteArrived(newestUnseen);
    });
    connect(&m_cloudMusic, &CloudMusicClient::togetherLeft, this, [this] {
        m_togetherPollTimer.stop();
        m_togetherPublishTimer.stop();
        clearTogetherEphemeral();
        m_togetherRoom.clear();
        emit togetherRoomChanged();
        emit toastRequested(QStringLiteral("已离开一起听房间，房间聊天与临时文件已从本机清除"));
    });
    connect(&m_cloudMusic, &CloudMusicClient::togetherRoomUnavailable, this,
            [this](const QString &code, const QString &message) {
        if (code.compare(m_togetherRoom.value(QStringLiteral("code")).toString(), Qt::CaseInsensitive) != 0)
            return;
        m_togetherPollTimer.stop();
        m_togetherPublishTimer.stop();
        clearTogetherEphemeral();
        m_togetherRoom.clear();
        emit togetherRoomChanged();
        const QString text = message.isEmpty() ? QStringLiteral("房主已解散房间") : message;
        emit togetherRoomClosed(text);
        emit toastRequested(QStringLiteral("一起听已结束：") + text);
    });
    connect(&m_cloudMusic, &CloudMusicClient::togetherMessagesReady, this,
            [this](const QVariantList &messages, qint64 cursor) {
        if (cursor < m_togetherMessageCursor) return;
        QSet<qint64> existing;
        for (const QVariant &entry : std::as_const(m_togetherMessages))
            existing.insert(entry.toMap().value(QStringLiteral("seq")).toLongLong());
        bool changed = false;
        for (const QVariant &entry : messages) {
            const QVariantMap message = entry.toMap();
            const qint64 seq = message.value(QStringLiteral("seq")).toLongLong();
            if (seq > 0 && existing.contains(seq)) continue;
            m_togetherMessages.append(message);
            if (seq > 0) existing.insert(seq);
            changed = true;
            if (message.value(QStringLiteral("kind")).toString() == QStringLiteral("image")) {
                const QString attachmentId = message.value(QStringLiteral("attachmentId")).toString();
                if (!attachmentId.isEmpty() && !m_togetherAttachmentUrls.contains(attachmentId)
                    && !m_togetherAttachmentDownloads.contains(attachmentId)) {
                    m_togetherAttachmentDownloads.insert(attachmentId);
                    m_cloudMusic.downloadTogetherAttachment(
                        m_togetherRoom.value(QStringLiteral("code")).toString(), attachmentId);
                }
            }
        }
        while (m_togetherMessages.size() > 220) m_togetherMessages.removeFirst();
        m_togetherMessageCursor = qMax(m_togetherMessageCursor, cursor);
        if (changed) emit togetherMessagesChanged();
    });
    connect(&m_cloudMusic, &CloudMusicClient::togetherMessageSent, this, [this] {
        refreshTogetherMessages();
    });
    connect(&m_cloudMusic, &CloudMusicClient::togetherAttachmentReady, this,
            [this](const QString &attachmentId, const QByteArray &bytes,
                   const QString &fileName, const QString &mimeType) {
        m_togetherAttachmentDownloads.remove(attachmentId);
        cacheTogetherAttachment(attachmentId, bytes, fileName, mimeType);
    });
    connect(&m_cloudMusic, &CloudMusicClient::togetherVoiceChunksReady, this,
            [this](const QVariantList &chunks, qint64 cursor) {
        if (cursor < m_togetherVoiceCursor) return;
        m_togetherVoiceCursor = qMax(m_togetherVoiceCursor, cursor);
        for (const QVariant &entry : chunks) {
            const QByteArray compressed = QByteArray::fromBase64(
                entry.toMap().value(QStringLiteral("payload")).toString().toLatin1());
            const QByteArray pcm = qUncompress(compressed);
            if (pcm.isEmpty()) continue;
            ensureTogetherVoiceOutput();
            if (m_togetherVoiceOutput) m_togetherVoiceOutput->write(pcm);
        }
    });

    connect(&m_cloudMusic, &CloudMusicClient::presenceReady, this, [this](int count) {
        count = qMax(0, count);
        if (m_onlineUsers == count) return;
        m_onlineUsers = count;
        emit onlineUsersChanged();
    });
    connect(&m_cloudMusic, &CloudMusicClient::togetherRoomReady, this,
            [this](const QVariantMap &room) {
        if (room.isEmpty())
            return;
        const QString oldCode = m_togetherRoom.value(QStringLiteral("code")).toString();
        const QString newCode = room.value(QStringLiteral("code")).toString();
        const bool enteredNewRoom = !newCode.isEmpty() && oldCode.compare(newCode, Qt::CaseInsensitive) != 0;
        if (enteredNewRoom) clearTogetherEphemeral();
        m_togetherRoom = room;
        emit togetherRoomChanged();

        if (enteredNewRoom) {
            m_togetherMessageCursor = 0;
            m_togetherVoiceCursor = 0;
            refreshTogetherMessages();
            refreshTogetherVoice();
        }
        if (!m_togetherChatPollTimer.isActive()) m_togetherChatPollTimer.start();
        if (!m_togetherVoicePollTimer.isActive()) m_togetherVoicePollTimer.start();

        const bool isHost = room.value(QStringLiteral("isHost")).toBool();
        if (!m_togetherPollTimer.isActive())
            m_togetherPollTimer.start();
        if (isHost) {
            if (!m_togetherPublishTimer.isActive())
                m_togetherPublishTimer.start();
            return;
        }
        m_togetherPublishTimer.stop();

        const QVariantMap state = room.value(QStringLiteral("state")).toMap();
        const QVariantMap track = state.value(QStringLiteral("track")).toMap();
        const QString remoteId = track.value(QStringLiteral("id")).toString();
        const QString localId = m_player.currentTrack().value(QStringLiteral("id")).toString();
        const qint64 remotePosition = state.value(QStringLiteral("position")).toLongLong();
        const bool remotePlaying = state.value(QStringLiteral("playing")).toBool();

        if (!remoteId.isEmpty() && remoteId != localId) {
            playTrack(track, QStringLiteral("together"));
            QTimer::singleShot(1200, this, [this, remotePosition, remoteId] {
                if (m_player.currentTrack().value(QStringLiteral("id")).toString() == remoteId)
                    m_player.seek(remotePosition);
            });
        } else if (!remoteId.isEmpty() && qAbs(m_player.position() - remotePosition) > 1800) {
            m_player.seek(remotePosition);
        }

        if (remotePlaying != m_player.playing())
            m_player.togglePlay();
    });

    connect(
        &m_cloudMusic,
        &CloudMusicClient::errorOccurred,
        this,
        [this](const QString &message) {
            // Cloud errors are account/community/Together errors only in
            // v0.18.0; music playback remains independent on Ourcraft.
            setLoading(false);
            setError(message);
            emit toastRequested(message);
        });

    // v0.18.0: do not connect Evolve Cloud catalog/search/playlist/stream/
    // lyrics/roam signals into the player UI. Ourcraft is the exclusive music
    // backend; Evolve Cloud remains connected below only for synchronized
    // Evolve account state and social features.

    connect(
        &m_cloudMusic,
        &CloudMusicClient::stateReady,
        this,
        &AppController::applyCloudState);


    connect(&m_sources, &MultiSourceManager::homeReady, this, [this](const QVariantList &items) {
        m_homePlaylists = items;
        setLoading(false);
        emit homePlaylistsChanged();
    });
    connect(&m_ourcraftNetease, &OurcraftProvider::discoveryTracksReady, this, [this](const QVariantList &items) {
        m_homeTracks = items;
        emit homeTracksChanged();
        bool playlistsChanged = false;
        if (!items.isEmpty()) {
            for (int i = 0; i < m_homePlaylists.size(); ++i) {
                QVariantMap playlist = m_homePlaylists.at(i).toMap();
                if (!playlist.value(QStringLiteral("cover")).toString().trimmed().isEmpty())
                    continue;
                const QVariantMap track = items.at(i % items.size()).toMap();
                const QString cover = track.value(QStringLiteral("cover")).toString().trimmed();
                if (cover.isEmpty()) continue;
                playlist[QStringLiteral("cover")] = cover;
                m_homePlaylists[i] = playlist;
                playlistsChanged = true;
            }
        }
        if (playlistsChanged) emit homePlaylistsChanged();
    });
    connect(&m_ourcraftNetease, &OurcraftProvider::catalogSearchReady, this,
            [this](const QVariantList &playlists, const QVariantList &artists) {
        m_searchPlaylists = playlists;
        if (!artists.isEmpty())
            m_searchArtists = artists;
        else if (m_searchArtists.isEmpty())
            inferSearchArtistsFromTracks();
        emit searchCatalogChanged();
    });
    const auto syncCurrentTrackMetadata = [this](const QVariantList &items) {
        const QVariantMap current = m_player.currentTrack();
        const QString currentId = current.value(QStringLiteral("id")).toString();
        if (currentId.isEmpty()) return;

        const QString currentTitle = current.value(QStringLiteral("title"))
                                         .toString().trimmed().toCaseFolded();
        const QString currentArtist = current.value(QStringLiteral("artist"))
                                          .toString().trimmed().toCaseFolded();
        for (const QVariant &entry : items) {
            QVariantMap candidate = entry.toMap();
            const bool sameId = candidate.value(QStringLiteral("id")).toString() == currentId;
            const bool sameText = !currentTitle.isEmpty()
                && candidate.value(QStringLiteral("title")).toString()
                       .trimmed().toCaseFolded() == currentTitle
                && (currentArtist.isEmpty()
                    || candidate.value(QStringLiteral("artist")).toString()
                           .trimmed().toCaseFolded() == currentArtist);
            if (!sameId && !sameText) continue;
            candidate[QStringLiteral("id")] = currentId;
            m_player.updateCurrentTrackMetadata(candidate);
            return;
        }
    };
    connect(&m_sources, &MultiSourceManager::searchProgress, this,
            [this, syncCurrentTrackMetadata](const QVariantList &items) {
        // Show the first healthy provider immediately; slower online providers
        // can merge more candidates afterwards without blocking the result list.
        m_searchResults = items;
        syncCurrentTrackMetadata(items);
        emit searchResultsChanged();
    });
    connect(&m_sources, &MultiSourceManager::searchReady, this,
            [this, syncCurrentTrackMetadata](const QVariantList &items) {
        m_searchResults = items;
        syncCurrentTrackMetadata(items);
        if (m_searchArtists.isEmpty()) {
            inferSearchArtistsFromTracks();
            emit searchCatalogChanged();
        }
        if (!m_pendingSearchKey.isEmpty()) {
            m_searchCache[m_pendingSearchKey] = CachedSearch{QDateTime::currentMSecsSinceEpoch(), m_searchResults};
            if (m_searchCache.size() > 32) {
                QString oldestKey;
                qint64 oldest = std::numeric_limits<qint64>::max();
                for (auto it = m_searchCache.cbegin(); it != m_searchCache.cend(); ++it) {
                    if (it.value().timestampMs < oldest) { oldest = it.value().timestampMs; oldestKey = it.key(); }
                }
                if (!oldestKey.isEmpty()) m_searchCache.remove(oldestKey);
            }
        }
        setLoading(false);
        emit searchResultsChanged();
    });
    connect(&m_sources, &MultiSourceManager::playlistReady, this,
            [this](const QVariantMap &playlist, const QVariantList &tracks) {
        m_currentPlaylist = playlist;
        m_playlistTracks = tracks;
        setLoading(false);
        emit currentPlaylistChanged();
        emit playlistTracksChanged();
        emit playlistOpened();
    });
    connect(&m_sources, &MultiSourceManager::streamReady, this,
            [this](const QString &id, const QString &url, const QString &providerId, const QString &providerName, const QString &access) {
        if (id != m_player.currentTrack().value(QStringLiteral("id")).toString())
            return;

        m_fastStreamLocalPending = false;
        const bool web = url.startsWith(QStringLiteral("evolve-web:"));
        QString resolvedUrl = url;
        if (web) {
            resolvedUrl = QUrl::fromPercentEncoding(
                url.mid(QStringLiteral("evolve-web:").size()).toLatin1());
        }

        considerFastStreamResult(
            QVariantMap{
                {QStringLiteral("trackId"), id},
                {QStringLiteral("urls"), QStringList{resolvedUrl}},
                {QStringLiteral("providerId"), providerId},
                {QStringLiteral("providerName"), providerName},
                {QStringLiteral("access"), access},
                {QStringLiteral("origin"), QStringLiteral("local")},
                {QStringLiteral("web"), web}
            },
            providerId == QStringLiteral("netease"));
    });
    connect(&m_sources, &MultiSourceManager::streamResolveFailed, this,
            [this](const QString &id, const QString &message) {
        if (id != m_player.currentTrack().value(QStringLiteral("id")).toString())
            return;
        markFastStreamBranchFailed(false, message);
    });

    connect(&m_sources, &MultiSourceManager::downloadReady, this,
            [this](const QString &id, const QString &url, const QString &, const QString &providerName) {
        if (!m_pendingDownloads.contains(id)) return;
        const QVariantMap track = m_pendingDownloads.take(id);
        if (url.isEmpty()) return;
        QNetworkRequest request{QUrl(url)};
        request.setHeader(QNetworkRequest::UserAgentHeader, "EvolveMusic/0.4 Qt6");
        request.setTransferTimeout(15000);
        QNetworkReply *reply = m_downloadNetwork.get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, track, providerName] {
            if (reply->error() != QNetworkReply::NoError) {
                emit toastRequested("下载失败：" + reply->errorString());
                reply->deleteLater();
                return;
            }
            QString musicDir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
            if (musicDir.isEmpty()) musicDir = QDir::homePath() + "/Music";
            QDir dir(musicDir);
            dir.mkpath("EvolveMusic");
            dir.cd("EvolveMusic");
            QString fileName = track.value("artist").toString() + " - " + track.value("title").toString();
            fileName.replace(QRegularExpression(R"([\/:*?"<>|])"), "_");
            fileName = fileName.trimmed();
            if (fileName.isEmpty()) fileName = track.value("id").toString();

            QString suffix = ".mp3";
            const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
            if (contentType.contains("flac")) suffix = ".flac";
            else if (contentType.contains("mp4") || contentType.contains("m4a")) suffix = ".m4a";
            const QString path = dir.filePath(fileName + suffix);
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(reply->readAll()) < 0) {
                emit toastRequested("下载失败：无法写入音乐文件夹");
            } else {
                file.close();
                emit toastRequested("已从 " + providerName + " 下载到：" + QDir::toNativeSeparators(path));
            }
            reply->deleteLater();
        });
    });
    connect(&m_sources, &MultiSourceManager::lyricsReady, this,
            [this](const QString &id, const QString &raw, const QString &translated) {
        if (id != m_player.currentTrack().value("id").toString()) return;
        m_lyrics = parseLrc(raw, translated);
        emit lyricsChanged();
    });
    connect(&m_sources, &MultiSourceManager::errorOccurred, this, [this](const QString &text) {
        setLoading(false);
        setError(text);
        emit toastRequested(text);
    });
    connect(&m_sources, &MultiSourceManager::providerStateChanged, this, [this] {
        emit providersChanged();
        emit playbackSourcePreferenceChanged();
    });
    connect(&m_sources, &MultiSourceManager::pingReady, this, [this](const QString &, bool ok, const QString &message) {
        emit toastRequested(ok ? message : ("连接失败：" + message));
    });

    connect(
        &m_player,
        &PlayerController::streamRequested,
        this,
        [this](const QVariantMap &track) {
            m_activeSourceName.clear();
            m_activeSourceAccess.clear();
            m_activeSourceProviderId.clear();
            emit activeSourceChanged();
            beginFastStreamRace(track);
        });
    connect(&m_player, &PlayerController::currentTrackChanged, this,
            [this, observedTrackId = QString()]() mutable {
        const QVariantMap track = m_player.currentTrack();
        const QString trackId = track.value(QStringLiteral("id")).toString();
        const bool identityChanged = trackId != observedTrackId;
        observedTrackId = trackId;
        m_cloudOuterFallbackAttempted = false;
        m_cloudPlaybackFallbackAttempted = false;
        m_activeSourceProviderId.clear();
        m_failedWebProviders.clear();
        if (identityChanged && !m_manualPlaybackProviderId.isEmpty()) {
            m_manualPlaybackProviderId.clear();
        }
        emit playbackSourcePreferenceChanged();
        if (track.isEmpty()) return;
        appendHistory(track);
        if (!identityChanged) return;
        m_lyrics.clear();
        emit lyricsChanged();

        m_sources.loadBestLyrics(track);

        if (m_roamActive) {
            const int remaining = qMax(0, static_cast<int>(m_player.queue().size())
                                          - m_player.currentIndex() - 1);
            if (remaining <= 4)
                requestRoamBatch(false);
        }
    });
    connect(&m_player, &PlayerController::playbackError, this, [this](const QString &msg) {
        const QVariantMap track = m_player.currentTrack();

        if (!m_manualPlaybackProviderId.isEmpty()) {
            const QString sourceName = m_manualPlaybackProviderId == QStringLiteral("netease")
                ? QStringLiteral("网易云音乐")
                : (m_manualPlaybackProviderId == QStringLiteral("kugou")
                       ? QStringLiteral("酷狗音乐")
                       : QStringLiteral("酷我音乐"));
            emit toastRequested(QStringLiteral("所选音源暂时无法播放：")
                                + sourceName);
            return;
        }

        if (m_player.webPlaybackActive() && !m_activeSourceProviderId.isEmpty() && !track.isEmpty()) {
            if (!m_failedWebProviders.contains(m_activeSourceProviderId))
                m_failedWebProviders << m_activeSourceProviderId;

            // The original fast race is already marked as settled when a web
            // page wins.  Reset only the local branch here so the next website
            // or native provider is actually allowed to replace the failed
            // WebView source.  v0.14.3 used to discard that second web result.
            m_fastStreamPreferenceTimer.stop();
            m_fastStreamTrackId = track.value(QStringLiteral("id")).toString();
            m_fastStreamSettled = false;
            m_fastStreamProvisional.clear();
            m_fastStreamCloudPending = false;
            m_fastStreamLocalPending = true;
            m_fastStreamErrors.clear();

            emit toastRequested(QStringLiteral("网页源不可用，正在并发切换下一个可用音源…"));
            m_sources.resolveBestStream(track, m_qualityLevel, m_failedWebProviders);
            return;
        }

        if (!track.isEmpty()) {
            if (!m_cloudPlaybackFallbackAttempted) {
                m_cloudPlaybackFallbackAttempted = true;
                emit toastRequested(
                    QStringLiteral("当前线路不可用，正在通过 Ourcraft 重新选择网易/酷狗/酷我来源…"));
                m_sources.resolveBestStream(track, m_qualityLevel);
                return;
            }
        }

        if (m_roamActive && m_player.queue().size() > 1) {
            emit toastRequested(QStringLiteral("这首音频线路不可用，已为你换下一首。"));
            QTimer::singleShot(180, this, [this] {
                if (m_roamActive)
                    m_player.next();
            });
            return;
        }

        setError(QStringLiteral("播放失败：") + msg);
        emit toastRequested(m_errorMessage);
    });
}

void AppController::configureProviders()
{
    // Music catalog/playback backend: ourcraft-music-api. Evolve Cloud remains
    // responsible for Evolve accounts, community playlists, Together rooms,
    // presence and updates; it is no longer the music catalog/stream resolver.
    const QString ourcraftBase = m_settings
        .value(QStringLiteral("music/ourcraft/baseUrl"), QStringLiteral("https://music.yuncan.xyz"))
        .toString()
        .trimmed();

    m_ourcraftNetease.setBaseUrl(ourcraftBase);
    m_ourcraftKugou.setBaseUrl(ourcraftBase);
    m_ourcraftKuwo.setBaseUrl(ourcraftBase);

    // Keep the old local gateway addresses only for the optional provider login
    // helper. They are deliberately not registered with MultiSourceManager.
    const QString legacyNeteaseBase = m_settings.value("source/apiBaseUrl", "http://127.0.0.1:3000").toString();
    m_netease.setBaseUrl(m_settings.value("accounts/netease/baseUrl", legacyNeteaseBase).toString());
    m_qq.setBaseUrl(m_settings.value("accounts/qq/baseUrl", "http://127.0.0.1:3200").toString());
    m_kugou.setBaseUrl(m_settings.value("accounts/kugou/baseUrl", "http://127.0.0.1:3300").toString());

    const QString yuetingBase = m_settings.value("providers/yueting/baseUrl", "https://www.yueting.net").toString();
    const QString gequhaiBase = m_settings.value("providers/gequhai/baseUrl", "https://www.gequhai.com").toString();
    m_yueting.setBaseUrl(yuetingBase);
    m_gequhai.setBaseUrl(gequhaiBase);

    m_sources.addProvider(
        &m_ourcraftNetease,
        m_settings.value("providers/netease/enabled", true).toBool(),
        0);

    m_sources.addProvider(
        &m_ourcraftKugou,
        m_settings.value("providers/kugou/enabled", true).toBool(),
        qMax(10, m_settings.value("providers/kugou/priority", 20).toInt()));

    m_sources.addProvider(
        &m_ourcraftKuwo,
        m_settings.value("providers/kuwo/enabled", true).toBool(),
        qMax(10, m_settings.value("providers/kuwo/priority", 30).toInt()));

    // Website-backed providers remain optional emergency fallbacks only.
    m_sources.addProvider(
        &m_yueting,
        m_settings.value("providers/yueting/enabled", false).toBool(),
        qMax(40, m_settings.value("providers/yueting/priority", 60).toInt()));

    m_sources.addProvider(
        &m_gequhai,
        m_settings.value("providers/gequhai/enabled", false).toBool(),
        qMax(40, m_settings.value("providers/gequhai/priority", 70).toInt()));
}

void AppController::loginAccount(const QString &username, const QString &password)
{
    m_cloudMusic.login(username, password);
}

void AppController::registerAccount(const QString &username,
                                    const QString &password,
                                    const QString &email,
                                    const QString &emailCode,
                                    bool acceptTerms)
{
    m_cloudMusic.registerAccount(username, password, email, emailCode, acceptTerms);
}

void AppController::sendRegistrationEmailCode(const QString &email)
{
    m_cloudMusic.sendRegistrationEmailCode(email);
}

void AppController::sendPasswordResetCode(const QString &email)
{
    m_cloudMusic.sendPasswordResetCode(email);
}

void AppController::resetPassword(const QString &email,
                                  const QString &code,
                                  const QString &newPassword)
{
    m_cloudMusic.resetPassword(email, code, newPassword);
}

void AppController::logoutAccount()
{
    m_cloudMusic.logout();
}

void AppController::createCustomPlaylist(const QString &name)
{
    if (!m_cloudMusic.ready()) {
        emit toastRequested(QStringLiteral("请先登录 EvolveMusic 账号"));
        return;
    }
    m_cloudMusic.createCustomPlaylist(name.trimmed());
}

void AppController::deleteCustomPlaylist(const QString &playlistId)
{
    if (customPlaylistIndex(playlistId) < 0) {
        emit toastRequested(QStringLiteral("只有歌单创建者可以删除这个歌单。"));
        return;
    }
    m_cloudMusic.deleteCustomPlaylist(playlistId);
}

void AppController::renameCustomPlaylist(const QString &playlistId, const QString &name)
{
    const int index = customPlaylistIndex(playlistId);
    if (index < 0) {
        emit toastRequested(QStringLiteral("只有歌单创建者可以编辑这个歌单。"));
        return;
    }
    QVariantMap playlist = m_customPlaylists.at(index).toMap();
    playlist[QStringLiteral("name")] = name.trimmed().isEmpty() ? QStringLiteral("未命名歌单") : name.trimmed().left(48);
    m_customPlaylists[index] = playlist;
    emit customPlaylistsChanged();
    if (m_currentPlaylist.value(QStringLiteral("custom")).toBool()
        && m_currentPlaylist.value(QStringLiteral("id")).toString() == playlistId) {
        m_currentPlaylist = playlist;
        emit currentPlaylistChanged();
    }
    saveCustomPlaylistAt(index);
}

void AppController::addTrackToCustomPlaylist(const QString &playlistId, const QVariantMap &track)
{
    const int index = customPlaylistIndex(playlistId);
    if (index < 0) { emit toastRequested(QStringLiteral("只有歌单创建者可以添加歌曲。")); return; }
    if (track.isEmpty()) return;
    QVariantMap playlist = m_customPlaylists.at(index).toMap();
    QVariantList tracks = playlist.value(QStringLiteral("tracks")).toList();
    const QString trackId = track.value(QStringLiteral("id")).toString();
    for (const QVariant &value : tracks) {
        if (!trackId.isEmpty() && value.toMap().value(QStringLiteral("id")).toString() == trackId) {
            emit toastRequested(QStringLiteral("这首歌已经在该歌单中"));
            return;
        }
    }
    tracks.append(persistentTrack(track));
    playlist[QStringLiteral("tracks")] = tracks;
    playlist[QStringLiteral("trackCount")] = tracks.size();
    m_customPlaylists[index] = playlist;
    emit customPlaylistsChanged();
    if (m_currentPlaylist.value(QStringLiteral("custom")).toBool()
        && m_currentPlaylist.value(QStringLiteral("id")).toString() == playlistId) {
        m_currentPlaylist = playlist;
        m_playlistTracks = tracks;
        emit currentPlaylistChanged();
        emit playlistTracksChanged();
    }
    saveCustomPlaylistAt(index);
}

void AppController::removeTrackFromCustomPlaylist(const QString &playlistId, const QString &trackId)
{
    const int index = customPlaylistIndex(playlistId);
    if (index < 0) { emit toastRequested(QStringLiteral("只有歌单创建者可以删除歌曲。")); return; }
    QVariantMap playlist = m_customPlaylists.at(index).toMap();
    QVariantList tracks = playlist.value(QStringLiteral("tracks")).toList();
    for (int i = tracks.size() - 1; i >= 0; --i) {
        if (tracks.at(i).toMap().value(QStringLiteral("id")).toString() == trackId)
            tracks.removeAt(i);
    }
    playlist[QStringLiteral("tracks")] = tracks;
    playlist[QStringLiteral("trackCount")] = tracks.size();
    m_customPlaylists[index] = playlist;
    emit customPlaylistsChanged();
    if (m_currentPlaylist.value(QStringLiteral("custom")).toBool()
        && m_currentPlaylist.value(QStringLiteral("id")).toString() == playlistId) {
        m_currentPlaylist = playlist;
        m_playlistTracks = tracks;
        emit currentPlaylistChanged();
        emit playlistTracksChanged();
    }
    saveCustomPlaylistAt(index);
}

void AppController::openCustomPlaylist(const QString &playlistId)
{
    const int index = customPlaylistIndex(playlistId);
    if (index < 0) return;
    const QVariantMap playlist = m_customPlaylists.at(index).toMap();
    m_currentPlaylist = playlist;
    m_playlistTracks = playlist.value(QStringLiteral("tracks")).toList();
    emit currentPlaylistChanged();
    emit playlistTracksChanged();
    emit playlistOpened();
}

void AppController::initialize()
{
    const QString apiBase =
        cloudApiBaseUrl();

    const bool nextCloudMode =
        !apiBase.isEmpty();

    if (m_cloudMusicMode
        != nextCloudMode) {
        m_cloudMusicMode =
            nextCloudMode;
        emit cloudMusicModeChanged();
        emit cloudStatusChanged();
    }

    if (m_cloudMusicMode) {
        m_cloudMusic.setBaseUrls(cloudApiUrls());
        m_cloudMusic.setProfileDataRoot(
            m_profiles.currentProfileDataRoot());
        // Cloud bootstrap is social/account state only. Music browsing and
        // playback below always go through Ourcraft Music API.
        m_cloudMusic.initialize();
    }

    refreshHome();
    m_sources.pingAll();
    QTimer::singleShot(0, this, &AppController::testCloudRoutes);
}

void AppController::setCloudRouteMode(const QString &mode)
{
    QString normalized = mode.trimmed().toLower();
    if (normalized != QStringLiteral("primary")
        && normalized != QStringLiteral("relay")) {
        normalized = QStringLiteral("auto");
    }
    if (m_cloudRouteMode == normalized)
        return;

    m_cloudRouteMode = normalized;
    m_settings.setValue(QStringLiteral("cloud/routeMode"), normalized);
    applyCloudRouteSelection();
    emit cloudRouteChanged();

    const QString label = normalized == QStringLiteral("primary")
        ? QStringLiteral("Workers.dev 主节点")
        : (normalized == QStringLiteral("relay")
               ? QStringLiteral("tyxowo.top 中转节点")
               : QStringLiteral("自动延迟选择"));
    emit toastRequested(QStringLiteral("云端线路已切换：") + label);
    testCloudRoutes();
}

void AppController::testCloudRoutes()
{
    const QStringList endpoints = configuredCloudApiUrls();
    const quint64 generation = ++m_cloudRouteGeneration;
    m_cloudRouteTesting = !endpoints.isEmpty();
    m_cloudRoutePending = endpoints.size();
    m_cloudPrimaryLatency = -1;
    m_cloudRelayLatency = -1;
    emit cloudRouteChanged();

    if (endpoints.isEmpty()) {
        m_cloudRouteTesting = false;
        emit cloudRouteChanged();
        return;
    }

    for (int index = 0; index < endpoints.size(); ++index) {
        const QString endpoint = endpoints.at(index);
        QNetworkRequest request{QUrl(endpoint + QStringLiteral("/v1/auth/status"))};
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EvolveMusic/0.18.1 RouteProbe"));
        request.setRawHeader("Accept", "application/json");
        request.setTransferTimeout(5000);
        const qint64 startedAt = QDateTime::currentMSecsSinceEpoch();
        QNetworkReply *reply = m_downloadNetwork.get(request);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, generation, index, startedAt]() {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray bytes = reply->readAll();
            const bool transportOk = reply->error() == QNetworkReply::NoError
                && status >= 200 && status < 300;
            reply->deleteLater();
            if (generation != m_cloudRouteGeneration)
                return;

            QJsonParseError parseError{};
            const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
            const bool available = transportOk
                && parseError.error == QJsonParseError::NoError
                && document.isObject()
                && document.object().value(QStringLiteral("ok")).toBool(true);
            const int latency = available
                ? qBound(1, static_cast<int>(QDateTime::currentMSecsSinceEpoch() - startedAt), 99999)
                : -1;
            if (index == 0) m_cloudPrimaryLatency = latency;
            else if (index == 1) m_cloudRelayLatency = latency;
            emit cloudRouteChanged();

            if (--m_cloudRoutePending > 0)
                return;
            m_cloudRouteTesting = false;
            qInfo() << "Cloud route probe complete; primary=" << m_cloudPrimaryLatency
                    << "ms relay=" << m_cloudRelayLatency << "ms mode=" << m_cloudRouteMode;
            if (m_cloudRouteMode == QStringLiteral("auto"))
                applyCloudRouteSelection();
            emit cloudRouteChanged();
        });
    }
}

void AppController::refreshHome()
{
    setLoading(true);
    setError(QString());
    m_homePlaylists.clear();
    m_homeTracks.clear();
    emit homePlaylistsChanged();
    emit homeTracksChanged();
    m_sources.loadHome();
    if (m_cloudMusic.loggedIn())
        m_cloudMusic.loadCommunityPlaylists(28);
    emit toastRequested(QStringLiteral("正在刷新发现内容"));
}

void AppController::setRoamLoading(bool value)
{
    if (m_roamLoading == value)
        return;
    m_roamLoading = value;
    emit roamLoadingChanged();
}

void AppController::startRoam(bool fresh)
{
    // Roam is a continuous radio-like queue: never inherit single-track repeat.
    if (m_player.playMode() == 1)
        m_player.setPlayMode(0);

    const bool wasActive = m_roamActive;
    if (!m_roamActive) {
        m_roamActive = true;
        emit roamActiveChanged();
    }

    if (fresh || !wasActive || m_player.currentTrack().isEmpty()) {
        requestRoamBatch(true);
        return;
    }

    const int remaining = qMax(0, static_cast<int>(m_player.queue().size())
                                  - m_player.currentIndex() - 1);
    if (remaining <= 4)
        requestRoamBatch(false);
}

void AppController::stopRoam()
{
    if (!m_roamActive)
        return;
    m_roamActive = false;
    m_roamReplacePending = false;
    setRoamLoading(false);
    emit roamActiveChanged();
}

void AppController::requestRoamBatch(bool replace)
{
    if (!m_roamActive || m_roamLoading)
        return;

    setRoamLoading(true);
    m_roamReplacePending = replace;

    QStringList excludeIds;
    QSet<QString> seen;
    const QVariantList queue = m_player.queue();
    for (const QVariant &entry : queue) {
        const QVariantMap row = entry.toMap();
        QString id = row.value(QStringLiteral("sourceId")).toString();
        if (id.isEmpty())
            id = row.value(QStringLiteral("id")).toString();
        if (!id.isEmpty() && !seen.contains(id)) {
            seen.insert(id);
            excludeIds << id;
        }
        if (excludeIds.size() >= 36)
            break;
    }

    for (const QVariant &entry : m_history) {
        if (excludeIds.size() >= 48)
            break;
        const QVariantMap row = entry.toMap();
        QString id = row.value(QStringLiteral("sourceId")).toString();
        if (id.isEmpty())
            id = row.value(QStringLiteral("id")).toString();
        if (!id.isEmpty() && !seen.contains(id)) {
            seen.insert(id);
            excludeIds << id;
        }
    }

    // v0.18.0: radio/roam also stays on the Ourcraft music backend.
    // Evolve Cloud is social/account storage only and must never be required
    // to return catalog recommendations or stream metadata.
    QVariantList candidates;
    for (const QVariant &entry : m_homeTracks) {
        const QVariantMap row = entry.toMap();
        const QString id = row.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || seen.contains(id))
            continue;
        candidates.append(row);
        seen.insert(id);
        if (candidates.size() >= 18)
            break;
    }

    const bool doReplace = m_roamReplacePending;
    m_roamReplacePending = false;
    setRoamLoading(false);

    if (candidates.isEmpty()) {
        refreshHome();
        emit toastRequested(QStringLiteral("正在刷新发现内容，稍后再点一次漫游。"));
        return;
    }

    if (doReplace)
        m_player.setQueueAndPlay(candidates, 0);
    else
        m_player.appendToQueue(candidates);
}

void AppController::search(const QString &keywords)
{
    const QString normalized = keywords.trimmed().simplified();
    if (normalized.isEmpty()) return;
    const QString key = normalized.toCaseFolded();

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const auto it = m_searchCache.constFind(key);
    if (it != m_searchCache.cend() && now - it.value().timestampMs < 2 * 60 * 1000) {
        m_pendingSearchKey = key;
        m_searchResults = it.value().rows;
        setLoading(false);
        emit searchResultsChanged();
        m_ourcraftNetease.searchCatalog(normalized, 18);
        return;
    }

    m_pendingSearchKey = key;
    m_cloudSearchResults.clear();
    m_webSearchResults.clear();
    m_searchPlaylists.clear();
    m_searchArtists.clear();
    emit searchCatalogChanged();
    setLoading(true);
    setError(QString());

    // Ourcraft Music API is the catalog backend even when Evolve Cloud is
    // connected for accounts/social. NetEase, KuGou and Kuwo race in parallel.
    m_sources.search(normalized, 40);
    m_ourcraftNetease.searchCatalog(normalized, 18);
}

void AppController::inferSearchArtistsFromTracks()
{
    if (!m_searchArtists.isEmpty() || m_searchResults.isEmpty())
        return;

    QHash<QString, QVariantMap> inferred;
    QHash<QString, int> counts;
    for (const QVariant &entry : std::as_const(m_searchResults)) {
        const QVariantMap track = entry.toMap();
        const QString name = track.value(QStringLiteral("artist")).toString().trimmed();
        const QString key = normalizedSearchPart(name);
        if (name.isEmpty() || key.isEmpty()
            || name == QStringLiteral("未知歌手") || name == QStringLiteral("Unknown Artist")) {
            continue;
        }
        counts[key] += 1;
        if (inferred.contains(key))
            continue;
        inferred.insert(key, QVariantMap{
            {QStringLiteral("id"), QStringLiteral("inferred:") + key.left(80)},
            {QStringLiteral("providerId"), QStringLiteral("netease")},
            {QStringLiteral("name"), name},
            {QStringLiteral("cover"), track.value(QStringLiteral("cover"))},
            {QStringLiteral("albumCount"), 0},
            {QStringLiteral("inferred"), true}
        });
    }

    QList<QVariantMap> rows = inferred.values();
    const QString queryKey = normalizedSearchPart(m_pendingSearchKey);
    std::sort(rows.begin(), rows.end(), [&counts, &queryKey](const QVariantMap &a, const QVariantMap &b) {
        const QString ak = normalizedSearchPart(a.value(QStringLiteral("name")).toString());
        const QString bk = normalizedSearchPart(b.value(QStringLiteral("name")).toString());
        const bool am = !queryKey.isEmpty() && (ak.contains(queryKey) || queryKey.contains(ak));
        const bool bm = !queryKey.isEmpty() && (bk.contains(queryKey) || queryKey.contains(bk));
        if (am != bm) return am;
        if (counts.value(ak) != counts.value(bk)) return counts.value(ak) > counts.value(bk);
        return a.value(QStringLiteral("name")).toString().localeAwareCompare(
                   b.value(QStringLiteral("name")).toString()) < 0;
    });

    for (QVariantMap row : std::as_const(rows)) {
        const QString key = normalizedSearchPart(row.value(QStringLiteral("name")).toString());
        row[QStringLiteral("trackCount")] = counts.value(key);
        m_searchArtists.append(row);
        if (m_searchArtists.size() >= 8)
            break;
    }
}

void AppController::openPlaylist(const QString &playlistId, const QString &providerId)
{
    QString resolvedProvider = providerId;
    QString sourceId = playlistId;
    if (resolvedProvider.isEmpty() && playlistId.contains(':')) {
        resolvedProvider = playlistId.section(':', 0, 0);
        sourceId = playlistId.section(':', 1);
    }
    if (resolvedProvider.isEmpty())
        resolvedProvider = "netease";

    setLoading(true);
    setError(QString());

    // Navigate immediately so a slow playlist request still produces visible
    // feedback. playlistReady replaces this lightweight loading shell with the
    // actual metadata and tracks.
    m_currentPlaylist = QVariantMap{
        {QStringLiteral("id"), resolvedProvider + QLatin1Char(':') + sourceId},
        {QStringLiteral("sourceId"), sourceId},
        {QStringLiteral("providerId"), resolvedProvider},
        {QStringLiteral("providerName"), resolvedProvider == QStringLiteral("netease")
             ? QStringLiteral("网易云 · Ourcraft") : QStringLiteral("Ourcraft Music API")},
        {QStringLiteral("name"), QStringLiteral("正在加载歌单…")},
        {QStringLiteral("description"), QStringLiteral("正在从 Ourcraft Music API 获取歌曲")},
        {QStringLiteral("loading"), true}
    };
    m_playlistTracks.clear();
    emit currentPlaylistChanged();
    emit playlistTracksChanged();
    emit playlistOpened();

    m_sources.loadPlaylist(
        resolvedProvider,
        sourceId);
}

QVariantList AppController::contextQueue(const QString &context) const
{
    if (context == "playlist") return m_playlistTracks;
    if (context == "search") return m_searchResults;
    if (context == "favorites") return m_favorites;
    if (context == "history") return m_history;
    if (context == "discovery") return m_homeTracks;
    if (!m_playlistTracks.isEmpty()) return m_playlistTracks;
    if (!m_searchResults.isEmpty()) return m_searchResults;
    return {};
}

void AppController::playTrack(const QVariantMap &track, const QString &context)
{
    if (context != QStringLiteral("roam") && m_roamActive)
        stopRoam();

    // Ourcraft search/home metadata intentionally does not pre-probe every
    // stream URL. Resolve availability only when Play is pressed; otherwise a
    // temporary upstream hiccup could hide valid tracks from the queue.
    QVariantList queue = contextQueue(context);
    if (queue.isEmpty())
        queue = {track};

    const QString id = track.value("id").toString();
    int index = -1;
    for (int i = 0; i < queue.size(); ++i) {
        if (queue.at(i).toMap().value("id").toString() == id) {
            index = i;
            break;
        }
    }
    // Social/together playback and community cards may point to a track that
    // is not part of whatever list happened to be open previously. Never fall
    // through to queue index 0 in that case.
    if (index < 0) {
        queue = {track};
        index = 0;
    }

    m_player.setQueueAndPlay(queue, index);
}

void AppController::toggleFavorite(const QVariantMap &track)
{
    const QString id = track.value("id").toString();
    if (id.isEmpty()) return;
    for (int i = 0; i < m_favorites.size(); ++i) {
        if (m_favorites.at(i).toMap().value("id").toString() == id) {
            m_favorites.removeAt(i);
            if (m_cloudMusicMode)
                scheduleCloudStateSave();
            else
                saveList("library/favorites", m_favorites);

            emit favoritesChanged();
            emit toastRequested("已取消喜欢");
            return;
        }
    }
    m_favorites.prepend(persistentTrack(track));
    if (m_cloudMusicMode)
        scheduleCloudStateSave();
    else
        saveList("library/favorites", m_favorites);

    emit favoritesChanged();
    emit toastRequested("已添加到我喜欢");
}

void AppController::downloadTrack(const QVariantMap &track)
{
    const QString id =
        track.value("id").toString();

    if (id.isEmpty())
        return;

    m_pendingDownloads[id] = track;

    emit toastRequested(
        "正在从可下载来源中自动选择："
        + track.value("title").toString());

    m_sources.resolveBestDownload(
        track,
        m_qualityLevel);
}

bool AppController::isFavorite(const QString &songId) const
{
    for (const auto &v : m_favorites)
        if (v.toMap().value("id").toString() == songId) return true;
    return false;
}

QVariantMap AppController::persistentTrack(const QVariantMap &track) const
{
    // Do not persist large/transient provider payloads in history/favorites.
    // The lightweight representation is enough to render and replay a track.
    static const QStringList keys{
        "id", "sourceId", "providerId", "providerName", "title", "artist",
        "album", "cover", "duration", "access", "playable", "bitrate",
        "audioType", "sourceCount", "sourceSummary", "sources"
    };
    QVariantMap out;
    for (const QString &key : keys) {
        if (track.contains(key)) out.insert(key, track.value(key));
    }
    return out;
}

void AppController::appendHistory(const QVariantMap &track)
{
    const QVariantMap stored = persistentTrack(track);
    const QString id = stored.value("id").toString();
    if (id.isEmpty()) return;
    for (int i = m_history.size() - 1; i >= 0; --i)
        if (m_history.at(i).toMap().value("id").toString() == id) m_history.removeAt(i);
    m_history.prepend(stored);
    while (m_history.size() > 80) m_history.removeLast();
    if (m_cloudMusicMode)
        scheduleCloudStateSave();
    else
        m_historySaveTimer.start();

    emit historyChanged();
}

void AppController::clearHistory()
{
    m_history.clear();
    m_historySaveTimer.stop();

    if (m_cloudMusicMode)
        scheduleCloudStateSave();
    else
        saveList("library/history", m_history);

    emit historyChanged();
    emit toastRequested("播放历史已清空");
}

void AppController::testProvider(const QString &providerId)
{
    m_sources.ping(providerId);
}

void AppController::setProviderEnabled(const QString &providerId, bool enabled)
{
    m_sources.setProviderEnabled(providerId, enabled);
    m_settings.setValue("providers/" + providerId + "/enabled", enabled);
    emit providersChanged();
}

void AppController::setProviderBaseUrl(const QString &providerId, const QString &url)
{
    if (providerId == QStringLiteral("netease")
        || providerId == QStringLiteral("kugou")
        || providerId == QStringLiteral("kuwo")) {
        setOurcraftApiUrl(url);
        return;
    }
    m_sources.setProviderBaseUrl(providerId, url);
    m_settings.setValue("providers/" + providerId + "/baseUrl", m_sources.providerBaseUrl(providerId));
    emit providersChanged();
}

void AppController::setOurcraftApiUrl(const QString &url)
{
    QString normalized = url.trimmed();
    if (normalized.isEmpty())
        normalized = QStringLiteral("https://music.yuncan.xyz");
    while (normalized.endsWith(QLatin1Char('/'))) normalized.chop(1);
    if (normalized.endsWith(QStringLiteral("/api"), Qt::CaseInsensitive))
        normalized.chop(4);
    while (normalized.endsWith(QLatin1Char('/'))) normalized.chop(1);

    const QUrl parsed(normalized);
    if (!parsed.isValid()
        || (parsed.scheme() != QStringLiteral("https")
            && parsed.scheme() != QStringLiteral("http"))) {
        emit toastRequested(QStringLiteral("Ourcraft API 地址无效，请填写 http:// 或 https:// 地址"));
        return;
    }

    m_ourcraftNetease.setBaseUrl(normalized);
    m_ourcraftKugou.setBaseUrl(normalized);
    m_ourcraftKuwo.setBaseUrl(normalized);
    m_settings.setValue(QStringLiteral("music/ourcraft/baseUrl"), normalized);
    m_searchCache.clear();
    emit providersChanged();
    m_sources.pingAll();
    emit toastRequested(QStringLiteral("Ourcraft Music API 地址已保存：") + normalized);
}

void AppController::resetOurcraftApiUrl()
{
    setOurcraftApiUrl(QStringLiteral("https://music.yuncan.xyz"));
}

void AppController::setProviderPriority(
    const QString &providerId,
    int priority)
{
    if (providerId
        == QStringLiteral("netease")) {
        m_sources.setProviderPriority(
            providerId,
            0);

        m_settings.setValue(
            QStringLiteral(
                "providers/netease/priority"),
            0);

        emit providersChanged();
        return;
    }

    const int safePriority =
        qMax(10, priority);

    m_sources.setProviderPriority(
        providerId,
        safePriority);

    m_settings.setValue(
        QStringLiteral("providers/")
            + providerId
            + QStringLiteral("/priority"),
        safePriority);

    emit providersChanged();
}

void AppController::openProviderWebsite(const QString &providerId)
{
    QString url;
    if (providerId == QStringLiteral("yueting")) url = m_yueting.baseUrl();
    else if (providerId == QStringLiteral("gequhai")) url = m_gequhai.baseUrl();
    else url = m_sources.providerBaseUrl(providerId);
    if (!url.trimmed().isEmpty()) QDesktopServices::openUrl(QUrl(url));
}

void AppController::setQualityLevel(const QString &level)
{
    static const QStringList allowed{"standard", "higher", "exhigh", "lossless"};
    if (!allowed.contains(level) || level == m_qualityLevel) return;
    m_qualityLevel = level;
    if (m_profileSettings) m_profileSettings->setValue("playback/qualityLevel", m_qualityLevel);
    emit qualityLevelChanged();
    if (m_cloudMusicMode) scheduleCloudStateSave();
    emit toastRequested("音质设置已更新，下首歌曲生效");
}

void AppController::setPlayerStyle(const QString &style)
{
    static const QStringList allowed{
        QStringLiteral("balanced"), QStringLiteral("centered"),
        QStringLiteral("cover")
    };
    if (!allowed.contains(style) || style == m_playerStyle) return;
    m_playerStyle = style;
    if (m_profileSettings)
        m_profileSettings->setValue(QStringLiteral("playback/playerStyle"), m_playerStyle);
    emit playerStyleChanged();
}

void AppController::setDarkMode(bool dark)
{
    if (m_darkMode == dark) return;
    m_darkMode = dark;
    if (m_profileSettings) m_profileSettings->setValue("appearance/darkMode", m_darkMode);
    emit darkModeChanged();
    if (m_cloudMusicMode) scheduleCloudStateSave();
}

void AppController::setAccentColor(const QString &color)
{
    static const QStringList allowed{"#13D9B0", "#735CFF", "#FF5A76", "#F0A43B", "#4F8CFF",
                                     "#8A72F1", "#E76ACB", "#65A7FF", "#F1B85B", "#F06E6E",
                                     "#36D38B", "#8D72FF", "#13B89B"};
    if (!allowed.contains(color) || color == m_accentColor) return;
    m_accentColor = color;
    if (m_profileSettings) m_profileSettings->setValue("appearance/accentColor", m_accentColor);
    emit accentColorChanged();
    if (m_cloudMusicMode) scheduleCloudStateSave();
}

void AppController::setAnimationsEnabled(bool enabled)
{
    if (m_animationsEnabled == enabled) return;
    m_animationsEnabled = enabled;
    m_animationLevel = enabled ? QStringLiteral("rich") : QStringLiteral("off");
    if (m_profileSettings) {
        m_profileSettings->setValue("appearance/animationsEnabled", enabled);
        m_profileSettings->setValue("appearance/animationLevel", m_animationLevel);
    }
    emit animationsEnabledChanged();
    emit animationLevelChanged();
    if (m_cloudMusicMode) scheduleCloudStateSave();
}

void AppController::setCompactTrackRows(bool compact)
{
    if (m_compactTrackRows == compact) return;
    m_compactTrackRows = compact;
    if (m_profileSettings) m_profileSettings->setValue("appearance/compactTrackRows", compact);
    emit compactTrackRowsChanged();
    if (m_cloudMusicMode) scheduleCloudStateSave();
}

void AppController::setShowSourceBadges(bool show)
{
    if (m_showSourceBadges == show) return;
    m_showSourceBadges = show;
    if (m_profileSettings) m_profileSettings->setValue("appearance/showSourceBadges", show);
    emit showSourceBadgesChanged();
    if (m_cloudMusicMode) scheduleCloudStateSave();
}


void AppController::setLanguage(const QString &language)
{
    const QString value = QStringList{QStringLiteral("zh-CN"), QStringLiteral("en-US")}.contains(language)
        ? language : QStringLiteral("zh-CN");
    if (m_language == value) return;
    m_language = value;
    if (m_profileSettings) m_profileSettings->setValue(QStringLiteral("appearance/language"), value);
    emit languageChanged();
    if (m_cloudMusicMode) scheduleCloudStateSave();
}

void AppController::applyThemePresetValues(const QString &preset)
{
    if (preset == QStringLiteral("midnight")) {
        m_darkMode = true; m_accentColor = QStringLiteral("#4F8CFF");
    } else if (preset == QStringLiteral("forest")) {
        m_darkMode = true; m_accentColor = QStringLiteral("#36D38B");
    } else if (preset == QStringLiteral("violet")) {
        m_darkMode = true; m_accentColor = QStringLiteral("#8D72FF");
    } else if (preset == QStringLiteral("paper")) {
        m_darkMode = false; m_accentColor = QStringLiteral("#13B89B");
    } else if (preset == QStringLiteral("oled")) {
        m_darkMode = true; m_accentColor = QStringLiteral("#13D9B0");
    } else {
        m_darkMode = false; m_accentColor = QStringLiteral("#13D9B0");
    }
}

void AppController::setThemePreset(const QString &preset)
{
    const QStringList allowed{QStringLiteral("evolve"), QStringLiteral("midnight"),
                              QStringLiteral("forest"), QStringLiteral("violet"),
                              QStringLiteral("paper"), QStringLiteral("oled")};
    if (!allowed.contains(preset) || preset == m_themePreset) return;
    m_themePreset = preset;
    applyThemePresetValues(preset);
    if (m_profileSettings) {
        m_profileSettings->setValue(QStringLiteral("appearance/themePreset"), preset);
        m_profileSettings->setValue(QStringLiteral("appearance/darkMode"), m_darkMode);
        m_profileSettings->setValue(QStringLiteral("appearance/accentColor"), m_accentColor);
    }
    emit themePresetChanged();
    emit darkModeChanged();
    emit accentColorChanged();
    if (m_cloudMusicMode) scheduleCloudStateSave();
}

void AppController::setAnimationLevel(const QString &level)
{
    if (!QStringList{QStringLiteral("off"), QStringLiteral("balanced"), QStringLiteral("rich")}.contains(level)
        || m_animationLevel == level) return;
    m_animationLevel = level;
    m_animationsEnabled = level != QStringLiteral("off");
    if (m_profileSettings) {
        m_profileSettings->setValue(QStringLiteral("appearance/animationLevel"), level);
        m_profileSettings->setValue(QStringLiteral("appearance/animationsEnabled"), m_animationsEnabled);
    }
    emit animationLevelChanged();
    emit animationsEnabledChanged();
    if (m_cloudMusicMode) scheduleCloudStateSave();
}

void AppController::setRecommendationDiversity(int value)
{
    value = qBound(0, value, 100);
    if (m_recommendationDiversity == value) return;
    m_recommendationDiversity = value;
    if (m_profileSettings)
        m_profileSettings->setValue(QStringLiteral("playback/recommendationDiversity"), value);
    emit recommendationDiversityChanged();
    if (m_cloudMusicMode) scheduleCloudStateSave();
}

void AppController::setFontFamily(const QString &family)
{
    QString selected = family.trimmed();
    if (selected.isEmpty()) selected = QStringLiteral("系统默认");
    if (!m_availableFonts.contains(selected, Qt::CaseInsensitive))
        return;
    if (m_fontFamily == selected) return;
    m_fontFamily = selected;
    if (m_profileSettings) m_profileSettings->setValue(QStringLiteral("appearance/fontFamily"), selected);
    // Intentionally do not emit a QML NOTIFY signal here. The font selector
    // is restart-only; keeping the live QML tree untouched avoids the Qt 6.10
    // recursive re-evaluation that produced Maximum call stack size exceeded.
    // Font changes intentionally take effect on the next launch. Applying a
    // window/application font to a live Qt 6.10 QML tree can recursively
    // invalidate Text/Control font bindings and overflow the QML JS stack.
    emit toastRequested(m_language == QStringLiteral("en-US")
        ? QStringLiteral("Font saved. Restart EvolveMusic to apply it safely.")
        : QStringLiteral("字体已保存，重启 EvolveMusic 后安全生效。"));
}

void AppController::scanPlugins()
{
    QVariantList next;
    QDir root(m_pluginDirectory);
    const QFileInfoList folders = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &folder : folders) {
        QFile manifestFile(QDir(folder.absoluteFilePath()).filePath(QStringLiteral("manifest.json")));
        if (!manifestFile.open(QIODevice::ReadOnly)) continue;
        const QJsonDocument doc = QJsonDocument::fromJson(manifestFile.readAll());
        if (!doc.isObject()) continue;
        const QJsonObject obj = doc.object();
        QString id = obj.value(QStringLiteral("id")).toString().trimmed();
        QString entry = obj.value(QStringLiteral("entry")).toString(QStringLiteral("Main.qml")).trimmed();
        if (id.isEmpty()) id = folder.fileName();
        const QString absoluteEntry = QDir(folder.absoluteFilePath()).absoluteFilePath(entry);
        if (!QFileInfo::exists(absoluteEntry)) continue;
        QStringList permissions;
        for (const QJsonValue &value : obj.value(QStringLiteral("permissions")).toArray()) {
            const QString permission = value.toString().trimmed();
            if (!permission.isEmpty() && !permissions.contains(permission))
                permissions << permission;
        }
        QStringList granted;
        if (m_profileSettings) {
            granted = m_profileSettings->value(
                QStringLiteral("plugins/%1/grantedPermissions").arg(id)).toStringList();
        }
        for (auto it = granted.begin(); it != granted.end();) {
            if (!permissions.contains(*it)) it = granted.erase(it);
            else ++it;
        }
        const bool enabled = m_profileSettings
            ? m_profileSettings->value(QStringLiteral("plugins/%1/enabled").arg(id), false).toBool()
            : false;
        next << QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), obj.value(QStringLiteral("name")).toString(id)},
            {QStringLiteral("version"), obj.value(QStringLiteral("version")).toString(QStringLiteral("1.0.0"))},
            {QStringLiteral("author"), obj.value(QStringLiteral("author")).toString()},
            {QStringLiteral("description"), obj.value(QStringLiteral("description")).toString()},
            {QStringLiteral("entryUrl"), QUrl::fromLocalFile(absoluteEntry).toString()},
            {QStringLiteral("folder"), folder.absoluteFilePath()},
            {QStringLiteral("permissions"), permissions},
            {QStringLiteral("grantedPermissions"), granted},
            {QStringLiteral("capabilities"), obj.value(QStringLiteral("capabilities")).toVariant()},
            {QStringLiteral("contributes"), obj.value(QStringLiteral("contributes")).toVariant()},
            {QStringLiteral("enabled"), enabled && permissions.size() == granted.size()}
        };
    }
    m_plugins = next;
    emit pluginsChanged();
}

void AppController::reloadPlugins()
{
    scanPlugins();
    emit toastRequested(QStringLiteral("插件列表已重新扫描"));
}

void AppController::openPluginDirectory()
{
    QDir().mkpath(m_pluginDirectory);
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_pluginDirectory));
}

void AppController::setPluginEnabled(const QString &pluginId, bool enabled)
{
    const QString id = pluginId.trimmed();
    if (id.isEmpty() || !m_profileSettings) return;
    if (enabled) {
        for (const QVariant &entry : std::as_const(m_plugins)) {
            const QVariantMap plugin = entry.toMap();
            if (plugin.value(QStringLiteral("id")).toString() != id) continue;
            const QStringList requested = plugin.value(QStringLiteral("permissions")).toStringList();
            const QStringList granted = plugin.value(QStringLiteral("grantedPermissions")).toStringList();
            if (requested.size() != granted.size()) {
                emit toastRequested(QStringLiteral("请先授权该插件请求的全部权限"));
                return;
            }
            break;
        }
    }
    m_profileSettings->setValue(QStringLiteral("plugins/%1/enabled").arg(id), enabled);
    scanPlugins();
    emit toastRequested(enabled ? QStringLiteral("插件已启用") : QStringLiteral("插件已停用"));
}

void AppController::setPluginPermission(const QString &pluginId,
                                        const QString &permission,
                                        bool granted)
{
    const QString id = pluginId.trimmed();
    const QString key = permission.trimmed();
    if (id.isEmpty() || key.isEmpty() || !m_profileSettings) return;
    const QString setting = QStringLiteral("plugins/%1/grantedPermissions").arg(id);
    QStringList values = m_profileSettings->value(setting).toStringList();
    if (granted && !values.contains(key)) values << key;
    if (!granted) values.removeAll(key);
    m_profileSettings->setValue(setting, values);
    if (!granted)
        m_profileSettings->setValue(QStringLiteral("plugins/%1/enabled").arg(id), false);
    scanPlugins();
}

void AppController::createPluginTemplate()
{
    QDir root(m_pluginDirectory);
    root.mkpath(QStringLiteral("."));
    QString folderName = QStringLiteral("hello-evolve");
    int suffix = 2;
    while (root.exists(folderName))
        folderName = QStringLiteral("hello-evolve-%1").arg(suffix++);
    if (!root.mkpath(folderName)) {
        emit toastRequested(QStringLiteral("无法创建插件示例目录"));
        return;
    }

    QDir pluginDir(root.filePath(folderName));
    QFile manifest(pluginDir.filePath(QStringLiteral("manifest.json")));
    if (!manifest.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit toastRequested(QStringLiteral("无法写入插件 manifest.json"));
        return;
    }
    const QJsonObject manifestObject{
        {QStringLiteral("id"), folderName},
        {QStringLiteral("name"), QStringLiteral("Hello Evolve")},
        {QStringLiteral("version"), QStringLiteral("1.0.0")},
        {QStringLiteral("author"), m_cloudMusic.username()},
        {QStringLiteral("description"), QStringLiteral("EvolveMusic QML 插件示例")},
        {QStringLiteral("entry"), QStringLiteral("Main.qml")},
        {QStringLiteral("permissions"), QJsonArray{
             QStringLiteral("player.read"), QStringLiteral("player.control")}},
        {QStringLiteral("capabilities"), QJsonArray{QStringLiteral("background")}},
        {QStringLiteral("contributes"), QJsonObject{
             {QStringLiteral("commands"), QJsonArray{QStringLiteral("hello-evolve.toggle")}}}}
    };
    manifest.write(QJsonDocument(manifestObject).toJson(QJsonDocument::Indented));
    manifest.close();

    QFile qml(pluginDir.filePath(QStringLiteral("Main.qml")));
    if (!qml.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        emit toastRequested(QStringLiteral("无法写入插件 Main.qml"));
        return;
    }
    const QByteArray templateQml = R"QML(import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: app.darkMode ? "#24241F" : "#F7F8FA"
    radius: 14
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 10
        Text { text: "Hello Evolve"; color: app.darkMode ? "#F4F4F1" : "#202126"; font.pixelSize: 22; font.bold: true }
        Text { text: "这是一个本地 QML 插件示例。"; color: app.darkMode ? "#A0A19A" : "#737780"; font.pixelSize: 10 }
        Button { text: player.playing ? "暂停当前歌曲" : "继续播放"; onClicked: player.togglePlay() }
    }
}
)QML";
    qml.write(templateQml);
    qml.close();
    scanPlugins();
    emit toastRequested(QStringLiteral("已创建插件示例：") + folderName);
}

void AppController::applyCacheEnvironment()
{
    QString path = m_cacheDirectory.trimmed();
    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (path.isEmpty())
            path = QDir(QDir::tempPath()).filePath(QStringLiteral("EvolveMusic-cache"));
        path = QDir(path).filePath(QStringLiteral("EvolveMusic"));
    }
    QDir().mkpath(path);
    m_cacheDirectory = QDir::cleanPath(path);
    qputenv("EVOLVE_MUSIC_CACHE_DIR", QDir::toNativeSeparators(m_cacheDirectory).toUtf8());
}

qint64 AppController::directorySize(const QString &path) const
{
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

QString AppController::cacheSizeText() const
{
    const qint64 bytes = directorySize(m_cacheDirectory);
    if (bytes >= 1024ll * 1024ll * 1024ll)
        return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + QStringLiteral(" GB");
    if (bytes >= 1024ll * 1024ll)
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
}

QString AppController::chooseImageFile()
{
    const QString selected = QFileDialog::getOpenFileName(
        nullptr,
        QStringLiteral("选择图片"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QStringLiteral("图片 (*.png *.jpg *.jpeg *.webp);;所有文件 (*)"));
    if (selected.trimmed().isEmpty())
        return {};
    return QUrl::fromLocalFile(QDir::cleanPath(selected)).toString();
}

void AppController::chooseCacheDirectory()
{
    const QString selected = QFileDialog::getExistingDirectory(
        nullptr, QStringLiteral("选择 EvolveMusic 缓存文件夹"), m_cacheDirectory);
    if (selected.trimmed().isEmpty()) return;
    m_cacheDirectory = QDir::cleanPath(selected);
    if (m_profileSettings)
        m_profileSettings->setValue(QStringLiteral("cache/directory"), m_cacheDirectory);
    applyCacheEnvironment();
    emit cacheDirectoryChanged();
    emit toastRequested(QStringLiteral("缓存目录已更新；新请求立即使用，QML 图片缓存完全切换将在重启后生效。"));
}

void AppController::resetCacheDirectory()
{
    m_cacheDirectory.clear();
    if (m_profileSettings) m_profileSettings->remove(QStringLiteral("cache/directory"));
    applyCacheEnvironment();
    emit cacheDirectoryChanged();
}

void AppController::clearCache()
{
    if (m_cacheDirectory.trimmed().isEmpty()) return;
    QDir dir(m_cacheDirectory);
    const QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) QDir(entry.absoluteFilePath()).removeRecursively();
        else QFile::remove(entry.absoluteFilePath());
    }
    emit cacheDirectoryChanged();
    emit toastRequested(QStringLiteral("缓存已清理"));
}

QString AppController::imageDataUrl(const QString &filePath, int maxBytes) const
{
    QString path = filePath.trimmed();
    QUrl maybeUrl(path);
    if (maybeUrl.isLocalFile()) path = maybeUrl.toLocalFile();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QByteArray bytes = file.read(maxBytes + 1);
    if (bytes.isEmpty() || bytes.size() > maxBytes) return {};
    QString mime = QStringLiteral("image/jpeg");
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("png")) mime = QStringLiteral("image/png");
    else if (suffix == QStringLiteral("webp")) mime = QStringLiteral("image/webp");
    else if (suffix == QStringLiteral("gif")) mime = QStringLiteral("image/gif");
    return QStringLiteral("data:%1;base64,%2")
        .arg(mime, QString::fromLatin1(bytes.toBase64()));
}

void AppController::updateCustomPlaylistMetadata(const QString &playlistId,
                                                 const QString &name,
                                                 const QString &description,
                                                 const QString &coverFile,
                                                 bool isPublic)
{
    const int index = customPlaylistIndex(playlistId);
    if (index < 0) {
        emit toastRequested(QStringLiteral("只有歌单创建者可以修改歌单资料。"));
        return;
    }
    QVariantMap playlist = m_customPlaylists.at(index).toMap();
    const QString cleanName = name.trimmed().isEmpty() ? QStringLiteral("未命名歌单") : name.trimmed().left(48);
    QString cover = playlist.value(QStringLiteral("cover")).toString();
    if (!coverFile.trimmed().isEmpty()) {
        const QString uploaded = imageDataUrl(coverFile);
        if (uploaded.isEmpty()) {
            emit toastRequested(QStringLiteral("封面读取失败或超过 420 KB，请换一张较小的 JPG/PNG/WebP。"));
            return;
        }
        cover = uploaded;
    }
    playlist[QStringLiteral("name")] = cleanName;
    playlist[QStringLiteral("description")] = description.trimmed().left(240);
    playlist[QStringLiteral("cover")] = cover;
    playlist[QStringLiteral("isPublic")] = isPublic;
    m_customPlaylists[index] = playlist;
    emit customPlaylistsChanged();
    if (m_currentPlaylist.value(QStringLiteral("id")).toString() == playlistId) {
        m_currentPlaylist = playlist;
        emit currentPlaylistChanged();
    }
    m_cloudMusic.updateCustomPlaylistMetadata(
        playlistId, cleanName, playlist.value(QStringLiteral("description")).toString(),
        cover, isPublic);
}

bool AppController::ownsPlaylist(const QVariantMap &playlist) const
{
    const QString ownerId = playlist.value(QStringLiteral("ownerId")).toString();
    if (!ownerId.isEmpty()) return ownerId == m_cloudMusic.userId();
    return customPlaylistIndex(playlist.value(QStringLiteral("id")).toString()) >= 0;
}

void AppController::refreshSocialState()
{
    if (!m_cloudMusicMode || !m_cloudMusic.loggedIn()) return;
    m_cloudMusic.loadTogetherInvites();
}

void AppController::refreshPresence()
{
    if (m_cloudMusicMode && m_cloudMusic.loggedIn())
        m_cloudMusic.heartbeatPresence();
}

void AppController::loadCommunityPlaylists()
{
    if (m_cloudMusicMode) m_cloudMusic.loadCommunityPlaylists(28);
}

void AppController::openCommunityPlaylist(const QString &playlistId)
{
    if (!m_cloudMusicMode || playlistId.trimmed().isEmpty()) return;
    for (const QVariant &entry : std::as_const(m_communityPlaylists)) {
        QVariantMap playlist = entry.toMap();
        if (playlist.value(QStringLiteral("id")).toString() != playlistId) continue;
        playlist[QStringLiteral("providerId")] = QStringLiteral("evolve");
        playlist[QStringLiteral("loading")] = true;
        const bool editable = ownsPlaylist(playlist);
        playlist[QStringLiteral("editable")] = editable;
        playlist[QStringLiteral("custom")] = editable;
        m_currentPlaylist = playlist;
        m_playlistTracks = playlist.value(QStringLiteral("tracks")).toList();
        emit currentPlaylistChanged();
        emit playlistTracksChanged();
        emit playlistOpened();
        break;
    }
    setLoading(true);
    m_cloudMusic.loadCommunityPlaylist(playlistId);
}

void AppController::loadUserProfile()
{
    if (!m_cloudMusicMode) return;
    m_cloudMusic.loadProfile();
    m_cloudMusic.loadFollowing();
    m_cloudMusic.loadTogetherInvites();
}

void AppController::updateUserProfile(const QString &displayName,
                                      const QString &bio,
                                      const QString &avatarFile)
{
    QString avatar;
    if (!avatarFile.trimmed().isEmpty()) {
        avatar = imageDataUrl(avatarFile, 360 * 1024);
        if (avatar.isEmpty()) {
            emit toastRequested(QStringLiteral("头像读取失败或超过 360 KB。"));
            return;
        }
    }
    m_cloudMusic.updateProfile(displayName.trimmed().left(32), bio.trimmed().left(180), avatar);
}

void AppController::searchUsers(const QString &query)
{
    m_cloudMusic.searchUsers(query);
}

void AppController::setFollowing(const QString &userId, bool follow)
{
    for (int i = 0; i < m_userSearchResults.size(); ++i) {
        QVariantMap row = m_userSearchResults.at(i).toMap();
        if (row.value(QStringLiteral("id")).toString() == userId) {
            row[QStringLiteral("following")] = follow;
            m_userSearchResults[i] = row;
            emit userSearchResultsChanged();
            break;
        }
    }
    m_cloudMusic.setFollowing(userId, follow);
    QTimer::singleShot(350, this, [this] { m_cloudMusic.loadProfile(); });
}

void AppController::createTogetherRoom()
{
    m_cloudMusic.createTogetherRoom();
}

void AppController::joinTogetherRoom(const QString &code)
{
    m_cloudMusic.joinTogetherRoom(code);
}

void AppController::leaveTogetherRoom()
{
    const QString code = m_togetherRoom.value(QStringLiteral("code")).toString();
    m_togetherPollTimer.stop();
    m_togetherPublishTimer.stop();
    m_cloudMusic.leaveTogetherRoom(code);
    // The room channel is ephemeral. Wipe chat/audio/attachment state locally at
    // the moment the user disconnects instead of waiting for the network reply.
    clearTogetherEphemeral();
    m_togetherRoom.clear();
    emit togetherRoomChanged();
}

void AppController::inviteTogetherUser(const QString &username)
{
    const QString code = m_togetherRoom.value(QStringLiteral("code")).toString();
    if (!code.isEmpty()) m_cloudMusic.inviteTogetherUser(code, username);
}

void AppController::dismissTogetherInvite(const QString &inviteId)
{
    if (!inviteId.trimmed().isEmpty()) m_cloudMusic.dismissTogetherInvite(inviteId);
}

void AppController::refreshTogetherRoom()
{
    const QString code = m_togetherRoom.value(QStringLiteral("code")).toString();
    if (!code.isEmpty()) m_cloudMusic.loadTogetherRoom(code);
}

void AppController::publishTogetherState()
{
    if (!m_togetherRoom.value(QStringLiteral("isHost")).toBool()) return;
    const QString code = m_togetherRoom.value(QStringLiteral("code")).toString();
    const QVariantMap track = persistentTrack(m_player.currentTrack());
    if (code.isEmpty() || track.isEmpty()) return;
    m_cloudMusic.updateTogetherState(code, QVariantMap{
        {QStringLiteral("track"), track},
        {QStringLiteral("position"), m_player.position()},
        {QStringLiteral("playing"), m_player.playing()},
        {QStringLiteral("updatedAt"), QDateTime::currentMSecsSinceEpoch()}
    });
}


void AppController::refreshTogetherMessages()
{
    const QString code = m_togetherRoom.value(QStringLiteral("code")).toString();
    if (!code.isEmpty() && m_cloudMusic.ready())
        m_cloudMusic.loadTogetherMessages(code, m_togetherMessageCursor);
}

void AppController::refreshTogetherVoice()
{
    const QString code = m_togetherRoom.value(QStringLiteral("code")).toString();
    if (!code.isEmpty() && m_cloudMusic.ready())
        m_cloudMusic.loadTogetherVoiceChunks(code, m_togetherVoiceCursor);
}

QString AppController::togetherTempDirectory() const
{
    QString root = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (root.isEmpty()) root = QDir::tempPath();
    return QDir(root).filePath(QStringLiteral("EvolveMusic/together-ephemeral"));
}

void AppController::clearTogetherEphemeral()
{
    m_togetherChatPollTimer.stop();
    m_togetherVoicePollTimer.stop();
    setTogetherMicrophone(false);
    if (m_togetherVoiceSink) {
        m_togetherVoiceSink->stop();
        m_togetherVoiceSink->deleteLater();
        m_togetherVoiceSink = nullptr;
        m_togetherVoiceOutput = nullptr;
    }
    m_togetherMessages.clear();
    m_togetherAttachmentUrls.clear();
    m_togetherAttachmentDownloads.clear();
    m_togetherPendingSavePaths.clear();
    m_togetherPendingOpen.clear();
    m_togetherMessageCursor = 0;
    m_togetherVoiceCursor = 0;
    m_togetherVoiceCaptureBuffer.clear();
    m_togetherVoiceError.clear();
    m_togetherMicLevel = 0.0;
    QDir(togetherTempDirectory()).removeRecursively();
    emit togetherMessagesChanged();
    emit togetherAttachmentUrlsChanged();
    emit togetherVoiceChanged();
}

void AppController::sendTogetherText(const QString &text)
{
    const QString code = m_togetherRoom.value(QStringLiteral("code")).toString();
    const QString clean = text.trimmed().left(2000);
    if (code.isEmpty() || clean.isEmpty()) return;
    m_cloudMusic.sendTogetherText(code, clean);
}

void AppController::sendTogetherImage()
{
    const QString code = m_togetherRoom.value(QStringLiteral("code")).toString();
    if (code.isEmpty()) {
        emit toastRequested(QStringLiteral("请先加入一起听房间"));
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        nullptr, QStringLiteral("发送图片"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QStringLiteral("图片 (*.png *.jpg *.jpeg *.webp *.bmp);;所有文件 (*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit toastRequested(QStringLiteral("图片读取失败"));
        return;
    }
    QByteArray bytes = file.readAll();
    file.close();
    QString fileName = QFileInfo(path).fileName();
    QString mimeType = QMimeDatabase().mimeTypeForFile(path).name();

    // Keep images usable even when the Worker has no ROOM_FILES/R2 binding.
    // Large photos are resized/re-encoded under the D1 inline fallback ceiling.
    constexpr int inlineTarget = 550 * 1024;
    if (bytes.size() > inlineTarget) {
        QImage image(path);
        if (image.isNull()) {
            emit toastRequested(QStringLiteral("图片格式无法读取"));
            return;
        }
        if (image.width() > 1800 || image.height() > 1800)
            image = image.scaled(1800, 1800, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QByteArray encoded;
        for (int pass = 0; pass < 7; ++pass) {
            encoded.clear();
            QBuffer buffer(&encoded);
            buffer.open(QIODevice::WriteOnly);
            const int quality = qMax(42, 88 - pass * 8);
            image.save(&buffer, "JPG", quality);
            if (encoded.size() <= inlineTarget) break;
            image = image.scaled(qMax(640, int(image.width() * 0.84)),
                                 qMax(640, int(image.height() * 0.84)),
                                 Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        if (encoded.isEmpty() || encoded.size() > 650 * 1024) {
            emit toastRequested(QStringLiteral("图片优化后仍然太大，请换一张较小的图片"));
            return;
        }
        bytes = encoded;
        fileName = QFileInfo(path).completeBaseName().left(120) + QStringLiteral(".jpg");
        mimeType = QStringLiteral("image/jpeg");
    }
    m_cloudMusic.uploadTogetherAttachment(code, QStringLiteral("image"), fileName, mimeType, bytes);
}

void AppController::sendTogetherFile()
{
    const QString code = m_togetherRoom.value(QStringLiteral("code")).toString();
    if (code.isEmpty()) {
        emit toastRequested(QStringLiteral("请先加入一起听房间"));
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        nullptr, QStringLiteral("发送文件"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStringLiteral("所有文件 (*)"));
    if (path.isEmpty()) return;
    QFileInfo info(path);
    constexpr qint64 maxFile = 20ll * 1024ll * 1024ll;
    if (info.size() <= 0 || info.size() > maxFile) {
        emit toastRequested(QStringLiteral("房间文件最大 20 MB"));
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit toastRequested(QStringLiteral("文件读取失败"));
        return;
    }
    const QByteArray bytes = file.readAll();
    const QString mimeType = QMimeDatabase().mimeTypeForFile(path).name();
    m_cloudMusic.uploadTogetherAttachment(code, QStringLiteral("file"), info.fileName(), mimeType, bytes);
}

void AppController::openTogetherAttachment(const QString &attachmentId, const QString &fileName)
{
    const QString id = attachmentId.trimmed();
    if (id.isEmpty()) return;
    const QString local = m_togetherAttachmentUrls.value(id).toString();
    if (!local.isEmpty()) {
        QDesktopServices::openUrl(QUrl(local));
        return;
    }
    m_togetherPendingOpen.insert(id);
    if (!m_togetherAttachmentDownloads.contains(id)) {
        m_togetherAttachmentDownloads.insert(id);
        m_cloudMusic.downloadTogetherAttachment(
            m_togetherRoom.value(QStringLiteral("code")).toString(), id);
    }
    Q_UNUSED(fileName);
}

void AppController::saveTogetherAttachment(const QString &attachmentId, const QString &fileName)
{
    const QString id = attachmentId.trimmed();
    if (id.isEmpty()) return;
    QString suggested = QFileInfo(fileName).fileName();
    if (suggested.isEmpty()) suggested = QStringLiteral("EvolveMusic-file");
    const QString target = QFileDialog::getSaveFileName(
        nullptr, QStringLiteral("保存房间文件"),
        QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).filePath(suggested));
    if (target.isEmpty()) return;
    const QString localUrl = m_togetherAttachmentUrls.value(id).toString();
    const QString localPath = QUrl(localUrl).toLocalFile();
    if (!localPath.isEmpty() && QFileInfo::exists(localPath)) {
        QFile::remove(target);
        if (QFile::copy(localPath, target))
            emit toastRequested(QStringLiteral("文件已保存"));
        else
            emit toastRequested(QStringLiteral("文件保存失败"));
        return;
    }
    m_togetherPendingSavePaths.insert(id, target);
    if (!m_togetherAttachmentDownloads.contains(id)) {
        m_togetherAttachmentDownloads.insert(id);
        m_cloudMusic.downloadTogetherAttachment(
            m_togetherRoom.value(QStringLiteral("code")).toString(), id);
    }
}

void AppController::cacheTogetherAttachment(const QString &attachmentId,
                                            const QByteArray &bytes,
                                            const QString &fileName,
                                            const QString &mimeType)
{
    if (attachmentId.isEmpty() || bytes.isEmpty()) return;
    QDir dir(togetherTempDirectory());
    if (!dir.exists()) dir.mkpath(QStringLiteral("."));
    QString suffix = QFileInfo(fileName).suffix().toLower();
    if (suffix.isEmpty()) {
        const QStringList candidates = QMimeDatabase().mimeTypeForName(mimeType).suffixes();
        suffix = candidates.value(0);
    }
    QString cacheName = attachmentId;
    if (!suffix.isEmpty()) cacheName += QLatin1Char('.') + suffix.left(12);
    const QString cachePath = dir.filePath(cacheName);
    QSaveFile out(cachePath);
    if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size() || !out.commit()) {
        emit toastRequested(QStringLiteral("临时附件写入失败"));
        return;
    }
    m_togetherAttachmentUrls[attachmentId] = QUrl::fromLocalFile(cachePath).toString();
    emit togetherAttachmentUrlsChanged();

    const QString savePath = m_togetherPendingSavePaths.take(attachmentId);
    if (!savePath.isEmpty()) {
        QSaveFile save(savePath);
        if (save.open(QIODevice::WriteOnly) && save.write(bytes) == bytes.size() && save.commit())
            emit toastRequested(QStringLiteral("文件已保存"));
        else
            emit toastRequested(QStringLiteral("文件保存失败"));
    }
    if (m_togetherPendingOpen.remove(attachmentId) > 0)
        QDesktopServices::openUrl(QUrl::fromLocalFile(cachePath));
}

QString AppController::formatFileSize(qint64 bytes) const
{
    if (bytes >= 1024ll * 1024ll)
        return QString::number(bytes / (1024.0 * 1024.0), 'f', bytes >= 10ll * 1024ll * 1024ll ? 0 : 1) + QStringLiteral(" MB");
    if (bytes >= 1024)
        return QString::number(bytes / 1024.0, 'f', bytes >= 100ll * 1024ll ? 0 : 1) + QStringLiteral(" KB");
    return QString::number(qMax<qint64>(0, bytes)) + QStringLiteral(" B");
}

void AppController::ensureTogetherVoiceOutput()
{
    if (m_togetherVoiceSink && m_togetherVoiceOutput) return;
    const QAudioDevice output = QMediaDevices::defaultAudioOutput();
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    if (output.isNull() || !output.isFormatSupported(format)) {
        const QString error = QStringLiteral("当前输出设备不支持房间语音所需的 16 kHz 单声道格式");
        if (m_togetherVoiceError != error) {
            m_togetherVoiceError = error;
            emit togetherVoiceChanged();
        }
        return;
    }
    m_togetherVoiceSink = new QAudioSink(output, format, this);
    m_togetherVoiceSink->setBufferSize(128 * 1024);
    m_togetherVoiceOutput = m_togetherVoiceSink->start();
    if (!m_togetherVoiceOutput) {
        m_togetherVoiceSink->deleteLater();
        m_togetherVoiceSink = nullptr;
        m_togetherVoiceError = QStringLiteral("无法打开扬声器进行房间语音播放");
        emit togetherVoiceChanged();
    }
}

void AppController::setTogetherMicrophone(bool enabled)
{
    if (!enabled) {
        if (m_togetherVoiceSource) {
            m_togetherVoiceSource->stop();
            m_togetherVoiceSource->deleteLater();
        }
        m_togetherVoiceSource = nullptr;
        m_togetherVoiceInput = nullptr;
        m_togetherVoiceCaptureBuffer.clear();
        m_togetherVoiceHangoverFrames = 0;
        const bool changed = m_togetherMicActive || m_togetherMicLevel != 0.0;
        m_togetherMicActive = false;
        m_togetherMicLevel = 0.0;
        if (changed) emit togetherVoiceChanged();
        return;
    }
    if (m_togetherMicActive) return;
    if (m_togetherRoom.value(QStringLiteral("code")).toString().isEmpty()) {
        m_togetherVoiceError = QStringLiteral("请先加入一起听房间");
        emit togetherVoiceChanged();
        return;
    }

    const QAudioDevice input = QMediaDevices::defaultAudioInput();
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    if (input.isNull() || !input.isFormatSupported(format)) {
        m_togetherVoiceError = QStringLiteral("当前麦克风不支持 16 kHz 单声道语音格式");
        emit togetherVoiceChanged();
        return;
    }
    m_togetherVoiceSource = new QAudioSource(input, format, this);
    m_togetherVoiceSource->setBufferSize(64 * 1024);
    m_togetherVoiceInput = m_togetherVoiceSource->start();
    if (!m_togetherVoiceInput) {
        m_togetherVoiceSource->deleteLater();
        m_togetherVoiceSource = nullptr;
        m_togetherVoiceError = QStringLiteral("麦克风启动失败，请检查 Windows 麦克风权限");
        emit togetherVoiceChanged();
        return;
    }
    m_togetherVoiceError.clear();
    m_togetherMicActive = true;
    emit togetherVoiceChanged();

    connect(m_togetherVoiceInput, &QIODevice::readyRead, this, [this] {
        if (!m_togetherVoiceInput || !m_togetherMicActive) return;
        const QByteArray incoming = m_togetherVoiceInput->readAll();
        if (incoming.isEmpty()) return;
        m_togetherVoiceCaptureBuffer.append(incoming);

        const qint16 *samples = reinterpret_cast<const qint16 *>(incoming.constData());
        const qsizetype sampleCount = incoming.size() / qsizetype(sizeof(qint16));
        qint32 peak = 0;
        for (qsizetype i = 0; i < sampleCount; ++i)
            peak = qMax<qint32>(peak, qAbs(qint32(samples[i])));
        const double level = qBound(0.0, peak / 32768.0, 1.0);
        if (qAbs(level - m_togetherMicLevel) > 0.025) {
            m_togetherMicLevel = level;
            emit togetherVoiceChanged();
        }

        // 480 ms @ 16 kHz, mono, signed 16-bit. This keeps D1 relay traffic
        // moderate while maintaining conversational latency for small rooms.
        constexpr qsizetype frameBytes = 16000 * 2 * 480 / 1000;
        while (m_togetherVoiceCaptureBuffer.size() >= frameBytes) {
            const QByteArray pcm = m_togetherVoiceCaptureBuffer.left(frameBytes);
            m_togetherVoiceCaptureBuffer.remove(0, frameBytes);
            const qint16 *frameSamples = reinterpret_cast<const qint16 *>(pcm.constData());
            const qsizetype frameSampleCount = pcm.size() / qsizetype(sizeof(qint16));
            qint32 framePeak = 0;
            for (qsizetype i = 0; i < frameSampleCount; ++i)
                framePeak = qMax<qint32>(framePeak, qAbs(qint32(frameSamples[i])));
            const bool voiceDetected = framePeak > 620; // about -34 dBFS
            if (voiceDetected) m_togetherVoiceHangoverFrames = 2;
            else if (m_togetherVoiceHangoverFrames > 0) --m_togetherVoiceHangoverFrames;
            if (!voiceDetected && m_togetherVoiceHangoverFrames <= 0) continue;

            const QByteArray compressed = qCompress(pcm, 1);
            m_cloudMusic.sendTogetherVoiceChunk(
                m_togetherRoom.value(QStringLiteral("code")).toString(), compressed);
        }
    });
}

QString AppController::uiText(const QString &key) const
{
    if (m_language != QStringLiteral("en-US")) {
        static const QHash<QString, QString> zh{
            {QStringLiteral("recommend"), QStringLiteral("推荐")},
            {QStringLiteral("discover"), QStringLiteral("发现")},
            {QStringLiteral("favorites"), QStringLiteral("我喜欢的")},
            {QStringLiteral("history"), QStringLiteral("最近播放")},
            {QStringLiteral("playing"), QStringLiteral("正在播放")},
            {QStringLiteral("nowPlaying"), QStringLiteral("正在播放")},
            {QStringLiteral("together"), QStringLiteral("一起听")},
            {QStringLiteral("profile"), QStringLiteral("个人主页")},
            {QStringLiteral("settings"), QStringLiteral("设置")},
            {QStringLiteral("loginTitle"), QStringLiteral("欢迎回来")},
            {QStringLiteral("registerTitle"), QStringLiteral("创建 EvolveMusic 账号")}
        };
        return zh.value(key, key);
    }
    static const QHash<QString, QString> en{
        {QStringLiteral("recommend"), QStringLiteral("For You")},
        {QStringLiteral("discover"), QStringLiteral("Discover")},
        {QStringLiteral("favorites"), QStringLiteral("Liked Songs")},
        {QStringLiteral("history"), QStringLiteral("Recently Played")},
        {QStringLiteral("playing"), QStringLiteral("Now Playing")},
        {QStringLiteral("nowPlaying"), QStringLiteral("Now Playing")},
        {QStringLiteral("together"), QStringLiteral("Listen Together")},
        {QStringLiteral("profile"), QStringLiteral("Profile")},
        {QStringLiteral("settings"), QStringLiteral("Settings")},
        {QStringLiteral("loginTitle"), QStringLiteral("Welcome back")},
        {QStringLiteral("registerTitle"), QStringLiteral("Create EvolveMusic account")}
    };
    return en.value(key, key);
}

QString AppController::formatDuration(qint64 ms) const
{
    const qint64 total = ms / 1000;
    return QString("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QChar('0'));
}

QString AppController::formatCount(qint64 count) const
{
    if (count >= 100000000) return QString::number(count / 100000000.0, 'f', 1) + "亿";
    if (count >= 10000) return QString::number(count / 10000.0, 'f', 1) + "万";
    return QString::number(count);
}

QString AppController::accessLabel(const QString &access) const
{
    const QString a = access.toLower();
    if (a == "free") return "免费可播";
    if (a == "account") return "登录可播";
    if (a == "vip") return "会员来源";
    if (a == "restricted") return "受限来源";
    if (a == "unavailable") return "当前不可播";
    if (a == "cloud") return "云端播放";
    return "可用性待解析";
}

QString AppController::coverDirectUrl(const QString &rawUrl, int pixelSize) const
{
    if (rawUrl.trimmed().isEmpty()) return {};

    QUrl url(rawUrl.trimmed());
    if (!url.isValid()) return rawUrl;

    const QString host = url.host().toLower();
    pixelSize = qBound(48, pixelSize, 1200);

    if (host.endsWith(QStringLiteral("music.126.net"))
        || host.endsWith(QStringLiteral("126.net"))) {
        // Playlist search covers can include a complete NetEase watermark
        // transformation pipeline in the query string. Appending our own
        // imageView parameters to that pipeline produces HTTP 400. The path is
        // already the immutable image identity, so discard all upstream image
        // transformations and rebuild one small JPEG request from scratch.
        url.setQuery(QString());
        QUrlQuery query;
        // NetEase currently serves RIFF/WebP bytes from many .jpg URLs even
        // when the request carries ?param=. The Qt bundle used by the desktop
        // app has JPEG support but no WebP imageformat plugin. imageView+type
        // makes the CDN return real JFIF/JPEG bytes, so artwork renders in the
        // app and in packaged builds without adding another native codec.
        query.addQueryItem(QStringLiteral("imageView"), QStringLiteral("1"));
        query.addQueryItem(
            QStringLiteral("thumbnail"),
            QString::number(pixelSize)
                + QStringLiteral("y")
                + QString::number(pixelSize));
        query.addQueryItem(QStringLiteral("type"), QStringLiteral("jpg"));
        url.setQuery(query);
    }

    return url.toString(QUrl::FullyEncoded);
}

QString AppController::coverUrl(const QString &rawUrl, int pixelSize) const
{
    if (rawUrl.trimmed().isEmpty()) return {};

    QUrl url(coverDirectUrl(rawUrl, pixelSize));
    if (!url.isValid()) return rawUrl;

    const QString host = url.host().toLower();
    pixelSize = qBound(48, pixelSize, 1200);

    // QML's Image loader used to connect to NetEase artwork hosts directly.
    // On some networks those hosts fail while the Worker API is perfectly
    // reachable, leaving every card as a music-note placeholder. In cloud mode
    // route only approved NetEase artwork through the Worker's restricted image
    // proxy; arbitrary third-party URLs are still loaded normally.
    const bool neteaseArtwork =
        host == QStringLiteral("music.126.net")
        || host.endsWith(QStringLiteral(".music.126.net"))
        || host == QStringLiteral("126.net")
        || host.endsWith(QStringLiteral(".126.net"))
        || host == QStringLiteral("163.com")
        || host.endsWith(QStringLiteral(".163.com"));

    if (m_cloudMusicMode
        && neteaseArtwork
        && !m_cloudMusic.baseUrl().isEmpty()) {
        QUrl proxy(
            m_cloudMusic.baseUrl()
            + QStringLiteral("/v1/artwork"));
        QUrlQuery proxyQuery;
        proxyQuery.addQueryItem(
            QStringLiteral("url"),
            url.toString());
        proxyQuery.addQueryItem(
            QStringLiteral("size"),
            QString::number(pixelSize));
        proxy.setQuery(proxyQuery);
        return proxy.toString(QUrl::FullyEncoded);
    }

    return url.toString(QUrl::FullyEncoded);
}

void AppController::beginFastStreamRace(const QVariantMap &track)
{
    const QString trackId = track.value(QStringLiteral("id")).toString().trimmed();
    if (trackId.isEmpty()) {
        setError(QStringLiteral("歌曲标识为空，无法开始播放"));
        emit toastRequested(m_errorMessage);
        return;
    }

    m_fastStreamPreferenceTimer.stop();
    m_fastStreamTrackId = trackId;
    m_fastStreamProvisional.clear();
    m_fastStreamErrors.clear();
    m_fastStreamSettled = false;
    m_fastStreamCloudPending = false;
    m_fastStreamLocalPending = true;
    m_cloudOuterFallbackAttempted = false;
    m_cloudPlaybackFallbackAttempted = false;

    // v0.18.0: Evolve Cloud no longer resolves music. The three Ourcraft
    // platform adapters race inside MultiSourceManager; website providers are
    // optional fallback only.
    QStringList excludedProviders;
    if (!m_manualPlaybackProviderId.isEmpty()) {
        const QVariantList states = m_sources.providerStates();
        for (const QVariant &entry : states) {
            const QString id = entry.toMap().value(QStringLiteral("id")).toString();
            if (!id.isEmpty() && id != m_manualPlaybackProviderId)
                excludedProviders.append(id);
        }
    }
    m_sources.resolveBestStream(track, m_qualityLevel, excludedProviders);
}

QVariantList AppController::playbackSourceChoices() const
{
    QVariantList choices;
    choices.append(QVariantMap{
        {QStringLiteral("value"), QString()},
        {QStringLiteral("label"), QStringLiteral("自动选择")},
        {QStringLiteral("detail"), m_activeSourceName.isEmpty()
             ? QStringLiteral("并发选择最快可用渠道")
             : QStringLiteral("当前使用：") + m_activeSourceName}
    });

    QSet<QString> concreteProviders;
    for (const QVariant &entry : m_player.currentTrack().value(QStringLiteral("sources")).toList()) {
        const QVariantMap source = entry.toMap();
        if (source.value(QStringLiteral("playable"), true).toBool()) {
            const QString id = source.value(QStringLiteral("providerId")).toString();
            if (!id.isEmpty()) concreteProviders.insert(id);
        }
    }

    const QVariantList states = m_sources.providerStates();
    for (const QVariant &entry : states) {
        const QVariantMap state = entry.toMap();
        const QString id = state.value(QStringLiteral("id")).toString();
        if (id != QStringLiteral("netease")
            && id != QStringLiteral("kugou")
            && id != QStringLiteral("kuwo")) {
            continue;
        }
        if (!state.value(QStringLiteral("enabled"), true).toBool()
            || !state.value(QStringLiteral("usable"), true).toBool()) {
            continue;
        }
        QString label = state.value(QStringLiteral("name")).toString();
        if (label.isEmpty()) {
            label = id == QStringLiteral("netease") ? QStringLiteral("网易云音乐")
                  : (id == QStringLiteral("kugou") ? QStringLiteral("酷狗音乐")
                                                   : QStringLiteral("酷我音乐"));
        }
        const bool concrete = concreteProviders.contains(id);
        const bool active = m_activeSourceProviderId == id;
        choices.append(QVariantMap{
            {QStringLiteral("value"), id},
            {QStringLiteral("label"), label},
            {QStringLiteral("detail"), active
                 ? QStringLiteral("当前播放渠道")
                 : (concrete ? QStringLiteral("当前歌曲已匹配")
                             : QStringLiteral("切换时在线查找"))}
        });
    }
    return choices;
}

void AppController::selectPlaybackSource(const QString &providerId)
{
    QString selected = providerId.trimmed().toLower();
    if (selected == QStringLiteral("auto")) selected.clear();
    if (!selected.isEmpty()
        && selected != QStringLiteral("netease")
        && selected != QStringLiteral("kugou")
        && selected != QStringLiteral("kuwo")) {
        emit toastRequested(QStringLiteral("这个音源渠道不可用"));
        return;
    }

    const QVariantMap track = m_player.currentTrack();
    if (track.isEmpty()) {
        emit toastRequested(QStringLiteral("请先选择一首歌曲"));
        return;
    }

    m_manualPlaybackProviderId = selected;
    emit playbackSourcePreferenceChanged();
    m_activeSourceName.clear();
    m_activeSourceAccess.clear();
    m_activeSourceProviderId.clear();
    emit activeSourceChanged();
    beginFastStreamRace(track);

    const QString sourceName = selected == QStringLiteral("netease")
        ? QStringLiteral("网易云音乐")
        : (selected == QStringLiteral("kugou")
               ? QStringLiteral("酷狗音乐")
               : QStringLiteral("酷我音乐"));
    emit toastRequested(selected.isEmpty()
        ? QStringLiteral("已恢复自动选择音源")
        : QStringLiteral("正在切换音源：") + sourceName);
}

void AppController::considerFastStreamResult(const QVariantMap &result,
                                             bool preferred)
{
    const QString trackId = result.value(QStringLiteral("trackId")).toString();
    if (trackId.isEmpty()
        || trackId != m_fastStreamTrackId
        || trackId != m_player.currentTrack().value(QStringLiteral("id")).toString()) {
        return;
    }

    const QStringList urls = result.value(QStringLiteral("urls")).toStringList();
    if (urls.isEmpty() || urls.first().trimmed().isEmpty()) {
        maybeFinishFastStreamRace();
        return;
    }

    if (m_fastStreamSettled) {
        // A late audio resolver is still valuable: add it as a hot failover
        // candidate without interrupting the source that already started.
        if (!result.value(QStringLiteral("web")).toBool())
            m_player.appendResolvedSources(trackId, urls);
        return;
    }

    if (preferred) {
        commitFastStreamResult(result);
        return;
    }

    if (m_fastStreamProvisional.isEmpty())
        m_fastStreamProvisional = result;

    if (m_fastStreamCloudPending) {
        if (!m_fastStreamPreferenceTimer.isActive())
            m_fastStreamPreferenceTimer.start();
        return;
    }

    commitFastStreamResult(m_fastStreamProvisional);
}

void AppController::commitFastStreamResult(const QVariantMap &result)
{
    if (m_fastStreamSettled || result.isEmpty())
        return;

    const QString trackId = result.value(QStringLiteral("trackId")).toString();
    if (trackId.isEmpty()
        || trackId != m_fastStreamTrackId
        || trackId != m_player.currentTrack().value(QStringLiteral("id")).toString()) {
        return;
    }

    const QStringList urls = result.value(QStringLiteral("urls")).toStringList();
    if (urls.isEmpty() || urls.first().trimmed().isEmpty())
        return;

    m_fastStreamSettled = true;
    m_fastStreamPreferenceTimer.stop();
    m_fastStreamProvisional.clear();

    const QString providerId = result.value(QStringLiteral("providerId")).toString();
    const QString providerName = result.value(QStringLiteral("providerName")).toString();
    const QString access = result.value(QStringLiteral("access"), QStringLiteral("unknown")).toString();
    const QString origin = result.value(QStringLiteral("origin")).toString();
    const bool web = result.value(QStringLiteral("web")).toBool();

    m_activeSourceName = providerName;
    m_activeSourceAccess = access;
    m_activeSourceProviderId = providerId;
    emit activeSourceChanged();
    emit playbackSourcePreferenceChanged();

    if (web) {
        m_player.beginWebPlayback(trackId, urls.first());
        m_sources.loadBestLyrics(m_player.currentTrack());
        emit toastRequested(QStringLiteral("已选择最快可用源 · ")
                            + providerName + QStringLiteral(" · 网页壳播放"));
        return;
    }

    m_player.setResolvedSources(trackId, urls);
    if (origin == QStringLiteral("local"))
        m_sources.loadBestLyrics(m_player.currentTrack());

    if (origin == QStringLiteral("cloud")) {
        emit toastRequested(urls.size() > 1
            ? QStringLiteral("极速播放 · 云端最快节点已就绪，并预留备用线路")
            : QStringLiteral("极速播放 · ") + providerName);
    } else {
        emit toastRequested(QStringLiteral("极速播放 · ") + providerName
                            + QStringLiteral(" · ") + accessLabel(access));
    }
}

void AppController::markFastStreamBranchFailed(bool cloudBranch,
                                               const QString &message)
{
    const QString currentId = m_player.currentTrack().value(QStringLiteral("id")).toString();
    if (currentId.isEmpty() || currentId != m_fastStreamTrackId)
        return;

    if (cloudBranch)
        m_fastStreamCloudPending = false;
    else
        m_fastStreamLocalPending = false;

    const QString detail = message.trimmed();
    if (!detail.isEmpty())
        m_fastStreamErrors << (cloudBranch
            ? QStringLiteral("云端：") + detail
            : QStringLiteral("本机/备用源：") + detail);

    maybeFinishFastStreamRace();
}

void AppController::maybeFinishFastStreamRace()
{
    if (m_fastStreamSettled)
        return;

    if (!m_fastStreamProvisional.isEmpty() && !m_fastStreamCloudPending) {
        commitFastStreamResult(m_fastStreamProvisional);
        return;
    }

    if (m_fastStreamCloudPending || m_fastStreamLocalPending)
        return;

    if (!m_fastStreamProvisional.isEmpty()) {
        commitFastStreamResult(m_fastStreamProvisional);
        return;
    }

    const QVariantMap track = m_player.currentTrack();
    const QString trackId = track.value(QStringLiteral("id")).toString();
    const QString providerId = track.value(
        QStringLiteral("providerId"), QStringLiteral("netease")).toString();
    const QString sourceId = track.value(QStringLiteral("sourceId")).toString().trimmed();

    // Last cheap official fallback after every real resolver has already
    // failed. This is not attempted ahead of the race because restricted songs
    // often reject outer-url and would otherwise delay working sources.
    if (!m_cloudOuterFallbackAttempted
        && (m_manualPlaybackProviderId.isEmpty()
            || m_manualPlaybackProviderId == QStringLiteral("netease"))
        && providerId == QStringLiteral("netease")
        && !sourceId.isEmpty()) {
        m_cloudOuterFallbackAttempted = true;
        m_cloudPlaybackFallbackAttempted = true;
        commitFastStreamResult(QVariantMap{
            {QStringLiteral("trackId"), trackId},
            {QStringLiteral("urls"), QStringList{
                QStringLiteral("https://music.163.com/song/media/outer/url?id=%1.mp3").arg(sourceId)}},
            {QStringLiteral("providerId"), QStringLiteral("netease")},
            {QStringLiteral("providerName"), QStringLiteral("网易云音乐 · 官方直连")},
            {QStringLiteral("access"), track.value(QStringLiteral("access"), QStringLiteral("unknown")).toString()},
            {QStringLiteral("origin"), QStringLiteral("outer")},
            {QStringLiteral("web"), false}
        });
        return;
    }

    m_fastStreamPreferenceTimer.stop();
    m_fastStreamTrackId.clear();

    if (m_roamActive && m_player.queue().size() > 1) {
        emit toastRequested(QStringLiteral("这首暂时不可播，智能漫游已自动跳到下一首。"));
        QTimer::singleShot(180, this, [this] {
            if (m_roamActive)
                m_player.next();
        });
        return;
    }

    const QString reason = m_fastStreamErrors.isEmpty()
        ? QStringLiteral("所有并发音源都没有返回可播放地址。")
        : m_fastStreamErrors.join(QStringLiteral("；"));
    setError(sourceId.isEmpty()
        ? QStringLiteral("播放失败：") + reason
        : QStringLiteral("播放失败 [网易ID %1]：%2").arg(sourceId, reason));
    emit toastRequested(m_errorMessage);
}

QString AppController::coverCandidateUrl(const QString &rawUrl,
                                         int pixelSize,
                                         int attempt) const
{
    if (rawUrl.trimmed().isEmpty() || attempt < 0)
        return {};

    QUrl direct(coverDirectUrl(rawUrl, pixelSize));
    if (!direct.isValid())
        return attempt == 0 ? rawUrl : QString();

    const QString host = direct.host().toLower();
    const bool neteaseArtwork =
        host == QStringLiteral("music.126.net")
        || host.endsWith(QStringLiteral(".music.126.net"))
        || host == QStringLiteral("126.net")
        || host.endsWith(QStringLiteral(".126.net"))
        || host == QStringLiteral("163.com")
        || host.endsWith(QStringLiteral(".163.com"));

    if (!neteaseArtwork)
        return attempt == 0 ? direct.toString(QUrl::FullyEncoded) : QString();

    QStringList candidates;
    auto appendUnique = [&candidates](const QUrl &url) {
        const QString value = url.toString(QUrl::FullyEncoded);
        if (!value.isEmpty() && !candidates.contains(value))
            candidates << value;
    };

    // Direct CDN first: for users in mainland China this usually avoids an
    // unnecessary Cloudflare round trip entirely.
    appendUnique(direct);

    // If the first CDN edge is dead, jump to the Worker immediately. The
    // Worker races all NetEase artwork mirrors in parallel, so putting it ahead
    // of serial client-side mirrors avoids several timeout rounds.
    if (m_cloudMusicMode && !m_cloudMusic.baseUrl().isEmpty()) {
        QUrl proxy(m_cloudMusic.baseUrl() + QStringLiteral("/v1/artwork"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("url"), direct.toString());
        query.addQueryItem(QStringLiteral("size"), QString::number(qBound(48, pixelSize, 1200)));
        proxy.setQuery(query);
        appendUnique(proxy);
    }

    const QRegularExpression mirrorRx(QStringLiteral(R"(^p\d+\.music\.126\.net$)"),
                                      QRegularExpression::CaseInsensitiveOption);
    if (host == QStringLiteral("music.126.net")
        || mirrorRx.match(host).hasMatch()) {
        for (int i = 1; i <= 4; ++i) {
            QUrl mirror(direct);
            mirror.setHost(QStringLiteral("p%1.music.126.net").arg(i));
            appendUnique(mirror);
        }
    }

    // Retry the original object without resize parameters. A few older album
    // objects fail only when the CDN's ?param= resize transform is requested.
    if (host.endsWith(QStringLiteral("126.net"))) {
        QUrl original(direct);
        QUrlQuery query(original);
        query.removeAllQueryItems(QStringLiteral("param"));
        original.setQuery(query);
        appendUnique(original);
    }

    return attempt < candidates.size() ? candidates.at(attempt) : QString();
}

void AppController::setLoading(bool value)
{
    if (m_loading == value) return;
    m_loading = value;
    emit loadingChanged();
}

void AppController::setError(const QString &message)
{
    if (m_errorMessage == message) return;
    m_errorMessage = message;
    emit errorMessageChanged();
}

QStringList AppController::configuredCloudApiUrls() const
{
    if (qEnvironmentVariableIntValue("EVOLVE_MUSIC_CLOUD_DISABLE") == 1)
        return {};

    auto repairEndpoint = [](QString value) {
        value = value.trimmed();
        // Migrate beta builds that accidentally persisted the old typo or a
        // malformed Punycode spelling.  Keep arbitrary user-supplied endpoints
        // untouched; only known EvolveMusic relay variants are repaired.
        value.replace(QStringLiteral("iyxowo.top"), QStringLiteral("tyxowo.top"), Qt::CaseInsensitive);
        const QString lower = value.toLower();
        if (lower.contains(QStringLiteral("tyxowo.top"))
            && (lower.contains(QStringLiteral("fiqq40n"))
                || lower.contains(QStringLiteral("flqq40n")))) {
            value = QStringLiteral("https://xn--fiqq40n.tyxowo.top");
        }
        while (value.endsWith(QLatin1Char('/'))) value.chop(1);
        return value;
    };

    QString primary = repairEndpoint(qEnvironmentVariable("EVOLVE_MUSIC_CLOUD_API"));
    QString backup = repairEndpoint(qEnvironmentVariable("EVOLVE_MUSIC_CLOUD_API_BACKUP"));

    if (primary.isEmpty())
        primary = repairEndpoint(m_settings.value(QStringLiteral("cloud/apiPrimaryUrl"),
            QStringLiteral("https://evolvemusic-cloud.18048369193.workers.dev")).toString());
    if (backup.isEmpty())
        backup = repairEndpoint(m_settings.value(QStringLiteral("cloud/apiBackupUrl"),
            QStringLiteral("https://xn--fiqq40n.tyxowo.top")).toString());

    QStringList urls;
    for (QString url : QStringList{primary, backup}) {
        if (!url.isEmpty() && !urls.contains(url)) urls << url;
    }
    return urls;
}

QStringList AppController::cloudApiUrls() const
{
    const QStringList configured = configuredCloudApiUrls();
    if (configured.isEmpty())
        return {};
    if (m_cloudRouteMode == QStringLiteral("primary"))
        return {configured.value(0)};
    if (m_cloudRouteMode == QStringLiteral("relay"))
        return {configured.size() > 1 ? configured.value(1) : configured.value(0)};

    QStringList ordered = configured;
    if (configured.size() > 1) {
        const bool relayWins = m_cloudRelayLatency >= 0
            && (m_cloudPrimaryLatency < 0 || m_cloudRelayLatency < m_cloudPrimaryLatency);
        if (relayWins)
            ordered.move(1, 0);
    }
    return ordered;
}

void AppController::applyCloudRouteSelection()
{
    const QStringList endpoints = cloudApiUrls();
    if (endpoints.isEmpty())
        return;
    const bool changed = m_cloudMusic.baseUrls() != endpoints;
    if (changed) {
        m_cloudMusic.setBaseUrls(endpoints);
        if (!m_profiles.currentProfileDataRoot().isEmpty())
            m_cloudMusic.setProfileDataRoot(m_profiles.currentProfileDataRoot());
        m_cloudMusic.initialize();
    }
    refreshCloudPolicy();
    emit cloudStatusChanged();
}

QString AppController::cloudApiBaseUrl() const
{
    return cloudApiUrls().value(0);
}

int AppController::customPlaylistIndex(const QString &playlistId) const
{
    for (int i = 0; i < m_customPlaylists.size(); ++i) {
        if (m_customPlaylists.at(i).toMap().value(QStringLiteral("id")).toString() == playlistId)
            return i;
    }
    return -1;
}

void AppController::saveCustomPlaylistAt(int index)
{
    if (index < 0 || index >= m_customPlaylists.size() || !m_cloudMusic.ready())
        return;
    const QVariantMap playlist = m_customPlaylists.at(index).toMap();
    m_cloudMusic.updateCustomPlaylist(
        playlist.value(QStringLiteral("id")).toString(),
        playlist.value(QStringLiteral("name")).toString(),
        playlist.value(QStringLiteral("tracks")).toList(),
        playlist.value(QStringLiteral("description")).toString());
}

void AppController::refreshCloudPolicy()
{
    const QString baseUrl =
        !m_cloudMusic.baseUrl().isEmpty() ? m_cloudMusic.baseUrl() : cloudApiBaseUrl();

    if (baseUrl.isEmpty())
        return;

    m_cloudPolicy.setApiBaseUrl(baseUrl);
    m_cloudPolicy.refresh();
}

void AppController::applyCloudPolicy(
    const QVariantMap &policy)
{
    const QVariantMap sourcePolicy =
        policy
            .value(
                QStringLiteral("sourcePolicy"))
            .toMap();

    if (sourcePolicy
            .value(
                QStringLiteral("mode"))
            .toString()
        != QStringLiteral("netease-first")) {
        return;
    }

    const QVariantMap priorities =
        sourcePolicy
            .value(
                QStringLiteral("priorities"))
            .toMap();

    // Remote config may tune fallback order, but it cannot unlock a global
    // account or override the local-profile credential boundary.
    m_sources.setProviderPriority(
        QStringLiteral("netease"),
        0);

    const QStringList fallbackIds{
        QStringLiteral("qq"),
        QStringLiteral("kugou"),
        QStringLiteral("yueting"),
        QStringLiteral("gequhai")
    };

    for (const QString &id : fallbackIds) {
        if (!priorities.contains(id))
            continue;

        const int priority =
            qBound(
                10,
                priorities
                    .value(id)
                    .toInt(),
                999);

        m_sources.setProviderPriority(
            id,
            priority);
    }

    emit providersChanged();
}

QVariantMap AppController::cloudSettingsSnapshot() const
{
    return QVariantMap{
        {QStringLiteral("qualityLevel"),
         m_qualityLevel},
        {QStringLiteral("darkMode"),
         m_darkMode},
        {QStringLiteral("accentColor"),
         m_accentColor},
        {QStringLiteral("animationsEnabled"),
         m_animationsEnabled},
        {QStringLiteral("compactTrackRows"),
         m_compactTrackRows},
        {QStringLiteral("showSourceBadges"),
         m_showSourceBadges},
        {QStringLiteral("language"), m_language},
        {QStringLiteral("themePreset"), m_themePreset},
        {QStringLiteral("animationLevel"), m_animationLevel},
        {QStringLiteral("recommendationDiversity"), m_recommendationDiversity}
    };
}

void AppController::applyCloudState(
    const QVariantList &favorites,
    const QVariantList &history,
    const QVariantMap &settings)
{
    if (!m_cloudMusicMode)
        return;

    qInfo() << "Applying cloud state; settings keys:" << settings.keys();

    const QVariantList oldFavorites = m_favorites;
    const QVariantList oldHistory = m_history;
    const QString oldQuality = m_qualityLevel;
    const bool oldDarkMode = m_darkMode;
    const QString oldAccent = m_accentColor;
    const bool oldAnimations = m_animationsEnabled;
    const bool oldCompactRows = m_compactTrackRows;
    const bool oldSourceBadges = m_showSourceBadges;
    const QString oldLanguage = m_language;
    const QString oldThemePreset = m_themePreset;
    const QString oldAnimationLevel = m_animationLevel;
    const int oldDiversity = m_recommendationDiversity;

    m_favorites = favorites;
    m_history = history;

    const QString quality = settings.value(QStringLiteral("qualityLevel")).toString();
    if (QStringList{
            QStringLiteral("standard"),
            QStringLiteral("higher"),
            QStringLiteral("exhigh"),
            QStringLiteral("lossless")
        }.contains(quality)) {
        m_qualityLevel = quality;
    }

    if (settings.contains(QStringLiteral("darkMode")))
        m_darkMode = settings.value(QStringLiteral("darkMode")).toBool();

    const QString accent = settings.value(QStringLiteral("accentColor")).toString();
    if (!accent.isEmpty())
        m_accentColor = accent;

    if (settings.contains(QStringLiteral("animationsEnabled")))
        m_animationsEnabled = settings.value(QStringLiteral("animationsEnabled")).toBool();

    if (settings.contains(QStringLiteral("compactTrackRows")))
        m_compactTrackRows = settings.value(QStringLiteral("compactTrackRows")).toBool();

    if (settings.contains(QStringLiteral("showSourceBadges")))
        m_showSourceBadges = settings.value(QStringLiteral("showSourceBadges")).toBool();

    const QString language = settings.value(QStringLiteral("language")).toString();
    if (QStringList{QStringLiteral("zh-CN"), QStringLiteral("en-US")}.contains(language))
        m_language = language;

    const QString preset = settings.value(QStringLiteral("themePreset")).toString();
    if (!preset.isEmpty()) {
        m_themePreset = preset;
        applyThemePresetValues(preset);
    }

    const QString anim = settings.value(QStringLiteral("animationLevel")).toString();
    if (QStringList{QStringLiteral("off"), QStringLiteral("balanced"), QStringLiteral("rich")}.contains(anim)) {
        m_animationLevel = anim;
        m_animationsEnabled = anim != QStringLiteral("off");
    }

    if (settings.contains(QStringLiteral("recommendationDiversity")))
        m_recommendationDiversity = qBound(0, settings.value(QStringLiteral("recommendationDiversity")).toInt(), 100);

    // UI font is intentionally device-local and restart-only. Do not apply or
    // synchronize it through live cloud state; this keeps cloud hydration from
    // invalidating any QML font/model bindings on Qt 6.10/Windows.

    QStringList changedSignals;
    if (oldFavorites != m_favorites) { emit favoritesChanged(); changedSignals << QStringLiteral("favorites"); }
    if (oldHistory != m_history) { emit historyChanged(); changedSignals << QStringLiteral("history"); }
    if (oldQuality != m_qualityLevel) { emit qualityLevelChanged(); changedSignals << QStringLiteral("quality"); }
    if (oldDarkMode != m_darkMode) { emit darkModeChanged(); changedSignals << QStringLiteral("darkMode"); }
    if (oldAccent != m_accentColor) { emit accentColorChanged(); changedSignals << QStringLiteral("accentColor"); }
    if (oldAnimations != m_animationsEnabled) { emit animationsEnabledChanged(); changedSignals << QStringLiteral("animations"); }
    if (oldCompactRows != m_compactTrackRows) { emit compactTrackRowsChanged(); changedSignals << QStringLiteral("compactRows"); }
    if (oldSourceBadges != m_showSourceBadges) { emit showSourceBadgesChanged(); changedSignals << QStringLiteral("sourceBadges"); }
    if (oldLanguage != m_language) { emit languageChanged(); changedSignals << QStringLiteral("language"); }
    if (oldThemePreset != m_themePreset) { emit themePresetChanged(); changedSignals << QStringLiteral("themePreset"); }
    if (oldAnimationLevel != m_animationLevel) { emit animationLevelChanged(); changedSignals << QStringLiteral("animationLevel"); }
    if (oldDiversity != m_recommendationDiversity) { emit recommendationDiversityChanged(); changedSignals << QStringLiteral("diversity"); }

    qInfo() << "Cloud state applied; changed bindings:" << changedSignals;
}

void AppController::scheduleCloudStateSave()
{
    if (!m_cloudMusicMode)
        return;

    m_cloudStateSaveTimer.start();
}

void AppController::openProfileSettings()
{
    QDir().mkpath(
        m_profiles.currentProfileDataRoot());

    const QString filePath =
        QDir(m_profiles.currentProfileDataRoot())
            .filePath(QStringLiteral("settings.ini"));

    m_profileSettings =
        std::make_unique<QSettings>(
            filePath,
            QSettings::IniFormat);
}

void AppController::migrateLegacySettingsIfNeeded()
{
    if (!m_profileSettings
        || m_profiles.currentProfileId()
               != QStringLiteral("default")
        || m_profileSettings
               ->value(
                   QStringLiteral("meta/legacyImported"),
                   false)
               .toBool()) {
        return;
    }

    const QStringList keys{
        QStringLiteral("playback/qualityLevel"),
        QStringLiteral("playback/playerStyle"),
        QStringLiteral("appearance/darkMode"),
        QStringLiteral("appearance/accentColor"),
        QStringLiteral("appearance/animationsEnabled"),
        QStringLiteral("appearance/compactTrackRows"),
        QStringLiteral("appearance/showSourceBadges"),
        QStringLiteral("appearance/language"),
        QStringLiteral("appearance/themePreset"),
        QStringLiteral("appearance/animationLevel"),
        QStringLiteral("playback/recommendationDiversity"),
        QStringLiteral("cache/directory"),
        QStringLiteral("library/favorites"),
        QStringLiteral("library/history")
    };

    for (const QString &key : keys) {
        if (m_settings.contains(key)
            && !m_profileSettings->contains(key)) {
            m_profileSettings->setValue(
                key,
                m_settings.value(key));
        }
    }

    m_profileSettings->setValue(
        QStringLiteral("meta/legacyImported"),
        true);
    m_profileSettings->sync();
}

void AppController::loadProfilePreferences()
{
    if (!m_profileSettings)
        return;

    m_qualityLevel =
        m_profileSettings
            ->value(
                QStringLiteral("playback/qualityLevel"),
                QStringLiteral("exhigh"))
            .toString();

    if (!QStringList{
            QStringLiteral("standard"),
            QStringLiteral("higher"),
            QStringLiteral("exhigh"),
            QStringLiteral("lossless")
        }.contains(m_qualityLevel)) {
        m_qualityLevel =
            QStringLiteral("exhigh");
    }

    m_playerStyle = m_profileSettings->value(
        QStringLiteral("playback/playerStyle"), QStringLiteral("balanced")).toString();
    if (!QStringList{QStringLiteral("balanced"), QStringLiteral("centered"),
                     QStringLiteral("cover")}.contains(m_playerStyle))
        m_playerStyle = QStringLiteral("balanced");

    m_darkMode =
        m_profileSettings
            ->value(
                QStringLiteral("appearance/darkMode"),
                false)
            .toBool();

    m_accentColor =
        m_profileSettings
            ->value(
                QStringLiteral("appearance/accentColor"),
                QStringLiteral("#13D9B0"))
            .toString();

    m_animationsEnabled =
        m_profileSettings
            ->value(
                QStringLiteral("appearance/animationsEnabled"),
                true)
            .toBool();

    m_compactTrackRows =
        m_profileSettings
            ->value(
                QStringLiteral("appearance/compactTrackRows"),
                true)
            .toBool();

    m_showSourceBadges =
        m_profileSettings
            ->value(
                QStringLiteral("appearance/showSourceBadges"),
                true)
            .toBool();

    m_language = m_profileSettings->value(
        QStringLiteral("appearance/language"), QStringLiteral("zh-CN")).toString();
    if (!QStringList{QStringLiteral("zh-CN"), QStringLiteral("en-US")}.contains(m_language))
        m_language = QStringLiteral("zh-CN");

    m_themePreset = m_profileSettings->value(
        QStringLiteral("appearance/themePreset"), QStringLiteral("evolve")).toString();
    if (!QStringList{QStringLiteral("evolve"), QStringLiteral("midnight"),
                     QStringLiteral("forest"), QStringLiteral("violet"),
                     QStringLiteral("paper"), QStringLiteral("oled")}.contains(m_themePreset))
        m_themePreset = QStringLiteral("evolve");
    applyThemePresetValues(m_themePreset);

    m_animationLevel = m_profileSettings->value(
        QStringLiteral("appearance/animationLevel"), QStringLiteral("rich")).toString();
    if (!QStringList{QStringLiteral("off"), QStringLiteral("balanced"), QStringLiteral("rich")}.contains(m_animationLevel))
        m_animationLevel = QStringLiteral("rich");
    m_animationsEnabled = m_animationLevel != QStringLiteral("off");

    m_recommendationDiversity = qBound(
        0,
        m_profileSettings->value(QStringLiteral("playback/recommendationDiversity"), 62).toInt(),
        100);

    const QString defaultUiFont = m_availableFonts.contains(QStringLiteral("Microsoft YaHei UI"), Qt::CaseInsensitive)
        ? QStringLiteral("Microsoft YaHei UI") : QStringLiteral("系统默认");
    m_fontFamily = m_profileSettings->value(QStringLiteral("appearance/fontFamily"), defaultUiFont).toString();
    if (!m_availableFonts.contains(m_fontFamily, Qt::CaseInsensitive))
        m_fontFamily = QStringLiteral("系统默认");
    // Do not call QGuiApplication::setFont() here. This function is also used
    // during runtime profile/session changes, after QML is alive. The startup
    // constructor applies the full application font once before QML loads.

    m_cacheDirectory = m_profileSettings->value(QStringLiteral("cache/directory")).toString().trimmed();
}

void AppController::saveCurrentProfileState()
{
    m_historySaveTimer.stop();

    saveList(
        QStringLiteral("library/favorites"),
        m_favorites);

    saveList(
        QStringLiteral("library/history"),
        m_history);

    if (m_profileSettings)
        m_profileSettings->sync();
}

void AppController::loadPersistentLists()
{
    auto load = [this](const QString &key) {
        if (!m_profileSettings)
            return QVariantList{};

        const QByteArray data =
            m_profileSettings
                ->value(key)
                .toByteArray();

        if (data.isEmpty())
            return QVariantList{};

        const QJsonDocument document =
            QJsonDocument::fromJson(data);

        return document.isArray()
            ? document.array().toVariantList()
            : QVariantList{};
    };

    m_favorites =
        load(QStringLiteral("library/favorites"));

    m_history =
        load(QStringLiteral("library/history"));
}

void AppController::saveList(const QString &key,
                             const QVariantList &list)
{
    if (!m_profileSettings)
        return;

    m_profileSettings->setValue(
        key,
        QJsonDocument(
            QJsonArray::fromVariantList(list))
            .toJson(QJsonDocument::Compact));
}

QVariantList AppController::parseLrc(const QString &raw, const QString &translated) const
{
    QMap<qint64, QString> translations;
    const QRegularExpression re(R"(\[(\d{1,2}):(\d{1,2})(?:\.(\d{1,3}))?\](.*))");
    auto parseInto = [&](const QString &text, bool translation, QVariantList *out) {
        const QStringList lines = text.split('\n');
        for (const QString &line : lines) {
            auto match = re.match(line.trimmed());
            if (!match.hasMatch()) continue;
            const qint64 min = match.captured(1).toLongLong();
            const qint64 sec = match.captured(2).toLongLong();
            QString frac = match.captured(3);
            while (frac.size() < 3) frac += '0';
            if (frac.size() > 3) frac = frac.left(3);
            const qint64 time = (min * 60 + sec) * 1000 + frac.toLongLong();
            const QString content = match.captured(4).trimmed();
            if (content.isEmpty()) continue;
            if (translation) translations[time] = content;
            else out->append(QVariantMap{{"time", time}, {"text", content}});
        }
    };
    QVariantList result;
    parseInto(translated, true, &result);
    parseInto(raw, false, &result);
    for (QVariant &v : result) {
        QVariantMap m = v.toMap();
        const qint64 t = m.value("time").toLongLong();
        if (translations.contains(t) && translations.value(t) != m.value("text").toString())
            m["translation"] = translations.value(t);
        v = m;
    }
    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("time").toLongLong() < b.toMap().value("time").toLongLong();
    });
    return result;
}
