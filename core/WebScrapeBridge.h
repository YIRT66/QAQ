#pragma once

#include <QObject>
#include <QQueue>
#include <QTimer>
#include <QUrl>

class WebScrapeBridge final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool rendererReady READ rendererReady NOTIFY rendererReadyChanged)

public:
    explicit WebScrapeBridge(QObject *parent = nullptr);

    bool rendererReady() const { return m_rendererReady; }

    Q_INVOKABLE void setRendererReady(bool ready);

    // mode:
    //   "capture"     - load URL, execute JS, return rendered DOM
    //   "form-search" - load URL, fill the site's own search form with keyword,
    //                   submit it, then return the rendered results DOM
    int requestPage(const QString &providerId,
                    const QString &purpose,
                    const QUrl &url,
                    const QString &mode = QStringLiteral("capture"),
                    const QString &keyword = QString(),
                    int settleMs = 850);

    Q_INVOKABLE void complete(int requestId,
                              const QString &html,
                              const QString &finalUrl);

    Q_INVOKABLE void fail(int requestId, const QString &message);
    void cancelProvider(const QString &providerId);

signals:
    void rendererReadyChanged();

    void renderRequested(int requestId,
                         const QString &providerId,
                         const QString &purpose,
                         const QUrl &url,
                         const QString &mode,
                         const QString &keyword,
                         int settleMs);

    void pageReady(int requestId,
                   const QString &providerId,
                   const QString &purpose,
                   const QString &html,
                   const QUrl &finalUrl);

    void pageFailed(int requestId,
                    const QString &providerId,
                    const QString &purpose,
                    const QString &message);

private:
    struct Request {
        int id = -1;
        QString providerId;
        QString purpose;
        QUrl url;
        QString mode;
        QString keyword;
        int settleMs = 850;
    };

    void dispatchNext();
    void finishActive(bool success,
                      const QString &payload,
                      const QString &finalUrl = QString());

    QQueue<Request> m_queue;
    Request m_active;
    bool m_hasActive = false;
    bool m_rendererReady = false;
    int m_nextId = 1;
    QTimer m_timeout;
};
