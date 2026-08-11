#include "WebScrapeBridge.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include <algorithm>


namespace {

void writeWebSourceLog(const QString &line)
{
    QDir dir(QCoreApplication::applicationDirPath());
    dir.mkpath(QStringLiteral("runtime/client-data"));

    QFile file(dir.filePath(
        QStringLiteral("runtime/client-data/web-sources.log")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
           << "  " << line << '\n';
}

}

WebScrapeBridge::WebScrapeBridge(QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(9000);

    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_hasActive)
            finishActive(false, QStringLiteral("网页渲染超时"));
    });
}

void WebScrapeBridge::setRendererReady(bool ready)
{
    if (m_rendererReady == ready)
        return;

    m_rendererReady = ready;
    emit rendererReadyChanged();

    if (ready)
        dispatchNext();
}

int WebScrapeBridge::requestPage(const QString &providerId,
                                 const QString &purpose,
                                 const QUrl &url,
                                 const QString &mode,
                                 const QString &keyword,
                                 int settleMs)
{
    if (!url.isValid() || url.scheme().isEmpty())
        return -1;

    Request request;
    request.id = m_nextId++;
    request.providerId = providerId;
    request.purpose = purpose;
    request.url = url;
    request.mode = mode.isEmpty() ? QStringLiteral("capture") : mode;
    request.keyword = keyword;
    request.settleMs = std::clamp(settleMs, 200, 2200);

    m_queue.enqueue(request);
    dispatchNext();
    return request.id;
}

void WebScrapeBridge::complete(int requestId,
                               const QString &html,
                               const QString &finalUrl)
{
    if (!m_hasActive || requestId != m_active.id)
        return;

    finishActive(true, html, finalUrl);
}

void WebScrapeBridge::fail(int requestId, const QString &message)
{
    if (!m_hasActive || requestId != m_active.id)
        return;

    finishActive(false, message);
}

void WebScrapeBridge::cancelProvider(const QString &providerId)
{
    QQueue<Request> kept;
    while (!m_queue.isEmpty()) {
        Request request = m_queue.dequeue();
        if (request.providerId != providerId)
            kept.enqueue(request);
    }
    m_queue = kept;

    if (m_hasActive && m_active.providerId == providerId)
        finishActive(false, QStringLiteral("请求已取消"));
}

void WebScrapeBridge::dispatchNext()
{
    if (!m_rendererReady || m_hasActive || m_queue.isEmpty())
        return;

    m_active = m_queue.dequeue();
    m_hasActive = true;
    m_timeout.start();

    writeWebSourceLog(
        QStringLiteral("REQUEST  id=%1 provider=%2 purpose=%3 mode=%4 url=%5")
            .arg(m_active.id)
            .arg(m_active.providerId,
                 m_active.purpose,
                 m_active.mode,
                 m_active.url.toString()));

    emit renderRequested(m_active.id,
                         m_active.providerId,
                         m_active.purpose,
                         m_active.url,
                         m_active.mode,
                         m_active.keyword,
                         m_active.settleMs);
}

void WebScrapeBridge::finishActive(bool success,
                                   const QString &payload,
                                   const QString &finalUrl)
{
    if (!m_hasActive)
        return;

    m_timeout.stop();

    const Request done = m_active;
    m_active = {};
    m_hasActive = false;

    if (success) {
        const QUrl resolved =
            finalUrl.isEmpty() ? done.url : QUrl(finalUrl);

        writeWebSourceLog(
            QStringLiteral("READY    id=%1 provider=%2 purpose=%3 chars=%4 final=%5")
                .arg(done.id)
                .arg(done.providerId,
                     done.purpose)
                .arg(payload.size())
                .arg(resolved.toString()));

        emit pageReady(done.id,
                       done.providerId,
                       done.purpose,
                       payload,
                       resolved);
    } else {
        writeWebSourceLog(
            QStringLiteral("FAILED   id=%1 provider=%2 purpose=%3 error=%4")
                .arg(done.id)
                .arg(done.providerId,
                     done.purpose,
                     payload.left(300)));

        emit pageFailed(done.id,
                        done.providerId,
                        done.purpose,
                        payload);
    }

    QTimer::singleShot(0, this, &WebScrapeBridge::dispatchNext);
}
