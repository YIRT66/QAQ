#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QStringList>

class QAction;
class QMenu;
class QNetworkReply;
class QSystemTrayIcon;
class QTimer;

class DesktopIntegration final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool backgroundEnabled READ backgroundEnabled WRITE setBackgroundEnabled NOTIFY backgroundEnabledChanged)
    Q_PROPERTY(bool autoStartEnabled READ autoStartEnabled WRITE setAutoStartEnabled NOTIFY autoStartEnabledChanged)
    Q_PROPERTY(bool autoUpdateEnabled READ autoUpdateEnabled WRITE setAutoUpdateEnabled NOTIFY autoUpdateEnabledChanged)
    Q_PROPERTY(bool processProtectionEnabled READ processProtectionEnabled WRITE setProcessProtectionEnabled NOTIFY processProtectionEnabledChanged)
    Q_PROPERTY(bool crashWarningsEnabled READ crashWarningsEnabled WRITE setCrashWarningsEnabled NOTIFY crashWarningsEnabledChanged)
    Q_PROPERTY(QString logDirectory READ logDirectory CONSTANT)
    Q_PROPERTY(bool startInBackground READ startInBackground CONSTANT)
    Q_PROPERTY(bool quitting READ quitting NOTIFY quittingChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateChanged)
    Q_PROPERTY(bool updateBusy READ updateBusy NOTIFY updateChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateChanged)
    Q_PROPERTY(QString updateStatus READ updateStatus NOTIFY updateChanged)

public:
    explicit DesktopIntegration(QObject *parent = nullptr);
    ~DesktopIntegration() override;

    bool backgroundEnabled() const { return m_backgroundEnabled; }
    bool autoStartEnabled() const { return m_autoStartEnabled; }
    bool autoUpdateEnabled() const { return m_autoUpdateEnabled; }
    bool processProtectionEnabled() const { return m_processProtectionEnabled; }
    bool crashWarningsEnabled() const { return m_crashWarningsEnabled; }
    QString logDirectory() const;
    bool startInBackground() const { return m_startInBackground; }
    bool quitting() const { return m_quitting; }
    bool updateAvailable() const { return m_updateAvailable; }
    bool updateBusy() const { return m_updateBusy; }
    QString latestVersion() const { return m_latestVersion; }
    QString updateStatus() const { return m_updateStatus; }

    Q_INVOKABLE void setBackgroundEnabled(bool enabled);
    Q_INVOKABLE void setAutoStartEnabled(bool enabled);
    Q_INVOKABLE void setAutoUpdateEnabled(bool enabled);
    Q_INVOKABLE void setProcessProtectionEnabled(bool enabled);
    Q_INVOKABLE void setCrashWarningsEnabled(bool enabled);
    Q_INVOKABLE void openLogFolder();
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void installAvailableUpdate();
    Q_INVOKABLE void quitApplication();
    Q_INVOKABLE void showTrayMessage(const QString &title, const QString &message);

    void setCloudEndpoints(const QStringList &urls);

signals:
    void backgroundEnabledChanged();
    void autoStartEnabledChanged();
    void autoUpdateEnabledChanged();
    void processProtectionEnabledChanged();
    void crashWarningsEnabledChanged();
    void quittingChanged();
    void updateChanged();
    void showRequested();
    void hideRequested();
    void togglePlayRequested();
    void toastRequested(const QString &message);

private:
    void createTray();
    void syncAutoStartFromSystem();
    void requestManifest(int endpointIndex);
    void handleManifest(const QByteArray &body);
    void downloadUpdate();
    void setUpdateStatus(const QString &status);
    QString autoStartCommand() const;

    QSettings m_settings;
    QNetworkAccessManager m_network;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_playPauseAction = nullptr;
    QAction *m_updateAction = nullptr;
    QAction *m_quitAction = nullptr;
    QTimer *m_updateTimer = nullptr;

    QStringList m_cloudEndpoints;
    bool m_backgroundEnabled = true;
    bool m_autoStartEnabled = false;
    bool m_autoUpdateEnabled = true;
    bool m_processProtectionEnabled = true;
    bool m_crashWarningsEnabled = true;
    bool m_startInBackground = false;
    bool m_quitting = false;
    bool m_updateAvailable = false;
    bool m_updateBusy = false;
    QString m_latestVersion;
    QString m_updateStatus = QStringLiteral("尚未检查更新");
    QString m_updateUrl;
    QString m_updateSha256;
    QString m_downloadedInstaller;
};
