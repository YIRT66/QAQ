#pragma once

#include <QObject>
#include <QByteArray>
#include <QSettings>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QNetworkAccessManager>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <memory>

class QAudioSource;
class QAudioSink;
class QIODevice;

#include "PlayerController.h"
#include "MultiSourceManager.h"
#include "AccountManager.h"
#include "LocalProfileManager.h"
#include "CloudPolicyClient.h"
#include "CloudMusicClient.h"
#include "../providers/NeteaseProvider.h"
#include "../providers/JsonGatewayProvider.h"
#include "../providers/OurcraftProvider.h"
#include "../providers/YuetingProvider.h"
#include "../providers/GequhaiProvider.h"

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList homePlaylists READ homePlaylists NOTIFY homePlaylistsChanged)
    Q_PROPERTY(QVariantList homeTracks READ homeTracks NOTIFY homeTracksChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)
    Q_PROPERTY(QVariantList searchPlaylists READ searchPlaylists NOTIFY searchCatalogChanged)
    Q_PROPERTY(QVariantList searchArtists READ searchArtists NOTIFY searchCatalogChanged)
    Q_PROPERTY(QVariantMap currentPlaylist READ currentPlaylist NOTIFY currentPlaylistChanged)
    Q_PROPERTY(QVariantList playlistTracks READ playlistTracks NOTIFY playlistTracksChanged)
    Q_PROPERTY(QVariantList favorites READ favorites NOTIFY favoritesChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QVariantList lyrics READ lyrics NOTIFY lyricsChanged)
    Q_PROPERTY(QVariantList providers READ providers NOTIFY providersChanged)
    Q_PROPERTY(QString ourcraftApiUrl READ ourcraftApiUrl WRITE setOurcraftApiUrl NOTIFY providersChanged)
    Q_PROPERTY(QVariantList customPlaylists READ customPlaylists NOTIFY customPlaylistsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY cloudMusicModeChanged)
    Q_PROPERTY(QString activeSourceName READ activeSourceName NOTIFY activeSourceChanged)
    Q_PROPERTY(QString activeSourceAccess READ activeSourceAccess NOTIFY activeSourceChanged)
    Q_PROPERTY(QString activeSourceProviderId READ activeSourceProviderId NOTIFY activeSourceChanged)
    Q_PROPERTY(QString playbackSourcePreference READ playbackSourcePreference NOTIFY playbackSourcePreferenceChanged)
    Q_PROPERTY(QVariantList playbackSourceChoices READ playbackSourceChoices NOTIFY playbackSourcePreferenceChanged)
    Q_PROPERTY(QString qualityLevel READ qualityLevel WRITE setQualityLevel NOTIFY qualityLevelChanged)
    Q_PROPERTY(QString playerStyle READ playerStyle WRITE setPlayerStyle NOTIFY playerStyleChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(QString accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled WRITE setAnimationsEnabled NOTIFY animationsEnabledChanged)
    Q_PROPERTY(bool compactTrackRows READ compactTrackRows WRITE setCompactTrackRows NOTIFY compactTrackRowsChanged)
    Q_PROPERTY(bool showSourceBadges READ showSourceBadges WRITE setShowSourceBadges NOTIFY showSourceBadgesChanged)
    Q_PROPERTY(bool preferFreeSources READ preferFreeSources CONSTANT)
    Q_PROPERTY(bool cloudMusicMode READ cloudMusicMode NOTIFY cloudMusicModeChanged)
    Q_PROPERTY(bool cloudReady READ cloudReady NOTIFY cloudStatusChanged)
    Q_PROPERTY(QString cloudUserId READ cloudUserId NOTIFY cloudStatusChanged)
    Q_PROPERTY(QString cloudApiUrl READ cloudApiUrl NOTIFY cloudStatusChanged)
    Q_PROPERTY(QStringList cloudApiUrls READ cloudApiUrls NOTIFY cloudStatusChanged)
    Q_PROPERTY(QString cloudRouteMode READ cloudRouteMode WRITE setCloudRouteMode NOTIFY cloudRouteChanged)
    Q_PROPERTY(bool cloudRouteTesting READ cloudRouteTesting NOTIFY cloudRouteChanged)
    Q_PROPERTY(int cloudPrimaryLatency READ cloudPrimaryLatency NOTIFY cloudRouteChanged)
    Q_PROPERTY(int cloudRelayLatency READ cloudRelayLatency NOTIFY cloudRouteChanged)
    Q_PROPERTY(bool accountLoggedIn READ accountLoggedIn NOTIFY accountChanged)
    Q_PROPERTY(QString accountUsername READ accountUsername NOTIFY accountChanged)
    Q_PROPERTY(QString accountEmail READ accountEmail NOTIFY accountChanged)
    Q_PROPERTY(bool accountBusy READ accountBusy NOTIFY accountChanged)
    Q_PROPERTY(QString accountError READ accountError NOTIFY accountChanged)
    Q_PROPERTY(bool roamActive READ roamActive NOTIFY roamActiveChanged)
    Q_PROPERTY(bool roamLoading READ roamLoading NOTIFY roamLoadingChanged)
    Q_PROPERTY(QString roamReason READ roamReason NOTIFY roamReasonChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString themePreset READ themePreset WRITE setThemePreset NOTIFY themePresetChanged)
    Q_PROPERTY(QString animationLevel READ animationLevel WRITE setAnimationLevel NOTIFY animationLevelChanged)
    Q_PROPERTY(QString cacheDirectory READ cacheDirectory NOTIFY cacheDirectoryChanged)
    Q_PROPERTY(QString cacheSizeText READ cacheSizeText NOTIFY cacheDirectoryChanged)
    Q_PROPERTY(int recommendationDiversity READ recommendationDiversity WRITE setRecommendationDiversity NOTIFY recommendationDiversityChanged)
    Q_PROPERTY(QVariantMap userProfile READ userProfile NOTIFY userProfileChanged)
    Q_PROPERTY(QVariantList followingUsers READ followingUsers NOTIFY followingUsersChanged)
    Q_PROPERTY(QVariantList userSearchResults READ userSearchResults NOTIFY userSearchResultsChanged)
    Q_PROPERTY(QVariantList communityPlaylists READ communityPlaylists NOTIFY communityPlaylistsChanged)
    Q_PROPERTY(QVariantMap togetherRoom READ togetherRoom NOTIFY togetherRoomChanged)
    Q_PROPERTY(QVariantList togetherInvites READ togetherInvites NOTIFY togetherInvitesChanged)
    Q_PROPERTY(QVariantList togetherMessages READ togetherMessages NOTIFY togetherMessagesChanged)
    Q_PROPERTY(QVariantMap togetherAttachmentUrls READ togetherAttachmentUrls NOTIFY togetherAttachmentUrlsChanged)
    Q_PROPERTY(bool togetherMicActive READ togetherMicActive NOTIFY togetherVoiceChanged)
    Q_PROPERTY(double togetherMicLevel READ togetherMicLevel NOTIFY togetherVoiceChanged)
    Q_PROPERTY(QString togetherVoiceError READ togetherVoiceError NOTIFY togetherVoiceChanged)
    // Runtime-immutable QML view of the selected UI font. Font changes are
    // persisted and become effective on the next process start; intentionally no
    // NOTIFY signal is exposed because reevaluating the large QStringList-backed
    // ComboBox binding on Qt 6.10/Windows caused a QML JS stack overflow.
    Q_PROPERTY(QString fontFamily READ fontFamily)
    Q_PROPERTY(QString resolvedFontFamily READ resolvedFontFamily)
    Q_PROPERTY(QStringList availableFonts READ availableFonts CONSTANT)
    Q_PROPERTY(int onlineUsers READ onlineUsers NOTIFY onlineUsersChanged)
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(QString pluginDirectory READ pluginDirectory CONSTANT)
public:
    explicit AppController(QObject *parent = nullptr);

    PlayerController *player() { return &m_player; }
    AccountManager *accounts() { return &m_accounts; }
    LocalProfileManager *profiles() { return &m_profiles; }
    QVariantList homePlaylists() const { return m_homePlaylists; }
    QVariantList homeTracks() const { return m_homeTracks; }
    QVariantList searchResults() const { return m_searchResults; }
    QVariantList searchPlaylists() const { return m_searchPlaylists; }
    QVariantList searchArtists() const { return m_searchArtists; }
    QVariantMap currentPlaylist() const { return m_currentPlaylist; }
    QVariantList playlistTracks() const { return m_playlistTracks; }
    QVariantList favorites() const { return m_favorites; }
    QVariantList history() const { return m_history; }
    QVariantList lyrics() const { return m_lyrics; }
    QVariantList providers() const { return m_sources.providerStates(); }
    QString ourcraftApiUrl() const { return m_ourcraftNetease.baseUrl(); }
    QVariantList customPlaylists() const { return m_customPlaylists; }
    bool loading() const { return m_loading; }
    QString errorMessage() const { return m_errorMessage; }
    QString sourceName() const { return QStringLiteral("Ourcraft Music API · 网易/酷狗/酷我"); }
    QString activeSourceName() const { return m_activeSourceName; }
    QString activeSourceAccess() const { return m_activeSourceAccess; }
    QString activeSourceProviderId() const { return m_activeSourceProviderId; }
    QString playbackSourcePreference() const { return m_manualPlaybackProviderId; }
    QVariantList playbackSourceChoices() const;
    QString qualityLevel() const { return m_qualityLevel; }
    QString playerStyle() const { return m_playerStyle; }
    bool darkMode() const { return m_darkMode; }
    QString accentColor() const { return m_accentColor; }
    bool animationsEnabled() const { return m_animationsEnabled; }
    bool compactTrackRows() const { return m_compactTrackRows; }
    bool showSourceBadges() const { return m_showSourceBadges; }
    bool preferFreeSources() const { return false; }
    bool cloudMusicMode() const { return m_cloudMusicMode; }
    bool cloudReady() const { return m_cloudMusic.ready(); }
    QString cloudUserId() const { return m_cloudMusic.userId(); }
    QString cloudApiUrl() const { return m_cloudMusic.baseUrl(); }
    QStringList cloudApiUrls() const;
    QString cloudRouteMode() const { return m_cloudRouteMode; }
    bool cloudRouteTesting() const { return m_cloudRouteTesting; }
    int cloudPrimaryLatency() const { return m_cloudPrimaryLatency; }
    int cloudRelayLatency() const { return m_cloudRelayLatency; }
    bool accountLoggedIn() const { return m_cloudMusic.loggedIn(); }
    QString accountUsername() const { return m_cloudMusic.username(); }
    QString accountEmail() const { return m_cloudMusic.email(); }
    bool accountBusy() const { return m_cloudMusic.authBusy(); }
    QString accountError() const { return m_cloudMusic.authError(); }
    bool roamActive() const { return m_roamActive; }
    bool roamLoading() const { return m_roamLoading; }
    QString roamReason() const { return m_roamReason; }
    QString language() const { return m_language; }
    QString themePreset() const { return m_themePreset; }
    QString animationLevel() const { return m_animationLevel; }
    QString cacheDirectory() const { return m_cacheDirectory; }
    QString cacheSizeText() const;
    int recommendationDiversity() const { return m_recommendationDiversity; }
    QVariantMap userProfile() const { return m_userProfile; }
    QVariantList followingUsers() const { return m_followingUsers; }
    QVariantList userSearchResults() const { return m_userSearchResults; }
    QVariantList communityPlaylists() const { return m_communityPlaylists; }
    QVariantMap togetherRoom() const { return m_togetherRoom; }
    QVariantList togetherInvites() const { return m_togetherInvites; }
    QVariantList togetherMessages() const { return m_togetherMessages; }
    QVariantMap togetherAttachmentUrls() const { return m_togetherAttachmentUrls; }
    bool togetherMicActive() const { return m_togetherMicActive; }
    double togetherMicLevel() const { return m_togetherMicLevel; }
    QString togetherVoiceError() const { return m_togetherVoiceError; }
    QString fontFamily() const { return m_fontFamily; }
    QString resolvedFontFamily() const {
        return m_fontFamily == QStringLiteral("系统默认") ? m_systemFontFamily : m_fontFamily;
    }
    QStringList availableFonts() const { return m_availableFonts; }
    int onlineUsers() const { return m_onlineUsers; }
    QVariantList plugins() const { return m_plugins; }
    QString pluginDirectory() const { return m_pluginDirectory; }

    Q_INVOKABLE void initialize();
    Q_INVOKABLE void selectPlaybackSource(const QString &providerId);
    Q_INVOKABLE void setCloudRouteMode(const QString &mode);
    Q_INVOKABLE void testCloudRoutes();
    Q_INVOKABLE void loginAccount(const QString &username, const QString &password);
    Q_INVOKABLE void registerAccount(const QString &username,
                                     const QString &password,
                                     const QString &email,
                                     const QString &emailCode,
                                     bool acceptTerms);
    Q_INVOKABLE void sendRegistrationEmailCode(const QString &email);
    Q_INVOKABLE void sendPasswordResetCode(const QString &email);
    Q_INVOKABLE void resetPassword(const QString &email,
                                   const QString &code,
                                   const QString &newPassword);
    Q_INVOKABLE void logoutAccount();
    Q_INVOKABLE void createCustomPlaylist(const QString &name);
    Q_INVOKABLE void deleteCustomPlaylist(const QString &playlistId);
    Q_INVOKABLE void renameCustomPlaylist(const QString &playlistId, const QString &name);
    Q_INVOKABLE void addTrackToCustomPlaylist(const QString &playlistId, const QVariantMap &track);
    Q_INVOKABLE void removeTrackFromCustomPlaylist(const QString &playlistId, const QString &trackId);
    Q_INVOKABLE void openCustomPlaylist(const QString &playlistId);
    Q_INVOKABLE void updateCustomPlaylistMetadata(const QString &playlistId,
                                                   const QString &name,
                                                   const QString &description,
                                                   const QString &coverFile,
                                                   bool isPublic);
    Q_INVOKABLE void loadCommunityPlaylists();
    Q_INVOKABLE void openCommunityPlaylist(const QString &playlistId);
    Q_INVOKABLE void loadUserProfile();
    Q_INVOKABLE void updateUserProfile(const QString &displayName,
                                       const QString &bio,
                                       const QString &avatarFile);
    Q_INVOKABLE void searchUsers(const QString &query);
    Q_INVOKABLE void setFollowing(const QString &userId, bool follow);
    Q_INVOKABLE void createTogetherRoom();
    Q_INVOKABLE void joinTogetherRoom(const QString &code);
    Q_INVOKABLE void leaveTogetherRoom();
    Q_INVOKABLE void inviteTogetherUser(const QString &username);
    Q_INVOKABLE void dismissTogetherInvite(const QString &inviteId);
    Q_INVOKABLE void refreshTogetherRoom();
    Q_INVOKABLE void sendTogetherText(const QString &text);
    Q_INVOKABLE void sendTogetherImage();
    Q_INVOKABLE void sendTogetherFile();
    Q_INVOKABLE void openTogetherAttachment(const QString &attachmentId, const QString &fileName);
    Q_INVOKABLE void saveTogetherAttachment(const QString &attachmentId, const QString &fileName);
    Q_INVOKABLE void setTogetherMicrophone(bool enabled);
    Q_INVOKABLE QString formatFileSize(qint64 bytes) const;
    Q_INVOKABLE void search(const QString &keywords);
    Q_INVOKABLE void openPlaylist(const QString &playlistId, const QString &providerId = QString());
    Q_INVOKABLE void playTrack(const QVariantMap &track, const QString &context = "auto");
    Q_INVOKABLE void toggleFavorite(const QVariantMap &track);
    Q_INVOKABLE void downloadTrack(const QVariantMap &track);
    Q_INVOKABLE bool isFavorite(const QString &songId) const;
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void refreshHome();
    Q_INVOKABLE void startRoam(bool fresh = false);
    Q_INVOKABLE void stopRoam();
    Q_INVOKABLE void testProvider(const QString &providerId);
    Q_INVOKABLE void setProviderEnabled(const QString &providerId, bool enabled);
    Q_INVOKABLE void setProviderBaseUrl(const QString &providerId, const QString &url);
    Q_INVOKABLE void setProviderPriority(const QString &providerId, int priority);
    Q_INVOKABLE void setOurcraftApiUrl(const QString &url);
    Q_INVOKABLE void resetOurcraftApiUrl();
    Q_INVOKABLE void openProviderWebsite(const QString &providerId);
    Q_INVOKABLE void setQualityLevel(const QString &level);
    Q_INVOKABLE void setPlayerStyle(const QString &style);
    Q_INVOKABLE void setDarkMode(bool dark);
    Q_INVOKABLE void setAccentColor(const QString &color);
    Q_INVOKABLE void setAnimationsEnabled(bool enabled);
    Q_INVOKABLE void setCompactTrackRows(bool compact);
    Q_INVOKABLE void setShowSourceBadges(bool show);
    Q_INVOKABLE void setLanguage(const QString &language);
    Q_INVOKABLE void setThemePreset(const QString &preset);
    Q_INVOKABLE void setAnimationLevel(const QString &level);
    Q_INVOKABLE void setRecommendationDiversity(int value);
    Q_INVOKABLE void setFontFamily(const QString &family);
    Q_INVOKABLE void reloadPlugins();
    Q_INVOKABLE void openPluginDirectory();
    Q_INVOKABLE void createPluginTemplate();
    Q_INVOKABLE void setPluginEnabled(const QString &pluginId, bool enabled);
    Q_INVOKABLE void setPluginPermission(const QString &pluginId,
                                         const QString &permission,
                                         bool granted);
    Q_INVOKABLE QString chooseImageFile();
    Q_INVOKABLE void chooseCacheDirectory();
    Q_INVOKABLE void resetCacheDirectory();
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE QString uiText(const QString &key) const;
    Q_INVOKABLE QString formatDuration(qint64 ms) const;
    Q_INVOKABLE QString formatCount(qint64 count) const;
    Q_INVOKABLE QString accessLabel(const QString &access) const;
    Q_INVOKABLE QString coverUrl(const QString &rawUrl, int pixelSize = 256) const;
    Q_INVOKABLE QString coverDirectUrl(const QString &rawUrl, int pixelSize = 256) const;
    Q_INVOKABLE QString coverCandidateUrl(const QString &rawUrl, int pixelSize, int attempt) const;

signals:
    void homePlaylistsChanged();
    void homeTracksChanged();
    void searchResultsChanged();
    void searchCatalogChanged();
    void currentPlaylistChanged();
    void playlistTracksChanged();
    void favoritesChanged();
    void historyChanged();
    void lyricsChanged();
    void providersChanged();
    void customPlaylistsChanged();
    void accountChanged();
    void loadingChanged();
    void errorMessageChanged();
    void activeSourceChanged();
    void playbackSourcePreferenceChanged();
    void qualityLevelChanged();
    void playerStyleChanged();
    void darkModeChanged();
    void accentColorChanged();
    void animationsEnabledChanged();
    void compactTrackRowsChanged();
    void showSourceBadgesChanged();
    void cloudMusicModeChanged();
    void cloudStatusChanged();
    void cloudRouteChanged();
    void roamActiveChanged();
    void roamLoadingChanged();
    void roamReasonChanged();
    void languageChanged();
    void themePresetChanged();
    void animationLevelChanged();
    void cacheDirectoryChanged();
    void recommendationDiversityChanged();
    void userProfileChanged();
    void followingUsersChanged();
    void userSearchResultsChanged();
    void communityPlaylistsChanged();
    void togetherRoomChanged();
    void togetherInvitesChanged();
    void togetherMessagesChanged();
    void togetherAttachmentUrlsChanged();
    void togetherVoiceChanged();
    void onlineUsersChanged();
    void pluginsChanged();
    void togetherInviteArrived(const QVariantMap &invite);
    void togetherRoomClosed(const QString &message);
    void toastRequested(const QString &message);
    void emailCodeSent(const QString &purpose, int cooldownSeconds);
    void passwordResetCompleted();
    void playlistOpened();

private:
    struct CachedSearch {
        qint64 timestampMs = 0;
        QVariantList rows;
    };

    QVariantList parseLrc(const QString &raw, const QString &translated) const;
    void setLoading(bool value);
    void setError(const QString &message);
    void openProfileSettings();
    void migrateLegacySettingsIfNeeded();
    void loadProfilePreferences();
    void saveCurrentProfileState();
    void loadPersistentLists();
    void saveList(const QString &key, const QVariantList &list);
    void appendHistory(const QVariantMap &track);
    QVariantMap persistentTrack(const QVariantMap &track) const;
    QVariantList contextQueue(const QString &context) const;
    QString imageDataUrl(const QString &filePath, int maxBytes = 420 * 1024) const;
    qint64 directorySize(const QString &path) const;
    void applyThemePresetValues(const QString &preset);
    void applyCacheEnvironment();
    void publishTogetherState();
    void refreshSocialState();
    void refreshPresence();
    void refreshTogetherMessages();
    void refreshTogetherVoice();
    void clearTogetherEphemeral();
    void ensureTogetherVoiceOutput();
    QString togetherTempDirectory() const;
    void cacheTogetherAttachment(const QString &attachmentId,
                                 const QByteArray &bytes,
                                 const QString &fileName,
                                 const QString &mimeType);
    void scanPlugins();
    bool ownsPlaylist(const QVariantMap &playlist) const;
    void configureProviders();
    void inferSearchArtistsFromTracks();
    QStringList configuredCloudApiUrls() const;
    QString cloudApiBaseUrl() const;
    void applyCloudRouteSelection();
    int customPlaylistIndex(const QString &playlistId) const;
    void saveCustomPlaylistAt(int index);
    void refreshCloudPolicy();
    void applyCloudPolicy(const QVariantMap &policy);
    QVariantMap cloudSettingsSnapshot() const;
    void applyCloudState(const QVariantList &favorites,
                         const QVariantList &history,
                         const QVariantMap &settings);
    void scheduleCloudStateSave();
    void requestRoamBatch(bool replace);
    void setRoamLoading(bool value);
    void beginFastStreamRace(const QVariantMap &track);
    void considerFastStreamResult(const QVariantMap &result, bool preferred);
    void commitFastStreamResult(const QVariantMap &result);
    void markFastStreamBranchFailed(bool cloudBranch, const QString &message);
    void maybeFinishFastStreamRace();

    // Legacy provider objects are retained only for the optional embedded music-account
    // login UI. Search, playlists, lyrics and playback now use Ourcraft Music API.
    NeteaseProvider m_netease;
    JsonGatewayProvider m_qq;
    JsonGatewayProvider m_kugou;
    OurcraftProvider m_ourcraftNetease;
    OurcraftProvider m_ourcraftKugou;
    OurcraftProvider m_ourcraftKuwo;
    YuetingProvider m_yueting;
    GequhaiProvider m_gequhai;
    MultiSourceManager m_sources;
    PlayerController m_player;
    LocalProfileManager m_profiles;
    AccountManager m_accounts;
    CloudPolicyClient m_cloudPolicy;
    CloudMusicClient m_cloudMusic;
    QSettings m_settings;
    std::unique_ptr<QSettings> m_profileSettings;
    QNetworkAccessManager m_downloadNetwork;
    QString m_cloudRouteMode = QStringLiteral("auto");
    bool m_cloudRouteTesting = false;
    int m_cloudPrimaryLatency = -1;
    int m_cloudRelayLatency = -1;
    int m_cloudRoutePending = 0;
    quint64 m_cloudRouteGeneration = 0;
    QHash<QString, QVariantMap> m_pendingDownloads;
    QHash<QString, CachedSearch> m_searchCache;
    QString m_pendingSearchKey;
    QVariantList m_homePlaylists;
    QVariantList m_homeTracks;
    QVariantList m_searchResults;
    QVariantList m_searchPlaylists;
    QVariantList m_searchArtists;
    QVariantList m_cloudSearchResults;
    QVariantList m_webSearchResults;
    QVariantMap m_currentPlaylist;
    QVariantList m_playlistTracks;
    QVariantList m_favorites;
    QVariantList m_history;
    QVariantList m_lyrics;
    QVariantList m_customPlaylists;
    bool m_loading = false;
    QString m_errorMessage;
    QString m_activeSourceName;
    QString m_activeSourceAccess;
    QString m_activeSourceProviderId;
    QString m_manualPlaybackProviderId;
    QStringList m_failedWebProviders;
    QString m_qualityLevel = "exhigh";
    QString m_playerStyle = QStringLiteral("balanced");
    bool m_darkMode = false;
    QString m_accentColor = "#13D9B0";
    bool m_animationsEnabled = true;
    bool m_compactTrackRows = true;
    bool m_showSourceBadges = true;
    QString m_language = QStringLiteral("zh-CN");
    QString m_themePreset = QStringLiteral("evolve");
    QString m_animationLevel = QStringLiteral("rich");
    QString m_cacheDirectory;
    int m_recommendationDiversity = 62;
    QVariantMap m_userProfile;
    QVariantList m_followingUsers;
    QVariantList m_userSearchResults;
    QVariantList m_communityPlaylists;
    QVariantMap m_togetherRoom;
    QVariantList m_togetherInvites;
    QVariantList m_togetherMessages;
    QVariantMap m_togetherAttachmentUrls;
    qint64 m_togetherMessageCursor = 0;
    qint64 m_togetherVoiceCursor = 0;
    QSet<QString> m_togetherAttachmentDownloads;
    QHash<QString, QString> m_togetherPendingSavePaths;
    QSet<QString> m_togetherPendingOpen;
    QAudioSource *m_togetherVoiceSource = nullptr;
    QAudioSink *m_togetherVoiceSink = nullptr;
    QIODevice *m_togetherVoiceInput = nullptr;
    QIODevice *m_togetherVoiceOutput = nullptr;
    QByteArray m_togetherVoiceCaptureBuffer;
    int m_togetherVoiceHangoverFrames = 0;
    bool m_togetherMicActive = false;
    double m_togetherMicLevel = 0.0;
    QString m_togetherVoiceError;
    QString m_fontFamily;
    QString m_systemFontFamily;
    QStringList m_availableFonts;
    int m_onlineUsers = 0;
    QVariantList m_plugins;
    QString m_pluginDirectory;
    QSet<QString> m_seenInviteKeys;
    QTimer m_historySaveTimer;
    QTimer m_cloudStateSaveTimer;
    QTimer m_fastStreamPreferenceTimer;
    QTimer m_togetherPollTimer;
    QTimer m_togetherPublishTimer;
    QTimer m_togetherChatPollTimer;
    QTimer m_togetherVoicePollTimer;
    QTimer m_socialPollTimer;
    QTimer m_presenceTimer;
    QTimer m_communityRefreshTimer;
    QString m_fastStreamTrackId;
    QVariantMap m_fastStreamProvisional;
    QStringList m_fastStreamErrors;
    bool m_fastStreamSettled = false;
    bool m_fastStreamCloudPending = false;
    bool m_fastStreamLocalPending = false;
    bool m_cloudMusicMode = false;
    bool m_cloudOuterFallbackAttempted = false;
    bool m_cloudPlaybackFallbackAttempted = false;
    bool m_roamActive = false;
    bool m_roamLoading = false;
    bool m_roamReplacePending = false;
    QString m_roamReason = QStringLiteral("根据你的喜欢与最近播放持续推荐");
};
