#pragma once

#include <QObject>
#include <QHash>

class WebPlaybackBridge final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool rendererReady READ rendererReady NOTIFY rendererReadyChanged)
    Q_PROPERTY(bool requestPending READ requestPending NOTIFY requestPendingChanged)
    Q_PROPERTY(bool verificationPending READ verificationPending NOTIFY verificationPendingChanged)
public:
    explicit WebPlaybackBridge(QObject *parent = nullptr) : QObject(parent) {}

    bool rendererReady() const { return m_rendererReady; }
    bool requestPending() const { return !m_pendingRequestId.isEmpty(); }
    bool verificationPending() const { return !m_pendingVerificationKeyword.isEmpty(); }
    void setRendererReady(bool ready);
    Q_INVOKABLE void requestYueting(const QString &requestId,
                                    const QString &songId,
                                    bool download = false);
    Q_INVOKABLE void requestTwoT58Verification(const QString &keyword);
    Q_INVOKABLE void verificationCompleted(const QString &cookie);
    Q_INVOKABLE void verificationFailed(const QString &message);
    Q_INVOKABLE void resolveSuccess(const QString &requestId, const QString &url);
    Q_INVOKABLE void resolveFailure(const QString &requestId, const QString &message);

signals:
    void rendererReadyChanged();
    void requestPendingChanged();
    void verificationPendingChanged();
    void yuetingResolveRequested(const QString &requestId,
                                 const QString &songId,
                                 bool download);
    void yuetingResolved(const QString &requestId,
                         const QString &url,
                         bool download);
    void yuetingFailed(const QString &requestId,
                       const QString &message,
                       bool download);
    void twoT58VerificationRequested(const QString &keyword);
    void twoT58VerificationCompleted(const QString &keyword,
                                     const QString &cookie);
    void twoT58VerificationFailed(const QString &keyword,
                                  const QString &message);

private:
    bool m_rendererReady = false;
    QHash<QString, bool> m_pendingDownloads;
    QString m_verificationKeyword;
    QString m_pendingRequestId;
    QString m_pendingSongId;
    bool m_pendingDownload = false;
    QString m_pendingVerificationKeyword;
};
