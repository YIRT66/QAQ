#include "WebPlaybackBridge.h"

void WebPlaybackBridge::setRendererReady(bool ready)
{
    if (m_rendererReady == ready)
        return;
    m_rendererReady = ready;
    emit rendererReadyChanged();
    if (ready) {
        if (!m_pendingRequestId.isEmpty()) {
            const QString requestId = m_pendingRequestId;
            const QString songId = m_pendingSongId;
            const bool download = m_pendingDownload;
            m_pendingRequestId.clear();
            m_pendingSongId.clear();
            m_pendingDownload = false;
            emit requestPendingChanged();
            m_pendingDownloads.insert(requestId, download);
            emit yuetingResolveRequested(requestId, songId, download);
        }
        if (!m_pendingVerificationKeyword.isEmpty()) {
            const QString keyword = m_pendingVerificationKeyword;
            m_pendingVerificationKeyword.clear();
            emit verificationPendingChanged();
            m_verificationKeyword = keyword;
            emit twoT58VerificationRequested(keyword);
        }
    }
}

void WebPlaybackBridge::requestYueting(const QString &requestId,
                                       const QString &songId,
                                       bool download)
{
    if (!m_rendererReady) {
        m_pendingRequestId = requestId;
        m_pendingSongId = songId;
        m_pendingDownload = download;
        emit requestPendingChanged();
        return;
    }
    m_pendingDownloads.insert(requestId, download);
    emit yuetingResolveRequested(requestId, songId, download);
}

void WebPlaybackBridge::resolveSuccess(const QString &requestId, const QString &url)
{
    const bool download = m_pendingDownloads.take(requestId);
    if (!url.trimmed().isEmpty())
        emit yuetingResolved(requestId, url.trimmed(), download);
    else
        emit yuetingFailed(requestId, QStringLiteral("网页没有返回音频地址"), download);
}

void WebPlaybackBridge::resolveFailure(const QString &requestId, const QString &message)
{
    const bool download = m_pendingDownloads.take(requestId);
    emit yuetingFailed(requestId,
                       message.trimmed().isEmpty()
                           ? QStringLiteral("悦听网页解析失败")
                           : message,
                       download);
}

void WebPlaybackBridge::requestTwoT58Verification(const QString &keyword)
{
    m_verificationKeyword = keyword.trimmed();
    if (!m_rendererReady) {
        m_pendingVerificationKeyword = m_verificationKeyword;
        emit verificationPendingChanged();
        return;
    }
    emit twoT58VerificationRequested(m_verificationKeyword);
}

void WebPlaybackBridge::verificationCompleted(const QString &cookie)
{
    emit twoT58VerificationCompleted(m_verificationKeyword, cookie.trimmed());
    m_verificationKeyword.clear();
}

void WebPlaybackBridge::verificationFailed(const QString &message)
{
    emit twoT58VerificationFailed(m_verificationKeyword, message);
    m_verificationKeyword.clear();
}
