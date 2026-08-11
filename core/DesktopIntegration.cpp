#include "DesktopIntegration.h"

#include <QAction>
#include <QDebug>
#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include "RuntimeDiagnostics.h"
#include <QVersionNumber>

namespace {
bool isDeveloperRuntimePath()
{
    const QString path = QDir::fromNativeSeparators(QCoreApplication::applicationDirPath()).toLower();
    return path.contains(QStringLiteral("/build-mingw"))
        || path.contains(QStringLiteral("/dist/app"));
}
}

DesktopIntegration::DesktopIntegration(QObject *parent)
    : QObject(parent),
      m_settings(QStringLiteral("EvolveMusic"), QStringLiteral("EvolveMusic"))
{
    const QStringList args = QCoreApplication::arguments();
    m_startInBackground = args.contains(QStringLiteral("--background"))
        || args.contains(QStringLiteral("--autostart"));
    m_backgroundEnabled = m_settings.value(QStringLiteral("app/backgroundEnabled"), true).toBool();
    m_autoUpdateEnabled = m_settings.value(QStringLiteral("app/autoUpdateEnabled"), true).toBool();
    m_processProtectionEnabled = m_settings.value(QStringLiteral("app/processProtectionEnabled"), true).toBool();
    m_crashWarningsEnabled = m_settings.value(QStringLiteral("app/crashWarningsEnabled"), true).toBool();
    syncAutoStartFromSystem();
    createTray();

    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(6 * 60 * 60 * 1000);
    connect(m_updateTimer, &QTimer::timeout, this, &DesktopIntegration::checkForUpdates);
    m_updateTimer->start();
}

DesktopIntegration::~DesktopIntegration()
{
    if (m_tray)
        m_tray->hide();
}

void DesktopIntegration::createTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#7357F6")));
    painter.drawRoundedRect(QRectF(4, 4, 56, 56), 16, 16);
    QFont font = painter.font();
    font.setBold(true);
    font.setItalic(true);
    font.setPixelSize(40);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(4, 1, 56, 58), Qt::AlignCenter, QStringLiteral("E"));
    painter.end();

    const QIcon icon(pixmap);
    qApp->setWindowIcon(icon);
    m_tray = new QSystemTrayIcon(icon, this);
    m_tray->setToolTip(QStringLiteral("EvolveMusic"));
    m_menu = new QMenu;
    m_openAction = m_menu->addAction(QStringLiteral("打开 EvolveMusic"));
    m_playPauseAction = m_menu->addAction(QStringLiteral("播放 / 暂停"));
    m_menu->addSeparator();
    m_updateAction = m_menu->addAction(QStringLiteral("检查更新"));
    m_menu->addSeparator();
    m_quitAction = m_menu->addAction(QStringLiteral("退出"));
    m_tray->setContextMenu(m_menu);

    connect(m_openAction, &QAction::triggered, this, &DesktopIntegration::showRequested);
    connect(m_playPauseAction, &QAction::triggered, this, &DesktopIntegration::togglePlayRequested);
    connect(m_updateAction, &QAction::triggered, this, &DesktopIntegration::checkForUpdates);
    connect(m_quitAction, &QAction::triggered, this, &DesktopIntegration::quitApplication);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            emit showRequested();
    });
    m_tray->show();
}

void DesktopIntegration::setBackgroundEnabled(bool enabled)
{
    if (m_backgroundEnabled == enabled)
        return;
    m_backgroundEnabled = enabled;
    m_settings.setValue(QStringLiteral("app/backgroundEnabled"), enabled);
    emit backgroundEnabledChanged();
}

QString DesktopIntegration::autoStartCommand() const
{
    return QStringLiteral("\"") + QDir::toNativeSeparators(QCoreApplication::applicationFilePath())
        + QStringLiteral("\" --autostart --background");
}

void DesktopIntegration::syncAutoStartFromSystem()
{
#ifdef Q_OS_WIN
    QSettings runKey(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                     QSettings::NativeFormat);
    m_autoStartEnabled = !runKey.value(QStringLiteral("EvolveMusic")).toString().trimmed().isEmpty();
#else
    m_autoStartEnabled = false;
#endif
}

void DesktopIntegration::setAutoStartEnabled(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings runKey(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                     QSettings::NativeFormat);
    if (enabled)
        runKey.setValue(QStringLiteral("EvolveMusic"), autoStartCommand());
    else
        runKey.remove(QStringLiteral("EvolveMusic"));
    runKey.sync();
    const bool actual = !runKey.value(QStringLiteral("EvolveMusic")).toString().trimmed().isEmpty();
#else
    const bool actual = false;
    Q_UNUSED(enabled);
#endif
    if (m_autoStartEnabled != actual) {
        m_autoStartEnabled = actual;
        emit autoStartEnabledChanged();
    }
}

void DesktopIntegration::setAutoUpdateEnabled(bool enabled)
{
    if (m_autoUpdateEnabled == enabled)
        return;
    m_autoUpdateEnabled = enabled;
    m_settings.setValue(QStringLiteral("app/autoUpdateEnabled"), enabled);
    emit autoUpdateEnabledChanged();
    if (enabled)
        checkForUpdates();
}


void DesktopIntegration::setProcessProtectionEnabled(bool enabled)
{
    if (m_processProtectionEnabled == enabled)
        return;
    m_processProtectionEnabled = enabled;
    m_settings.setValue(QStringLiteral("app/processProtectionEnabled"), enabled);
    m_settings.sync();
    emit processProtectionEnabledChanged();
    emit toastRequested(enabled
        ? QStringLiteral("进程保护已开启，下次启动后生效")
        : QStringLiteral("进程保护已关闭，下次启动后生效"));
}

void DesktopIntegration::setCrashWarningsEnabled(bool enabled)
{
    if (m_crashWarningsEnabled == enabled)
        return;
    m_crashWarningsEnabled = enabled;
    m_settings.setValue(QStringLiteral("app/crashWarningsEnabled"), enabled);
    m_settings.sync();
    emit crashWarningsEnabledChanged();
}

QString DesktopIntegration::logDirectory() const
{
    return RuntimeDiagnostics::logDirectory();
}

void DesktopIntegration::openLogFolder()
{
    const QString dir = logDirectory();
    QDir().mkpath(dir);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dir)))
        emit toastRequested(QStringLiteral("无法打开日志文件夹：") + QDir::toNativeSeparators(dir));
}

void DesktopIntegration::setCloudEndpoints(const QStringList &urls)
{
    QStringList next;
    for (QString url : urls) {
        url = url.trimmed();
        while (url.endsWith(QLatin1Char('/'))) url.chop(1);
        if (!url.isEmpty() && !next.contains(url)) next << url;
    }
    m_cloudEndpoints = next;
    // Give the UI time to become visible before doing background network work.
    // Developer builds must never disappear a few seconds after launch because
    // an update manifest happened to advertise a newer installer.
    QTimer::singleShot(45000, this, &DesktopIntegration::checkForUpdates);
}

void DesktopIntegration::setUpdateStatus(const QString &status)
{
    if (m_updateStatus == status)
        return;
    m_updateStatus = status;
    emit updateChanged();
}

void DesktopIntegration::checkForUpdates()
{
    if (m_updateBusy || m_cloudEndpoints.isEmpty())
        return;
    m_updateBusy = true;
    emit updateChanged();
    setUpdateStatus(QStringLiteral("正在检查更新…"));
    requestManifest(0);
}

void DesktopIntegration::requestManifest(int endpointIndex)
{
    if (endpointIndex >= m_cloudEndpoints.size()) {
        m_updateBusy = false;
        setUpdateStatus(QStringLiteral("两个更新节点都暂时不可用"));
        emit updateChanged();
        return;
    }
    QNetworkRequest request{QUrl(m_cloudEndpoints.at(endpointIndex) + QStringLiteral("/v1/update/manifest"))};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EvolveMusic/0.18.1 Updater"));
    request.setTransferTimeout(9000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, endpointIndex] {
        const QByteArray body = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() >= 200
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() < 300;
        reply->deleteLater();
        if (!ok) {
            requestManifest(endpointIndex + 1);
            return;
        }
        m_updateBusy = false;
        handleManifest(body);
        emit updateChanged();
    });
}

void DesktopIntegration::handleManifest(const QByteArray &body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        setUpdateStatus(QStringLiteral("更新信息格式异常"));
        return;
    }
    const QJsonObject object = doc.object();
    m_latestVersion = object.value(QStringLiteral("version")).toString().trimmed();
    m_updateUrl = object.value(QStringLiteral("url")).toString().trimmed();
    m_updateSha256 = object.value(QStringLiteral("sha256")).toString().trimmed().toLower();
    const QVersionNumber current = QVersionNumber::fromString(QCoreApplication::applicationVersion());
    const QVersionNumber latest = QVersionNumber::fromString(m_latestVersion);
    m_updateAvailable = !m_latestVersion.isEmpty() && QVersionNumber::compare(latest, current) > 0;
    if (!m_updateAvailable) {
        setUpdateStatus(QStringLiteral("已经是最新版本 · v") + QCoreApplication::applicationVersion());
        return;
    }
    if (m_updateUrl.isEmpty()) {
        setUpdateStatus(QStringLiteral("发现 v") + m_latestVersion + QStringLiteral("，但服务器尚未配置安装包"));
        return;
    }
    setUpdateStatus(QStringLiteral("发现新版本 v") + m_latestVersion);
    if (m_tray)
        m_tray->showMessage(QStringLiteral("EvolveMusic 更新"), m_updateStatus, QSystemTrayIcon::Information, 5000);
    if (m_autoUpdateEnabled) {
        if (isDeveloperRuntimePath()) {
            qInfo() << "Automatic update install suppressed for developer runtime:"
                    << QCoreApplication::applicationFilePath();
            setUpdateStatus(QStringLiteral("发现新版本 v") + m_latestVersion
                            + QStringLiteral(" · 开发构建不会自动退出安装，请手动点击立即更新"));
        } else {
            downloadUpdate();
        }
    }
}

void DesktopIntegration::installAvailableUpdate()
{
    if (!m_updateAvailable || m_updateUrl.isEmpty()) {
        checkForUpdates();
        return;
    }
    downloadUpdate();
}

void DesktopIntegration::downloadUpdate()
{
    if (m_updateBusy || m_updateUrl.isEmpty())
        return;
    m_updateBusy = true;
    setUpdateStatus(QStringLiteral("正在后台下载 v") + m_latestVersion + QStringLiteral("…"));
    emit updateChanged();

    QNetworkRequest request{QUrl(m_updateUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EvolveMusic/0.18.1 Updater"));
    request.setTransferTimeout(120000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError && !data.isEmpty();
        const QString networkError = reply->errorString();
        reply->deleteLater();
        if (!ok) {
            m_updateBusy = false;
            setUpdateStatus(QStringLiteral("更新下载失败：") + networkError);
            emit updateChanged();
            return;
        }
        if (!m_updateSha256.isEmpty()) {
            const QString actual = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()).toLower();
            if (actual != m_updateSha256) {
                m_updateBusy = false;
                setUpdateStatus(QStringLiteral("更新包校验失败，已阻止安装"));
                emit updateChanged();
                return;
            }
        }
        const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(QStringLiteral("EvolveMusicUpdate"));
        QDir().mkpath(dir);
        const QString path = QDir(dir).filePath(QStringLiteral("EvolveMusic-Setup-") + m_latestVersion + QStringLiteral(".exe"));
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
            m_updateBusy = false;
            setUpdateStatus(QStringLiteral("无法保存更新安装包"));
            emit updateChanged();
            return;
        }
        m_downloadedInstaller = path;
        m_updateBusy = false;
        setUpdateStatus(QStringLiteral("更新已下载，正在安装…"));
        emit updateChanged();

        QStringList args{
            QStringLiteral("/VERYSILENT"),
            QStringLiteral("/SUPPRESSMSGBOXES"),
            QStringLiteral("/CLOSEAPPLICATIONS"),
            QStringLiteral("/RESTARTAPPLICATIONS"),
            QStringLiteral("/NORESTART")
        };
        if (QProcess::startDetached(path, args)) {
            m_quitting = true;
            emit quittingChanged();
            if (m_tray) m_tray->hide();
            QTimer::singleShot(500, qApp, &QCoreApplication::quit);
        } else {
            setUpdateStatus(QStringLiteral("无法启动更新安装程序"));
        }
    });
}

void DesktopIntegration::quitApplication()
{
    if (m_quitting)
        return;
    m_quitting = true;
    emit quittingChanged();
    if (m_tray)
        m_tray->hide();
    qApp->quit();
}

void DesktopIntegration::showTrayMessage(const QString &title, const QString &message)
{
    if (m_tray)
        m_tray->showMessage(title, message, QSystemTrayIcon::Information, 3500);
}
