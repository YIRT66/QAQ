#pragma once

#include <QObject>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QVariantList>
#include <QVariantMap>
#include <QUrlQuery>
#include <functional>
#include <QList>
#include <QStringList>

class QNetworkReply;

class CloudMusicClient final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY authChanged)
    Q_PROPERTY(QString userId READ userId NOTIFY identityChanged)
    Q_PROPERTY(QString username READ username NOTIFY authChanged)
    Q_PROPERTY(QString email READ email NOTIFY authChanged)
    Q_PROPERTY(QString baseUrl READ baseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(QStringList baseUrls READ baseUrls NOTIFY baseUrlsChanged)
    Q_PROPERTY(bool authBusy READ authBusy NOTIFY authBusyChanged)
    Q_PROPERTY(QString authError READ authError NOTIFY authErrorChanged)

public:
    explicit CloudMusicClient(QObject *parent = nullptr);

    bool ready() const { return m_ready; }
    bool loggedIn() const { return !m_token.isEmpty() && !m_userId.isEmpty(); }
    QString userId() const { return m_userId; }
    QString username() const { return m_username; }
    QString email() const { return m_email; }
    QString baseUrl() const { return m_baseUrl; }
    QStringList baseUrls() const { return m_baseUrls; }
    bool authBusy() const { return m_authBusy; }
    QString authError() const { return m_authError; }

    void setBaseUrl(const QString &url);
    void setBaseUrls(const QStringList &urls);
    void setProfileDataRoot(const QString &path);

    void initialize();
    Q_INVOKABLE void login(const QString &identifier, const QString &password);
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
    Q_INVOKABLE void logout();

    void loadHome();
    void loadRoam(const QStringList &excludeIds = {},
                  int limit = 18,
                  const QVariantMap &currentTrack = {});
    void cancelPendingSearch();
    void search(const QString &keywords, int limit = 40);
    void loadPlaylist(const QString &providerId,
                      const QString &sourceId);
    void loadLyrics(const QVariantMap &track);
    void requestStreamTicket(const QVariantMap &track,
                             const QString &quality);

    void loadState();
    void saveState(const QVariantList &favorites,
                   const QVariantList &history,
                   const QVariantMap &settings);

    void loadCustomPlaylists();
    void createCustomPlaylist(const QString &name);
    void updateCustomPlaylist(const QString &playlistId,
                              const QString &name,
                              const QVariantList &tracks,
                              const QString &description = {});
    void deleteCustomPlaylist(const QString &playlistId);
    void updateCustomPlaylistMetadata(const QString &playlistId,
                                      const QString &name,
                                      const QString &description,
                                      const QString &coverData,
                                      bool isPublic);

    void loadProfile();
    void updateProfile(const QString &displayName,
                       const QString &bio,
                       const QString &avatarData);
    void searchUsers(const QString &query);
    void setFollowing(const QString &userId, bool follow);
    void loadFollowing();
    void loadCommunityPlaylists(int limit = 24);
    void loadCommunityPlaylist(const QString &playlistId);

    void createTogetherRoom();
    void joinTogetherRoom(const QString &code);
    void loadTogetherRoom(const QString &code);
    void updateTogetherState(const QString &code,
                             const QVariantMap &state);
    void leaveTogetherRoom(const QString &code);
    void inviteTogetherUser(const QString &code,
                            const QString &username);
    void loadTogetherInvites();
    void heartbeatPresence();
    void dismissTogetherInvite(const QString &inviteId);
    void loadTogetherMessages(const QString &code, qint64 afterSequence = 0);
    void sendTogetherText(const QString &code, const QString &text);
    void uploadTogetherAttachment(const QString &code,
                                  const QString &kind,
                                  const QString &fileName,
                                  const QString &mimeType,
                                  const QByteArray &bytes);
    void downloadTogetherAttachment(const QString &code,
                                    const QString &attachmentId);
    void sendTogetherVoiceChunk(const QString &code,
                                const QByteArray &compressedPcm);
    void loadTogetherVoiceChunks(const QString &code, qint64 afterSequence);

    void uploadProviderSession(const QString &providerId,
                               const QString &cookieHeader,
                               const QString &userAgent,
                               const QString &pageUrl);
    void removeProviderSession(const QString &providerId);

signals:
    void readyChanged();
    void authChanged();
    void authBusyChanged();
    void authErrorChanged();
    void identityChanged();
    void baseUrlChanged();
    void baseUrlsChanged();

    void homeReady(const QVariantList &playlists,
                   const QVariantList &tracks);
    void roamReady(const QVariantList &tracks,
                   const QString &reason);
    void searchReady(const QVariantList &tracks);
    void playlistReady(const QVariantMap &playlist,
                       const QVariantList &tracks);
    void lyricsReady(const QString &trackId,
                     const QString &raw,
                     const QString &translated);
    void streamReady(const QString &trackId,
                     const QString &url,
                     const QString &providerName,
                     const QString &access,
                     const QStringList &alternatives);
    void streamFailed(const QString &trackId,
                      const QString &message);
    void stateReady(const QVariantList &favorites,
                    const QVariantList &history,
                    const QVariantMap &settings);
    void customPlaylistsReady(const QVariantList &playlists);
    void customPlaylistSaved(const QVariantMap &playlist);
    void customPlaylistDeleted(const QString &playlistId);

    void profileReady(const QVariantMap &profile);
    void profileSaved(const QVariantMap &profile);
    void userSearchReady(const QVariantList &users);
    void followingReady(const QVariantList &users);
    void communityPlaylistsReady(const QVariantList &playlists);
    void communityPlaylistReady(const QVariantMap &playlist,
                                const QVariantList &tracks);
    void togetherRoomReady(const QVariantMap &room);
    void togetherInvitesReady(const QVariantList &invites);
    void togetherLeft();
    void togetherRoomUnavailable(const QString &code, const QString &message);
    void togetherMessagesReady(const QVariantList &messages, qint64 cursor);
    void togetherMessageSent();
    void togetherAttachmentReady(const QString &attachmentId,
                                 const QByteArray &bytes,
                                 const QString &fileName,
                                 const QString &mimeType);
    void togetherVoiceChunksReady(const QVariantList &chunks, qint64 cursor);
    void presenceReady(int onlineUsers);

    void providerSessionStored(const QString &providerId);
    void providerSessionRemoved(const QString &providerId);

    void errorOccurred(const QString &message);
    void toastRequested(const QString &message);
    void emailCodeSent(const QString &purpose, int cooldownSeconds);
    void passwordResetCompleted();

private:
    using JsonSuccess = std::function<void(const QVariantMap &)>;
    using Failure = std::function<void(const QString &)>;

    QString authFilePath() const;
    void loadIdentity();
    bool saveIdentity() const;
    void clearIdentity();
    void applyAuthResponse(const QVariantMap &response);
    void setAuthBusy(bool busy);
    void setAuthError(const QString &message);
    void setReady(bool ready);
    void setActiveEndpoint(const QString &url);
    QStringList requestEndpoints() const;
    void runWhenReady(std::function<void()> action);
    void flushReadyQueue();

    QString protectToken(const QString &token) const;
    QString unprotectToken(const QString &value) const;

    QString audioCacheDirectory() const;
    QString cachedAudioFile(const QString &trackId,
                            const QString &quality) const;
    QString audioCacheStem(const QString &trackId,
                           const QString &quality) const;
    void prepareCachedStream(const QVariantMap &track,
                             const QString &quality,
                             const QString &remoteUrl,
                             const QString &providerName,
                             const QString &access,
                             const QString &fallbackUrl = {});
    void pruneAudioCache();

    QNetworkReply *requestJson(const QByteArray &method,
                               const QString &path,
                               const QUrlQuery &query,
                               const QVariantMap &body,
                               bool authenticated,
                               JsonSuccess success,
                               Failure failure = {});
    QNetworkReply *requestJsonAttempt(const QByteArray &method,
                                      const QString &path,
                                      const QUrlQuery &query,
                                      const QVariantMap &body,
                                      bool authenticated,
                                      const QStringList &endpoints,
                                      int endpointIndex,
                                      quint64 requestGeneration,
                                      JsonSuccess success,
                                      Failure failure);

    QString m_baseUrl;
    QStringList m_baseUrls;
    QString m_profileDataRoot;
    QString m_userId;
    QString m_username;
    QString m_email;
    QString m_token;
    bool m_ready = false;
    // Guards asynchronous cloud bootstrap. runWhenReady() may be called from
    // auth/identity notifications while initialize() is still hydrating a
    // persisted session; without this guard that path recursively re-entered
    // initialize() until the QML/JS stack overflowed.
    bool m_initializeInFlight = false;
    bool m_communityListInFlight = false;
    bool m_authBusy = false;
    QString m_authError;
    quint64 m_contextGeneration = 0;
    quint64 m_searchGeneration = 0;
    QList<std::function<void()>> m_readyQueue;

    QNetworkAccessManager m_network;
};
