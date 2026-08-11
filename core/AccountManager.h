#pragma once

#include <QObject>
#include <QHash>
#include <QVariantList>
#include <QUrl>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QList>
#include <QPair>
#include <QPointer>
#include <QTimer>

#include "../providers/IMusicProvider.h"

class AccountManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList accounts READ accounts NOTIFY accountsChanged)
    Q_PROPERTY(bool embeddedBrowserAvailable READ embeddedBrowserAvailable CONSTANT)
    Q_PROPERTY(QString activeProviderId READ activeProviderId NOTIFY activeProviderChanged)
    Q_PROPERTY(QString activeProviderName READ activeProviderName NOTIFY activeProviderChanged)
    Q_PROPERTY(QUrl activeLoginUrl READ activeLoginUrl NOTIFY activeProviderChanged)
    Q_PROPERTY(bool nativeQrLoginActive READ nativeQrLoginActive NOTIFY nativeQrLoginChanged)
    Q_PROPERTY(QString qrImageSource READ qrImageSource NOTIFY nativeQrLoginChanged)
    Q_PROPERTY(QString qrLoginStatus READ qrLoginStatus NOTIFY nativeQrLoginChanged)
    Q_PROPERTY(bool qrLoginBusy READ qrLoginBusy NOTIFY nativeQrLoginChanged)
    Q_PROPERTY(QString profileId READ profileId NOTIFY profileChanged)

public:
    explicit AccountManager(QObject *parent = nullptr);

    void registerProvider(IMusicProvider *provider);
    QVariantList accounts() const;
    bool embeddedBrowserAvailable() const;

    QString activeProviderId() const { return m_activeProviderId; }
    QString activeProviderName() const;
    QUrl activeLoginUrl() const;
    bool nativeQrLoginActive() const { return m_nativeQrLoginActive; }
    QString qrImageSource() const { return m_qrImageSource; }
    QString qrLoginStatus() const { return m_qrLoginStatus; }
    bool qrLoginBusy() const { return m_qrLoginBusy; }
    QString profileId() const { return m_profileId; }

    void setProfileId(const QString &profileId);
    Q_INVOKABLE void beginWebLogin(const QString &providerId);
    Q_INVOKABLE void refreshNativeLogin();
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void finishWebLoginPayload(const QString &providerId,
                                           const QString &payloadJson,
                                           const QString &userAgent,
                                           const QString &pageUrl);
    Q_INVOKABLE void refreshAccount(const QString &providerId);
    Q_INVOKABLE void logout(const QString &providerId);
    Q_INVOKABLE QString sessionDataRoot() const;

signals:
    void accountsChanged();
    void activeProviderChanged();
    void browserRequested();
    void browserLoginSucceeded();
    void nativeQrLoginChanged();
    void toastRequested(const QString &message);
    void profileChanged();
    void providerSessionCaptured(const QString &providerId,
                                 const QString &cookieHeader,
                                 const QString &userAgent,
                                 const QString &pageUrl);
    void providerSessionRemoved(const QString &providerId);

private:
    struct AccountState {
        IMusicProvider *provider = nullptr;
        bool loggedIn = false;
        QString displayName;
        QString status = QStringLiteral("未登录");
    };

    struct SessionSnapshot {
        QString cookieHeader;
        QString userAgent;
        QString storageJson;
        QString pageUrl;
        bool isEmpty() const {
            return cookieHeader.isEmpty() && storageJson.isEmpty();
        }
    };

    IMusicProvider *provider(const QString &providerId) const;
    void updateProviderState(const QString &providerId, bool loggedIn,
                             const QString &displayName, const QString &message);

    void startNeteaseQrLogin();
    void requestNeteaseQrImage(const QString &key);
    void pollNeteaseQrStatus();
    void stopNeteaseQrLogin(bool clearVisuals = false);
    void setQrState(const QString &status, bool busy, const QString &imageSource = QString());
    QNetworkReply *neteaseLoginGet(const QString &path,
                                   const QList<QPair<QString, QString>> &query);

    QString profileDataRoot() const;
    QString legacySessionDataRoot() const;
    void migrateLegacySessionsIfNeeded();
    void restoreSavedSessions();
    QString sessionFilePath(const QString &providerId) const;
    bool saveSession(const QString &providerId, const SessionSnapshot &session);
    SessionSnapshot loadSession(const QString &providerId) const;
    void removeSession(const QString &providerId);

    static QByteArray protectForCurrentUser(const QByteArray &plain);
    static QByteArray unprotectForCurrentUser(const QByteArray &encrypted);

    QHash<QString, AccountState> m_accounts;
    QStringList m_order;
    QString m_activeProviderId;

    QNetworkAccessManager m_loginNetwork;
    QTimer m_qrPollTimer;
    QPointer<QNetworkReply> m_qrReply;
    QString m_qrKey;
    QString m_qrImageSource;
    QString m_qrLoginStatus;
    bool m_nativeQrLoginActive = false;
    bool m_qrLoginBusy = false;
    bool m_qrNoCookieFallback = false;
    bool m_switchingProfile = false;
    QString m_profileId = QStringLiteral("default");
};
