#include "RuntimeDiagnostics.h"

#include <QApplication>
#include <QAbstractButton>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QMessageBox>
#include <QMutex>
#include <QMutexLocker>
#include <QOperatingSystemVersion>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QUrl>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#endif

namespace RuntimeDiagnostics {
namespace {
QMutex g_logMutex;
QString g_logFile;
QString g_logDir;
QString g_runtimeDir;
QString g_sessionFile;
QString g_version;
std::atomic_bool g_cleanExit{false};
std::atomic_bool g_guardActive{false};

QString runtimeRoot()
{
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (root.trimmed().isEmpty())
        root = QDir::homePath() + QStringLiteral("/.evolvemusic");
    return root;
}

void writeJsonFile(const QString &path, const QJsonObject &object)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    file.commit();
}

QJsonObject readJsonFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

bool pidIsAlive(qint64 pid)
{
    if (pid <= 0)
        return false;
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(pid));
    if (!process)
        return false;
    const DWORD state = WaitForSingleObject(process, 0);
    CloseHandle(process);
    return state == WAIT_TIMEOUT;
#else
    return false;
#endif
}

void rotateLogs()
{
    QDir dir(g_logDir);
    const auto entries = dir.entryInfoList({QStringLiteral("evolvemusic-*.log"), QStringLiteral("crash-*.txt"), QStringLiteral("crash-*.dmp")},
                                           QDir::Files, QDir::Time);
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-14);
    int kept = 0;
    for (const QFileInfo &info : entries) {
        ++kept;
        if (info.lastModified() < cutoff || kept > 40)
            QFile::remove(info.absoluteFilePath());
    }

    QFileInfo current(g_logFile);
    if (current.exists() && current.size() > 8 * 1024 * 1024) {
        const QString rotated = current.absolutePath() + QLatin1Char('/')
            + current.completeBaseName() + QStringLiteral("-")
            + QDateTime::currentDateTime().toString(QStringLiteral("HHmmss"))
            + QStringLiteral(".log");
        QFile::rename(g_logFile, rotated);
    }
}

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO";
    case QtWarningMsg: return "WARN";
    case QtCriticalMsg: return "CRITICAL";
    case QtFatalMsg: return "FATAL";
    }
    return "INFO";
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const QString line = QStringLiteral("%1 [%2] [pid=%3 tid=%4] %5%6\n")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
             QString::fromLatin1(levelName(type)),
             QString::number(QCoreApplication::applicationPid()),
             QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId())),
             message,
             (context.file && *context.file)
                ? QStringLiteral(" (%1:%2)").arg(QString::fromUtf8(context.file)).arg(context.line)
                : QString());

    {
        QMutexLocker locker(&g_logMutex);
        QFile file(g_logFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            file.write(line.toUtf8());
            file.flush();
        }
    }

#ifdef Q_OS_WIN
    OutputDebugStringW(reinterpret_cast<LPCWSTR>(line.utf16()));
#endif
    if (type == QtFatalMsg)
        std::abort();
}

QString createCrashReportPath(const QString &suffix)
{
    return QDir(g_logDir).filePath(
        QStringLiteral("crash-%1-pid%2.%3")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")))
            .arg(QCoreApplication::applicationPid())
            .arg(suffix));
}

void writeCrashText(const QString &reason, quint64 code = 0, quint64 address = 0)
{
    const QString path = createCrashReportPath(QStringLiteral("txt"));
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "EvolveMusic crash report\n"
            << "Version: " << g_version << "\n"
            << "Time: " << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << "\n"
            << "PID: " << QCoreApplication::applicationPid() << "\n"
            << "Reason: " << reason << "\n";
        if (code)
            out << "ExceptionCode: 0x" << QString::number(code, 16) << "\n";
        if (address)
            out << "ExceptionAddress: 0x" << QString::number(address, 16) << "\n";
        out << "Log: " << g_logFile << "\n";
    }
}

#ifdef Q_OS_WIN
LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS *exceptionInfo)
{
    const DWORD code = exceptionInfo && exceptionInfo->ExceptionRecord
        ? exceptionInfo->ExceptionRecord->ExceptionCode : 0;
    const quint64 address = exceptionInfo && exceptionInfo->ExceptionRecord
        ? reinterpret_cast<quint64>(exceptionInfo->ExceptionRecord->ExceptionAddress) : 0;

    writeCrashText(QStringLiteral("Windows unhandled exception"), code, address);

    const QString dumpPath = createCrashReportPath(QStringLiteral("dmp"));
    HANDLE dump = CreateFileW(reinterpret_cast<LPCWSTR>(dumpPath.utf16()), GENERIC_WRITE,
                              FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dump != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = exceptionInfo;
        mei.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump,
                          MiniDumpNormal, exceptionInfo ? &mei : nullptr, nullptr, nullptr);
        CloseHandle(dump);
    }

    const QString text = g_guardActive.load()
        ? QStringLiteral("EvolveMusic 遇到异常并已记录崩溃信息。\n\n进程保护会尝试自动重新启动。\n\n日志目录：\n%1")
              .arg(QDir::toNativeSeparators(g_logDir))
        : QStringLiteral("EvolveMusic 遇到异常并已记录崩溃信息。\n\n日志目录：\n%1")
              .arg(QDir::toNativeSeparators(g_logDir));
    MessageBoxW(nullptr, reinterpret_cast<LPCWSTR>(text.utf16()), L"EvolveMusic 崩溃警告",
                MB_OK | MB_ICONERROR | MB_TOPMOST);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void terminateHandler()
{
    writeCrashText(QStringLiteral("std::terminate"));
#ifdef Q_OS_WIN
    const QString text = QStringLiteral("EvolveMusic 发生未处理的 C++ 异常。\n\n已写入崩溃日志：\n%1")
        .arg(QDir::toNativeSeparators(g_logDir));
    MessageBoxW(nullptr, reinterpret_cast<LPCWSTR>(text.utf16()), L"EvolveMusic 严重错误",
                MB_OK | MB_ICONERROR | MB_TOPMOST);
#endif
    std::abort();
}


StartupState detectPreviousCrash()
{
    StartupState state;
    state.logFile = g_logFile;
    state.logDirectory = g_logDir;

    QDir dir(g_runtimeDir);
    const QFileInfoList sessions = dir.entryInfoList({QStringLiteral("session-*.json")}, QDir::Files, QDir::Time);
    const QDateTime sessionCutoff = QDateTime::currentDateTime().addDays(-14);
    for (const QFileInfo &info : sessions) {
        if (info.lastModified() < sessionCutoff) {
            QFile::remove(info.absoluteFilePath());
            continue;
        }
        const QJsonObject obj = readJsonFile(info.absoluteFilePath());
        if (obj.value(QStringLiteral("cleanExit")).toBool(false))
            continue;
        if (obj.value(QStringLiteral("reported")).toBool(false))
            continue;
        const qint64 pid = qint64(obj.value(QStringLiteral("pid")).toDouble());
        if (pid == QCoreApplication::applicationPid() || pidIsAlive(pid))
            continue;

        state.previousCrashDetected = true;
        state.previousCrashSummary = QStringLiteral("上一次会话异常结束（PID %1，启动时间 %2）。")
            .arg(pid)
            .arg(obj.value(QStringLiteral("startedAt")).toString());
        QJsonObject updated = obj;
        updated.insert(QStringLiteral("reported"), true);
        writeJsonFile(info.absoluteFilePath(), updated);
        break;
    }
    return state;
}

void beginSession()
{
    QDir().mkpath(g_runtimeDir);
    g_sessionFile = QDir(g_runtimeDir).filePath(
        QStringLiteral("session-%1.json").arg(QCoreApplication::applicationPid()));
    QJsonObject obj;
    obj.insert(QStringLiteral("pid"), double(QCoreApplication::applicationPid()));
    obj.insert(QStringLiteral("version"), g_version);
    obj.insert(QStringLiteral("startedAt"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    obj.insert(QStringLiteral("cleanExit"), false);
    obj.insert(QStringLiteral("reported"), false);
    obj.insert(QStringLiteral("exe"), QCoreApplication::applicationFilePath());
    writeJsonFile(g_sessionFile, obj);
}

} // namespace

bool isWatchdogInvocation(int argc, char *argv[])
{
    return argc >= 4 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--watchdog");
}

int runWatchdog(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("EvolveMusic"));
    app.setApplicationName(QStringLiteral("EvolveMusic"));

    const QStringList args = app.arguments();
    if (args.size() < 4)
        return 2;

    bool ok = false;
    const qint64 targetPid = args.at(2).toLongLong(&ok);
    const QString sessionPath = args.at(3);
    const QString exePath = args.size() >= 5 ? args.at(4) : QString();
    if (!ok || targetPid <= 0 || exePath.isEmpty())
        return 2;

#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(targetPid));
    if (!process)
        return 0;
    WaitForSingleObject(process, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process, &exitCode);
    CloseHandle(process);
#else
    const int exitCode = 1;
#endif

    const QJsonObject session = readJsonFile(sessionPath);
    if (session.value(QStringLiteral("cleanExit")).toBool(false) || exitCode == 0)
        return 0;

    QSettings settings(QStringLiteral("EvolveMusic"), QStringLiteral("EvolveMusic"));
    if (!settings.value(QStringLiteral("app/processProtectionEnabled"), true).toBool())
        return 0;

    const QString runtimeDir = QFileInfo(sessionPath).absolutePath();
    const QString guardPath = QDir(runtimeDir).filePath(QStringLiteral("restart-guard.json"));
    QJsonObject guard = readJsonFile(guardPath);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 last = qint64(guard.value(QStringLiteral("lastCrashMs")).toDouble());
    int count = (now - last <= 120000) ? guard.value(QStringLiteral("count")).toInt(0) + 1 : 1;
    guard.insert(QStringLiteral("lastCrashMs"), double(now));
    guard.insert(QStringLiteral("count"), count);
    guard.insert(QStringLiteral("exitCode"), int(exitCode));
    writeJsonFile(guardPath, guard);

    const QString lastCrash = QDir(runtimeDir).filePath(QStringLiteral("last-crash.json"));
    QJsonObject crash;
    crash.insert(QStringLiteral("time"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    crash.insert(QStringLiteral("exitCode"), int(exitCode));
    crash.insert(QStringLiteral("recoveryAttempt"), count);
    crash.insert(QStringLiteral("session"), sessionPath);
    writeJsonFile(lastCrash, crash);

    if (count > 2) {
#ifdef Q_OS_WIN
        const QString text = QStringLiteral("EvolveMusic 在 2 分钟内连续异常退出 %1 次。\n\n为避免重启循环，进程保护已暂停自动恢复。请查看日志后再启动程序。")
            .arg(count);
        MessageBoxW(nullptr, reinterpret_cast<LPCWSTR>(text.utf16()), L"EvolveMusic 进程保护",
                    MB_OK | MB_ICONWARNING | MB_TOPMOST);
#endif
        return 3;
    }

    QThread::msleep(850);
    return QProcess::startDetached(exePath, {QStringLiteral("--recovered")}) ? 0 : 4;
}

StartupState initialize(QApplication &app)
{
    g_version = app.applicationVersion();
    const QString root = runtimeRoot();
    g_logDir = QDir(root).filePath(QStringLiteral("logs"));
    g_runtimeDir = QDir(root).filePath(QStringLiteral("runtime"));
    QDir().mkpath(g_logDir);
    QDir().mkpath(g_runtimeDir);
    g_logFile = QDir(g_logDir).filePath(
        QStringLiteral("evolvemusic-%1.log").arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));
    rotateLogs();

    qInstallMessageHandler(qtMessageHandler);
    std::set_terminate(terminateHandler);
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
#endif

    StartupState state = detectPreviousCrash();
    beginSession();

    qInfo().noquote() << QStringLiteral("================ EvolveMusic session start ================");
    qInfo() << "Version:" << g_version;
    qInfo() << "Executable:" << QCoreApplication::applicationFilePath();
    qInfo() << "Arguments:" << QCoreApplication::arguments();
    qInfo() << "Qt:" << qVersion();
    qInfo() << "OS:" << QOperatingSystemVersion::current().name()
            << QOperatingSystemVersion::current().majorVersion()
            << QOperatingSystemVersion::current().minorVersion()
            << QOperatingSystemVersion::current().microVersion();
    qInfo() << "Log file:" << g_logFile;
    if (state.previousCrashDetected)
        qWarning() << state.previousCrashSummary;
    return state;
}

void markCleanExit()
{
    if (g_cleanExit.exchange(true) || g_sessionFile.isEmpty())
        return;
    QJsonObject obj = readJsonFile(g_sessionFile);
    obj.insert(QStringLiteral("cleanExit"), true);
    obj.insert(QStringLiteral("endedAt"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    writeJsonFile(g_sessionFile, obj);
    qInfo() << "Clean shutdown recorded.";
}

void startWatchdogIfEnabled()
{
    QSettings settings(QStringLiteral("EvolveMusic"), QStringLiteral("EvolveMusic"));
    if (!settings.value(QStringLiteral("app/processProtectionEnabled"), true).toBool()) {
        qInfo() << "Process protection disabled by user setting.";
        return;
    }
    if (qEnvironmentVariableIntValue("EVOLVE_DISABLE_PROCESS_GUARD") == 1) {
        qInfo() << "Process protection disabled by EVOLVE_DISABLE_PROCESS_GUARD.";
        return;
    }
    if (g_sessionFile.isEmpty())
        return;

    const QString exe = QCoreApplication::applicationFilePath();
    const bool started = QProcess::startDetached(exe, {
        QStringLiteral("--watchdog"),
        QString::number(QCoreApplication::applicationPid()),
        g_sessionFile,
        exe
    });
    g_guardActive.store(started);
    if (started)
        qInfo() << "Process protection watchdog started.";
    else
        qWarning() << "Failed to start process protection watchdog.";
}

void showPreviousCrashWarning(const StartupState &state)
{
    if (!state.previousCrashDetected)
        return;
    QSettings settings(QStringLiteral("EvolveMusic"), QStringLiteral("EvolveMusic"));
    if (!settings.value(QStringLiteral("app/crashWarningsEnabled"), true).toBool())
        return;

    QMessageBox box;
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(QStringLiteral("EvolveMusic 已从异常退出中恢复"));
    box.setText(QStringLiteral("检测到上一次运行没有正常结束。"));
    box.setInformativeText(state.previousCrashSummary
        + QStringLiteral("\n\n本次启动已继续运行，日志会保留用于定位问题。"));
    QAbstractButton *openButton = box.addButton(QStringLiteral("打开日志文件夹"), QMessageBox::ActionRole);
    box.addButton(QStringLiteral("继续使用"), QMessageBox::AcceptRole);
    box.exec();
    if (box.clickedButton() == openButton)
        QDesktopServices::openUrl(QUrl::fromLocalFile(state.logDirectory));
}

QString currentLogFile()
{
    return g_logFile;
}

QString logDirectory()
{
    if (!g_logDir.isEmpty())
        return g_logDir;
    return QDir(runtimeRoot()).filePath(QStringLiteral("logs"));
}

} // namespace RuntimeDiagnostics
