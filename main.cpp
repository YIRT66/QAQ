#include <cstdlib>
#include <memory>

#include <QApplication>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTimer>

#ifdef EVOLVEMUSIC_HAS_WEBVIEW
#include <QtWebView>
#endif

#include "core/AppController.h"
#include "core/CachedNetworkAccessManagerFactory.h"
#include "core/DesktopIntegration.h"
#include "core/RuntimeDiagnostics.h"

int main(int argc, char *argv[])
{
    // The process-protection watchdog is the same executable in a tiny
    // QCoreApplication mode. Handle it before WebView/Quick are initialized.
    if (RuntimeDiagnostics::isWatchdogInvocation(argc, argv))
        return RuntimeDiagnostics::runWatchdog(argc, argv);

    if (qEnvironmentVariableIsEmpty("QT_MEDIA_BACKEND"))
        qputenv("QT_MEDIA_BACKEND", "ffmpeg");
#ifdef Q_OS_WIN
    if (qEnvironmentVariableIsEmpty("WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS"))
        qputenv("WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",
                "--autoplay-policy=no-user-gesture-required");
#endif
#ifdef EVOLVEMUSIC_HAS_WEBVIEW
    QtWebView::initialize();
#endif

    QApplication app(argc, argv);
    app.setApplicationName("EvolveMusic");
    app.setApplicationDisplayName("EvolveMusic");
    app.setApplicationVersion("0.18.1");
    app.setOrganizationName("EvolveMusic");
    app.setQuitOnLastWindowClosed(false);
    QQuickStyle::setStyle("Basic");

    // Avoid two full EvolveMusic instances fighting over the player, tray,
    // session files and auto-update state. The watchdog starts only after the
    // protected process has exited, so it does not conflict with this lock.
    const QString runtimeDir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("runtime"));
    QDir().mkpath(runtimeDir);
    auto instanceLock = std::make_unique<QLockFile>(QDir(runtimeDir).filePath(QStringLiteral("EvolveMusic.lock")));
    instanceLock->setStaleLockTime(0);
    if (!instanceLock->tryLock(120)) {
        QMessageBox::information(nullptr,
                                 QStringLiteral("EvolveMusic 已在运行"),
                                 QStringLiteral("EvolveMusic 已经在后台或任务栏托盘中运行。\n\n"
                                                "请从托盘图标打开现有窗口。"));
        return 0;
    }

    const RuntimeDiagnostics::StartupState startupState = RuntimeDiagnostics::initialize(app);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [] {
        RuntimeDiagnostics::markCleanExit();
    });

    if (qEnvironmentVariableIsEmpty("QML_DISK_CACHE_PATH")) {
        QString dataRoot = qEnvironmentVariable("EVOLVE_MUSIC_DATA_ROOT").trimmed();
        if (dataRoot.isEmpty())
            dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        const QString qmlCacheDir = QDir(dataRoot).filePath(QStringLiteral("qml-cache"));
        QDir().mkpath(qmlCacheDir);
        qputenv("QML_DISK_CACHE_PATH", QDir::toNativeSeparators(qmlCacheDir).toUtf8());
    }

    qInfo() << "Creating AppController...";
    AppController controller;
    qInfo() << "Creating DesktopIntegration...";
    DesktopIntegration desktop;
    qInfo() << "Core controllers created.";
    desktop.setCloudEndpoints(controller.cloudApiUrls());
    QObject::connect(&controller, &AppController::cloudRouteChanged,
                     &desktop, [&controller, &desktop] {
        desktop.setCloudEndpoints(controller.cloudApiUrls());
    });

    QQmlApplicationEngine engine;
    engine.setNetworkAccessManagerFactory(new CachedNetworkAccessManagerFactory);
    engine.rootContext()->setContextProperty("app", &controller);
    engine.rootContext()->setContextProperty("player", controller.player());
    engine.rootContext()->setContextProperty("accounts", controller.accounts());
    engine.rootContext()->setContextProperty("profiles", controller.profiles());
    engine.rootContext()->setContextProperty("desktop", &desktop);

    QObject::connect(&desktop, &DesktopIntegration::togglePlayRequested,
                     controller.player(), &PlayerController::togglePlay);
    QObject::connect(&desktop, &DesktopIntegration::toastRequested,
                     &controller, &AppController::toastRequested);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [&app] {
        qCritical() << "QML root object creation failed";
        QMessageBox::critical(
            nullptr,
            QStringLiteral("EvolveMusic 启动失败"),
            QStringLiteral("主界面 QML 加载失败。\n\n错误日志：\n%1\n\n请把该日志发来定位。")
                .arg(QDir::toNativeSeparators(RuntimeDiagnostics::currentLogFile())));
        QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    qInfo() << "Loading QML module EvolveMusic/Main...";
    engine.loadFromModule("EvolveMusic", "Main");
    qInfo() << "QML load request completed. Root objects:" << engine.rootObjects().size();

    // A QML syntax/type error means there is no application window to protect.
    // Do not launch the watchdog or initialize background services in that state,
    // otherwise a broken build can repeatedly respawn itself.
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "No QML root object; waiting for startup-failure handler.";
        return app.exec();
    }

    // Start a lightweight watchdog. build_windows.bat terminates every
    // EvolveMusic.exe before a clean build, so both the client and watchdog
    // are stopped together and cannot lock the linker output.
    RuntimeDiagnostics::startWatchdogIfEnabled();

    if (startupState.previousCrashDetected) {
        QTimer::singleShot(1100, &app, [startupState] {
            RuntimeDiagnostics::showPreviousCrashWarning(startupState);
        });
    }

    QTimer::singleShot(0, &controller, [&controller] {
        qInfo() << "AppController initialize() starting...";
        controller.initialize();
        qInfo() << "AppController initialize() returned.";
    });
    const int code = app.exec();
    qInfo() << "Qt event loop exited with code" << code;
    RuntimeDiagnostics::markCleanExit();
    return code;
}
