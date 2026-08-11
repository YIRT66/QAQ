#include "HttpFallbackFetcher.h"
#include "WebProviderUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

namespace HttpFallbackFetcher {

namespace {

QString curlExecutable()
{
    QString found = QStandardPaths::findExecutable(QStringLiteral("curl.exe"));
    if (!found.isEmpty())
        return found;

#ifdef Q_OS_WIN
    const QString systemRoot = qEnvironmentVariable("SystemRoot");
    if (!systemRoot.isEmpty()) {
        const QString candidate =
            QDir(systemRoot).filePath(QStringLiteral("System32/curl.exe"));
        if (QFileInfo::exists(candidate))
            return QDir::toNativeSeparators(candidate);
    }
#endif

    return {};
}

}

bool available()
{
    return !curlExecutable().isEmpty();
}

void fetch(QObject *owner,
           const QUrl &url,
           Success success,
           Failure failure,
           const QString &referer,
           int timeoutMs,
           const QMap<QString, QString> &extraHeaders)
{
    if (!owner || !url.isValid()) {
        if (failure)
            failure(QStringLiteral("HTTP fallback 参数无效"));
        return;
    }

    const QString curl = curlExecutable();
    if (curl.isEmpty()) {
        if (failure)
            failure(QStringLiteral("系统没有找到 curl.exe"));
        return;
    }

    auto *process = new QProcess(owner);
    auto *timer = new QTimer(process);
    timer->setSingleShot(true);
    timer->setInterval(qMax(1200, timeoutMs + 700));

    const int maxSeconds = qMax(2, (timeoutMs + 999) / 1000);
    const int connectSeconds = qMin(2, maxSeconds);

    QStringList args{
        QStringLiteral("--location"),
        QStringLiteral("--silent"),
        QStringLiteral("--show-error"),
        QStringLiteral("--compressed"),
        QStringLiteral("--connect-timeout"),
        QString::number(connectSeconds),
        QStringLiteral("--max-time"),
        QString::number(maxSeconds),
        QStringLiteral("--user-agent"),
        WebProviderUtils::browserUserAgent(),
        QStringLiteral("--header"),
        QStringLiteral("Accept: text/html,application/xhtml+xml,application/json,text/plain;q=0.9,*/*;q=0.5"),
        QStringLiteral("--header"),
        QStringLiteral("Accept-Language: zh-CN,zh;q=0.9,en;q=0.6"),
        QStringLiteral("--header"),
        QStringLiteral("Cache-Control: no-cache"),
        QStringLiteral("--write-out"),
        QStringLiteral("\n__EVOLVE_FINAL_URL__:%{url_effective}"),
        url.toString(QUrl::FullyEncoded)
    };

    if (!referer.isEmpty()) {
        args.insert(args.size() - 3, QStringLiteral("--referer"));
        args.insert(args.size() - 3, referer);
    }

    for (auto it = extraHeaders.constBegin(); it != extraHeaders.constEnd(); ++it) {
        args.insert(args.size() - 3, QStringLiteral("--header"));
        args.insert(args.size() - 3, it.key() + QStringLiteral(": ") + it.value());
    }

    QObject::connect(timer, &QTimer::timeout, process, [process, failure] {
        if (process->state() != QProcess::NotRunning)
            process->kill();
        if (failure)
            failure(QStringLiteral("curl HTTP 请求超时"));
        process->deleteLater();
    });

    QObject::connect(
        process,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        process,
        [process, timer, url, success, failure]
        (int exitCode, QProcess::ExitStatus status) {
            if (!timer->isActive())
                return;
            timer->stop();

            QByteArray out = process->readAllStandardOutput();
            const QByteArray err = process->readAllStandardError();

            if (status != QProcess::NormalExit || exitCode != 0) {
                if (failure) {
                    QString message = QString::fromUtf8(err).simplified();
                    if (message.isEmpty())
                        message = QStringLiteral("curl HTTP 请求失败");
                    failure(message.left(240));
                }
                process->deleteLater();
                return;
            }

            const QByteArray marker("\n__EVOLVE_FINAL_URL__:");
            const qsizetype markerPos = out.lastIndexOf(marker);

            QUrl finalUrl = url;
            QByteArray body = out;
            if (markerPos >= 0) {
                const QByteArray finalRaw =
                    out.mid(markerPos + marker.size()).trimmed();
                body = out.left(markerPos);
                const QUrl parsed(QString::fromUtf8(finalRaw));
                if (parsed.isValid())
                    finalUrl = parsed;
            }

            const QString page =
                WebProviderUtils::decodePageBytes(body);

            if (page.trimmed().isEmpty()) {
                if (failure)
                    failure(QStringLiteral("curl 返回空页面"));
            } else {
                success(page, finalUrl);
            }

            process->deleteLater();
        });

    process->start(curl, args);
    if (!process->waitForStarted(400)) {
        timer->stop();
        if (failure)
            failure(QStringLiteral("curl.exe 无法启动：") + process->errorString());
        process->deleteLater();
        return;
    }

    timer->start();
}

}
