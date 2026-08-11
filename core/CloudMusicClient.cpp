#include "CloudMusicClient.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSharedPointer>
#include <QUrlQuery>
#include <QUrl>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

CloudMusicClient::CloudMusicClient(QObject *parent)
    : QObject(parent)
{
}

static QString normalizeEndpoint(QString url)
{
    url = url.trimmed();
    while (url.endsWith(QLatin1Char('/')))
        url.chop(1);
    if (!url.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)
        && !url.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive))
        return {};
    return url;
}

void CloudMusicClient::setBaseUrl(const QString &url)
{
    setBaseUrls(QStringList{url});
}

void CloudMusicClient::setBaseUrls(const QStringList &urls)
{
    QStringList normalized;
    for (const QString &raw : urls) {
        const QString value = normalizeEndpoint(raw);
        if (!value.isEmpty() && !normalized.contains(value))
            normalized << value;
    }
    if (normalized == m_baseUrls)
        return;

    ++m_contextGeneration;
    ++m_searchGeneration;
    m_readyQueue.clear();
    // Any bootstrap request from the previous endpoint generation is obsolete.
    m_initializeInFlight = false;
    m_communityListInFlight = false;
    m_baseUrls = normalized;
    const QString first = m_baseUrls.value(0);
    if (m_baseUrl != first) {
        m_baseUrl = first;
        emit baseUrlChanged();
    }
    emit baseUrlsChanged();

    if (!m_profileDataRoot.isEmpty())
        loadIdentity();
    setReady(false);
}

void CloudMusicClient::setActiveEndpoint(const QString &url)
{
    const QString normalized = normalizeEndpoint(url);
    if (normalized.isEmpty() || m_baseUrl == normalized)
        return;
    m_baseUrl = normalized;
    emit baseUrlChanged();
}

QStringList CloudMusicClient::requestEndpoints() const
{
    QStringList result;
    if (!m_baseUrl.isEmpty())
        result << m_baseUrl;
    for (const QString &url : m_baseUrls) {
        if (!url.isEmpty() && !result.contains(url))
            result << url;
    }
    return result;
}

void CloudMusicClient::setProfileDataRoot(const QString &path)
{
    const QString normalized = QDir::cleanPath(path.trimmed());
    if (m_profileDataRoot == normalized)
        return;
    ++m_contextGeneration;
    ++m_searchGeneration;
    m_readyQueue.clear();
    m_initializeInFlight = false;
    m_communityListInFlight = false;
    m_profileDataRoot = normalized;
    setReady(false);
    // Load the new profile identity transactionally. loadIdentity() emits at
    // most one auth/identity notification after the final values are known; it
    // never exposes a transient cleared identity to QML.
    loadIdentity();
}

QString CloudMusicClient::authFilePath() const
{
    if (m_profileDataRoot.isEmpty())
        return {};
    return QDir(m_profileDataRoot).filePath(QStringLiteral("cloud-auth.json"));
}

QString CloudMusicClient::protectToken(const QString &token) const
{
#ifdef Q_OS_WIN
    const QByteArray utf8 = token.toUtf8();
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(utf8.constData()));
    input.cbData = static_cast<DWORD>(utf8.size());
    DATA_BLOB output{};
    if (CryptProtectData(&input, L"EvolveMusic Cloud Session", nullptr, nullptr, nullptr,
                         CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        const QByteArray bytes(reinterpret_cast<const char *>(output.pbData), static_cast<int>(output.cbData));
        LocalFree(output.pbData);
        return QStringLiteral("dpapi:") + QString::fromLatin1(bytes.toBase64());
    }
#endif
    return QStringLiteral("plain:") + QString::fromLatin1(token.toUtf8().toBase64());
}

QString CloudMusicClient::unprotectToken(const QString &value) const
{
    if (value.startsWith(QStringLiteral("dpapi:"))) {
#ifdef Q_OS_WIN
        const QByteArray encrypted = QByteArray::fromBase64(value.mid(6).toLatin1());
        DATA_BLOB input{};
        input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(encrypted.constData()));
        input.cbData = static_cast<DWORD>(encrypted.size());
        DATA_BLOB output{};
        if (CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                               CRYPTPROTECT_UI_FORBIDDEN, &output)) {
            const QString token = QString::fromUtf8(reinterpret_cast<const char *>(output.pbData), static_cast<int>(output.cbData));
            LocalFree(output.pbData);
            return token;
        }
#endif
        return {};
    }
    if (value.startsWith(QStringLiteral("plain:")))
        return QString::fromUtf8(QByteArray::fromBase64(value.mid(6).toLatin1()));
    return value;
}

void CloudMusicClient::loadIdentity()
{
    QString nextUserId;
    QString nextUsername;
    QString nextEmail;
    QString nextToken;

    const QString path = authFilePath();
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
            if (document.isObject()) {
                const QJsonObject object = document.object();
                nextUserId = object.value(QStringLiteral("userId")).toString().trimmed();
                nextUsername = object.value(QStringLiteral("username")).toString().trimmed();
                nextEmail = object.value(QStringLiteral("email")).toString().trimmed();
                const QString protectedToken = object.value(QStringLiteral("protectedToken")).toString();
                if (!protectedToken.isEmpty())
                    nextToken = unprotectToken(protectedToken).trimmed();
                else
                    nextToken = object.value(QStringLiteral("token")).toString().trimmed(); // v0.13 migration
            }
        }
    }

    const bool identityDidChange = m_userId != nextUserId;
    const bool authDidChange = identityDidChange
        || m_username != nextUsername
        || m_email != nextEmail
        || m_token != nextToken;

    m_userId = nextUserId;
    m_username = nextUsername;
    m_email = nextEmail;
    m_token = nextToken;

    if (identityDidChange)
        emit identityChanged();
    if (authDidChange)
        emit authChanged();
}

bool CloudMusicClient::saveIdentity() const
{
    const QString path = authFilePath();
    if (path.isEmpty() || m_userId.isEmpty() || m_token.isEmpty())
        return false;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QJsonObject object{
        {QStringLiteral("userId"), m_userId},
        {QStringLiteral("username"), m_username},
        {QStringLiteral("email"), m_email},
        {QStringLiteral("protectedToken"), protectToken(m_token)}
    };
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (file.write(data) != data.size())
        return false;
    return file.commit();
}

void CloudMusicClient::clearIdentity()
{
    const bool hadIdentity = !m_userId.isEmpty();
    const bool hadAuth = hadIdentity || !m_username.isEmpty() || !m_token.isEmpty();
    m_userId.clear();
    m_username.clear();
    m_email.clear();
    m_token.clear();
    m_initializeInFlight = false;
    setReady(false);
    const QString path = authFilePath();
    if (!path.isEmpty())
        QFile::remove(path);
    if (hadIdentity)
        emit identityChanged();
    if (hadAuth)
        emit authChanged();
}

void CloudMusicClient::setReady(bool ready)
{
    if (m_ready == ready)
        return;
    m_ready = ready;
    emit readyChanged();
}

void CloudMusicClient::setAuthBusy(bool busy)
{
    if (m_authBusy == busy)
        return;
    m_authBusy = busy;
    emit authBusyChanged();
}

void CloudMusicClient::setAuthError(const QString &message)
{
    if (m_authError == message)
        return;
    m_authError = message;
    emit authErrorChanged();
}

void CloudMusicClient::applyAuthResponse(const QVariantMap &response)
{
    const QVariantMap user = response.value(QStringLiteral("user")).toMap();
    m_userId = user.value(QStringLiteral("id")).toString().trimmed();
    m_username = user.value(QStringLiteral("username")).toString().trimmed();
    m_email = user.value(QStringLiteral("email")).toString().trimmed();
    const QString token = response.value(QStringLiteral("token")).toString().trimmed();
    if (!token.isEmpty())
        m_token = token;
    if (m_userId.isEmpty() || m_token.isEmpty()) {
        setAuthError(QStringLiteral("云端没有返回有效的账号会话"));
        return;
    }
    saveIdentity();
    setReady(true);
    setAuthError({});
    emit identityChanged();
    emit authChanged();
    flushReadyQueue();
    loadState();
    loadHome();
    loadCustomPlaylists();
}

QNetworkReply *CloudMusicClient::requestJson(const QByteArray &method,
                                             const QString &path,
                                             const QUrlQuery &query,
                                             const QVariantMap &body,
                                             bool authenticated,
                                             JsonSuccess success,
                                             Failure failure)
{
    const QStringList endpoints = requestEndpoints();
    if (endpoints.isEmpty()) {
        if (failure) failure(QStringLiteral("Evolve Cloud 地址为空"));
        return nullptr;
    }
    return requestJsonAttempt(method, path, query, body, authenticated, endpoints, 0,
                              m_contextGeneration, std::move(success), std::move(failure));
}

QNetworkReply *CloudMusicClient::requestJsonAttempt(const QByteArray &method,
                                                    const QString &path,
                                                    const QUrlQuery &query,
                                                    const QVariantMap &body,
                                                    bool authenticated,
                                                    const QStringList &endpoints,
                                                    int endpointIndex,
                                                    quint64 requestGeneration,
                                                    JsonSuccess success,
                                                    Failure failure)
{
    if (endpointIndex < 0 || endpointIndex >= endpoints.size()) {
        if (failure) failure(QStringLiteral("两个 Evolve Cloud 节点都不可用"));
        return nullptr;
    }
    if (authenticated && m_token.isEmpty()) {
        if (failure) failure(QStringLiteral("请先登录 EvolveMusic 账号"));
        return nullptr;
    }

    const QString endpoint = endpoints.at(endpointIndex);
    QUrl url(endpoint + path);
    url.setQuery(query);
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EvolveMusic/0.18.1"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("X-Evolve-Endpoint", endpoint.toUtf8());
    if (authenticated)
        request.setRawHeader("Authorization", QByteArray("Bearer ") + m_token.toUtf8());
    request.setTransferTimeout(m_baseUrls.size() > 1 ? 9000 : 12000);

    const QByteArray payload = body.isEmpty() ? QByteArray()
        : QJsonDocument(QJsonObject::fromVariantMap(body)).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = nullptr;
    if (method == "GET") reply = m_network.get(request);
    else if (method == "POST") reply = m_network.post(request, payload);
    else if (method == "PUT") reply = m_network.put(request, payload);
    else if (method == "DELETE") reply = m_network.sendCustomRequest(request, QByteArray("DELETE"), payload);
    else {
        if (failure) failure(QStringLiteral("不支持的 Evolve Cloud API 方法"));
        return nullptr;
    }

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, method, path, query, body, authenticated, endpoints, endpointIndex,
             requestGeneration, success = std::move(success), failure = std::move(failure)]() mutable {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkMessage = reply->errorString();
        reply->deleteLater();
        if (requestGeneration != m_contextGeneration)
            return;

        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
        const QVariantMap object = document.isObject() ? document.object().toVariantMap() : QVariantMap{};
        const bool responseOk = networkError == QNetworkReply::NoError
            && status >= 200 && status < 300
            && object.value(QStringLiteral("ok"), true).toBool();

        if (responseOk) {
            setActiveEndpoint(endpoints.at(endpointIndex));
            if (success) success(object);
            return;
        }

        // Qt may report an HTTP 4xx/5xx response as a QNetworkReply error.  A
        // meaningful application 4xx (for example invalid password or username
        // already taken) must NOT be mistaken for a dead cloud node.  Only retry
        // transport failures (no HTTP status) and endpoint/server failures.
        const bool transportFailure = networkError != QNetworkReply::NoError && status == 0;
        const bool retryableStatus = status == 403 || status == 404 || status == 408
            || status == 429 || status >= 500;
        const bool retryable = transportFailure || retryableStatus;
        if (retryable && endpointIndex + 1 < endpoints.size()) {
            requestJsonAttempt(method, path, query, body, authenticated, endpoints, endpointIndex + 1,
                               requestGeneration, std::move(success), std::move(failure));
            return;
        }

        // Prefer the JSON error returned by our Worker. This keeps useful errors
        // such as "密码长度需为 8-128 位" instead of Qt's vague
        // "Error transferring ... server replied" string.
        QString message = object.value(QStringLiteral("message")).toString().trimmed();
        if (message.isEmpty()) message = object.value(QStringLiteral("error")).toString().trimmed();
        if (message.isEmpty() && !bytes.trimmed().isEmpty() && parseError.error == QJsonParseError::NoError)
            message = QString::fromUtf8(bytes).trimmed();
        if (message.isEmpty()) message = networkMessage.trimmed();
        if (message.isEmpty()) message = QStringLiteral("Evolve Cloud 请求失败");
        if (message.startsWith(QStringLiteral("Error transferring"), Qt::CaseInsensitive)) {
            message = QStringLiteral("云端节点请求失败（HTTP %1）：%2")
                .arg(status > 0 ? QString::number(status) : QStringLiteral("network"),
                     endpoints.at(endpointIndex));
        }
        if (status == 401 && authenticated) {
            // A rejected persisted token should return the UI to the login gate.
            clearIdentity();
            message = QStringLiteral("登录已失效，请重新登录");
        }
        if (failure) failure(message);
    });
    return reply;
}

void CloudMusicClient::runWhenReady(std::function<void()> action)
{
    if (!action)
        return;
    if (m_ready) {
        action();
        return;
    }
    if (m_readyQueue.size() < 24)
        m_readyQueue.append(std::move(action));
    initialize();
}

void CloudMusicClient::flushReadyQueue()
{
    if (!m_ready || m_readyQueue.isEmpty())
        return;
    auto pending = std::move(m_readyQueue);
    m_readyQueue.clear();
    for (auto &action : pending)
        if (action) action();
}

void CloudMusicClient::initialize()
{
    if (m_baseUrls.isEmpty() && m_baseUrl.isEmpty())
        return;

    // Cloud bootstrap is asynchronous. During loadIdentity()/authChanged, QML
    // and AppController may ask for community/social data. Those requests call
    // runWhenReady(), which used to call initialize() again synchronously and
    // recurse until "Maximum call stack size exceeded". Keep one bootstrap in
    // flight per endpoint/profile generation and let runWhenReady() only queue.
    if (m_initializeInFlight)
        return;
    m_initializeInFlight = true;
    qInfo() << "Cloud bootstrap starting; endpoint:" << m_baseUrl
            << "profileRoot:" << m_profileDataRoot;

    loadIdentity();
    if (m_token.isEmpty()) {
        m_initializeInFlight = false;
        setReady(false);
        qInfo() << "Cloud bootstrap paused: no persisted account session.";
        return;
    }

    const quint64 bootstrapGeneration = m_contextGeneration;
    QNetworkReply *reply = requestJson(QByteArray("GET"), QStringLiteral("/v1/auth/me"), {}, {}, true,
        [this, bootstrapGeneration](const QVariantMap &response) {
            if (bootstrapGeneration != m_contextGeneration)
                return;
            m_initializeInFlight = false;
            qInfo() << "Cloud bootstrap auth/me succeeded.";
            const QVariantMap user = response.value(QStringLiteral("user")).toMap();
            const QString nextUserId = user.value(QStringLiteral("id")).toString().trimmed();
            const QString nextUsername = user.value(QStringLiteral("username")).toString().trimmed();
            const QString nextEmail = user.value(QStringLiteral("email")).toString().trimmed();
            const bool identityDidChange = m_userId != nextUserId;
            const bool authDidChange = identityDidChange || m_username != nextUsername || m_email != nextEmail;
            m_userId = nextUserId;
            m_username = nextUsername;
            m_email = nextEmail;
            saveIdentity();
            setReady(true);
            setAuthError({});
            if (identityDidChange) emit identityChanged();
            if (authDidChange) emit authChanged();
            flushReadyQueue();
            loadState();
            loadHome();
            loadCustomPlaylists();
            loadProfile();
            loadFollowing();
            loadCommunityPlaylists();
            loadTogetherInvites();
        },
        [this, bootstrapGeneration](const QString &message) {
            if (bootstrapGeneration != m_contextGeneration)
                return;
            m_initializeInFlight = false;
            qWarning() << "Cloud bootstrap auth/me failed:" << message;
            setReady(false);
            setAuthError(message);
        });
    if (!reply)
        m_initializeInFlight = false;
}

void CloudMusicClient::login(const QString &identifier, const QString &password)
{
    if (m_authBusy) return;
    setAuthBusy(true);
    setAuthError({});
    requestJson(QByteArray("POST"), QStringLiteral("/v1/auth/login"), {},
                {{QStringLiteral("identifier"), identifier}, {QStringLiteral("password"), password}},
                false,
                [this](const QVariantMap &response) {
                    setAuthBusy(false);
                    applyAuthResponse(response);
                    emit toastRequested(QStringLiteral("欢迎回来，") + m_username);
                },
                [this](const QString &message) {
                    setAuthBusy(false);
                    setAuthError(message);
                });
}

void CloudMusicClient::registerAccount(const QString &username,
                                       const QString &password,
                                       const QString &email,
                                       const QString &emailCode,
                                       bool acceptTerms)
{
    if (m_authBusy) return;
    setAuthBusy(true);
    setAuthError({});
    requestJson(QByteArray("POST"), QStringLiteral("/v1/auth/register"), {},
                {{QStringLiteral("username"), username},
                 {QStringLiteral("password"), password},
                 {QStringLiteral("email"), email},
                 {QStringLiteral("emailCode"), emailCode},
                 {QStringLiteral("acceptTerms"), acceptTerms}},
                false,
                [this](const QVariantMap &response) {
                    setAuthBusy(false);
                    applyAuthResponse(response);
                    emit toastRequested(QStringLiteral("账号创建成功，欢迎使用 EvolveMusic"));
                },
                [this](const QString &message) {
                    setAuthBusy(false);
                    setAuthError(message);
                });
}

void CloudMusicClient::sendRegistrationEmailCode(const QString &email)
{
    if (m_authBusy) return;
    setAuthBusy(true);
    setAuthError({});
    requestJson(QByteArray("POST"), QStringLiteral("/v1/auth/email/send-code"), {},
                {{QStringLiteral("email"), email}}, false,
                [this](const QVariantMap &response) {
                    setAuthBusy(false);
                    const int cooldown = qMax(1, response.value(QStringLiteral("cooldown"), 60).toInt());
                    emit emailCodeSent(QStringLiteral("register"), cooldown);
                    emit toastRequested(QStringLiteral("验证码已发送，请检查邮箱"));
                },
                [this](const QString &message) { setAuthBusy(false); setAuthError(message); });
}

void CloudMusicClient::sendPasswordResetCode(const QString &email)
{
    if (m_authBusy) return;
    setAuthBusy(true);
    setAuthError({});
    requestJson(QByteArray("POST"), QStringLiteral("/v1/auth/password/send-code"), {},
                {{QStringLiteral("email"), email}}, false,
                [this](const QVariantMap &response) {
                    setAuthBusy(false);
                    const int cooldown = qMax(1, response.value(QStringLiteral("cooldown"), 60).toInt());
                    emit emailCodeSent(QStringLiteral("password_reset"), cooldown);
                    emit toastRequested(QStringLiteral("如果该邮箱已绑定账号，验证码会发送到邮箱"));
                },
                [this](const QString &message) { setAuthBusy(false); setAuthError(message); });
}

void CloudMusicClient::resetPassword(const QString &email,
                                     const QString &code,
                                     const QString &newPassword)
{
    if (m_authBusy) return;
    setAuthBusy(true);
    setAuthError({});
    requestJson(QByteArray("POST"), QStringLiteral("/v1/auth/password/reset"), {},
                {{QStringLiteral("email"), email}, {QStringLiteral("code"), code},
                 {QStringLiteral("newPassword"), newPassword}}, false,
                [this](const QVariantMap &) {
                    setAuthBusy(false);
                    emit passwordResetCompleted();
                    emit toastRequested(QStringLiteral("密码已重置，请使用新密码登录"));
                },
                [this](const QString &message) { setAuthBusy(false); setAuthError(message); });
}

void CloudMusicClient::logout()
{
    if (m_token.isEmpty()) {
        clearIdentity();
        return;
    }
    requestJson(QByteArray("POST"), QStringLiteral("/v1/auth/logout"), {}, {}, true,
                [this](const QVariantMap &) {
                    clearIdentity();
                    emit toastRequested(QStringLiteral("已退出 EvolveMusic 账号"));
                },
                [this](const QString &) {
                    clearIdentity();
                });
}

void CloudMusicClient::loadHome()
{
    if (!m_ready) {
        runWhenReady([this] { loadHome(); });
        return;
    }

    if (m_token.isEmpty())
        return;

    requestJson(
        QByteArray("GET"),
        QStringLiteral("/v1/home"),
        {},
        {},
        true,
        [this](const QVariantMap &response) {
            emit homeReady(
                response.value(
                    QStringLiteral("playlists"))
                    .toList(),
                response.value(
                    QStringLiteral("tracks"))
                    .toList());
        },
        [this](const QString &message) {
            emit errorOccurred(
                QStringLiteral("云端首页加载失败：")
                + message);
        });
}

void CloudMusicClient::loadRoam(const QStringList &excludeIds,
                                int limit,
                                const QVariantMap &currentTrack)
{
    if (!m_ready) {
        runWhenReady([this, excludeIds, limit, currentTrack] {
            loadRoam(excludeIds, limit, currentTrack);
        });
        return;
    }

    QUrlQuery query;
    query.addQueryItem(
        QStringLiteral("limit"),
        QString::number(qBound(6, limit, 30)));

    if (!excludeIds.isEmpty()) {
        query.addQueryItem(
            QStringLiteral("exclude"),
            excludeIds.mid(0, 48).join(QLatin1Char(',')));
    }

    const QString currentId =
        currentTrack.value(QStringLiteral("sourceId")).toString();
    const QString currentArtist =
        currentTrack.value(QStringLiteral("artist")).toString();
    const QString currentTitle =
        currentTrack.value(QStringLiteral("title")).toString();

    if (!currentId.isEmpty())
        query.addQueryItem(QStringLiteral("currentId"), currentId);
    if (!currentArtist.isEmpty())
        query.addQueryItem(QStringLiteral("artist"), currentArtist.left(120));
    if (!currentTitle.isEmpty())
        query.addQueryItem(QStringLiteral("title"), currentTitle.left(160));

    requestJson(
        QByteArray("GET"),
        QStringLiteral("/v1/roam"),
        query,
        {},
        true,
        [this](const QVariantMap &response) {
            emit roamReady(
                response.value(QStringLiteral("tracks")).toList(),
                response.value(QStringLiteral("reason"),
                               QStringLiteral("正在根据你的喜好持续推荐"))
                    .toString());
        },
        [this](const QString &message) {
            emit errorOccurred(
                QStringLiteral("智能漫游加载失败：") + message);
        });
}

void CloudMusicClient::cancelPendingSearch()
{
    ++m_searchGeneration;
}

void CloudMusicClient::search(const QString &keywords,
                              int limit)
{
    const QString queryText =
        keywords.trimmed().simplified();

    if (queryText.isEmpty())
        return;

    if (!m_ready) {
        runWhenReady([this, queryText, limit] {
            search(queryText, limit);
        });
        return;
    }

    const quint64 searchGeneration = ++m_searchGeneration;

    QUrlQuery query;
    query.addQueryItem(
        QStringLiteral("q"),
        queryText);
    query.addQueryItem(
        QStringLiteral("limit"),
        QString::number(limit));

    requestJson(
        QByteArray("GET"),
        QStringLiteral("/v1/search"),
        query,
        {},
        true,
        [this, searchGeneration](const QVariantMap &response) {
            if (searchGeneration != m_searchGeneration)
                return;

            emit searchReady(
                response.value(
                    QStringLiteral("tracks"))
                    .toList());
        },
        [this, searchGeneration](const QString &message) {
            if (searchGeneration != m_searchGeneration)
                return;

            emit errorOccurred(
                QStringLiteral("云端搜索失败：")
                + message);
        });
}

void CloudMusicClient::loadPlaylist(
    const QString &providerId,
    const QString &sourceId)
{
    if (!m_ready) {
        runWhenReady([this, providerId, sourceId] {
            loadPlaylist(providerId, sourceId);
        });
        return;
    }

    QUrlQuery query;
    query.addQueryItem(
        QStringLiteral("provider"),
        providerId);
    query.addQueryItem(
        QStringLiteral("id"),
        sourceId);

    requestJson(
        QByteArray("GET"),
        QStringLiteral("/v1/playlist"),
        query,
        {},
        true,
        [this](const QVariantMap &response) {
            emit playlistReady(
                response.value(
                    QStringLiteral("playlist"))
                    .toMap(),
                response.value(
                    QStringLiteral("tracks"))
                    .toList());
        },
        [this](const QString &message) {
            emit errorOccurred(
                QStringLiteral("云端歌单加载失败：")
                + message);
        });
}

void CloudMusicClient::loadLyrics(const QVariantMap &track)
{
    const QString trackId =
        track.value(
            QStringLiteral("id"))
            .toString();

    const QString providerId =
        track.value(
            QStringLiteral("providerId"),
            QStringLiteral("netease"))
            .toString();

    const QString sourceId =
        track.value(
            QStringLiteral("sourceId"))
            .toString();

    if (trackId.isEmpty()
        || sourceId.isEmpty())
        return;

    if (!m_ready) {
        runWhenReady([this, track] { loadLyrics(track); });
        return;
    }

    QUrlQuery query;
    query.addQueryItem(
        QStringLiteral("provider"),
        providerId);
    query.addQueryItem(
        QStringLiteral("id"),
        sourceId);

    requestJson(
        QByteArray("GET"),
        QStringLiteral("/v1/lyrics"),
        query,
        {},
        true,
        [this, trackId](
            const QVariantMap &response) {
            emit lyricsReady(
                trackId,
                response.value(
                    QStringLiteral("lyric"))
                    .toString(),
                response.value(
                    QStringLiteral("translatedLyric"))
                    .toString());
        },
        [this](const QString &message) {
            emit errorOccurred(
                QStringLiteral("云端歌词加载失败：")
                + message);
        });
}


QString CloudMusicClient::audioCacheDirectory() const
{
    QString root = qEnvironmentVariable("EVOLVE_MUSIC_CACHE_DIR").trimmed();

    if (root.isEmpty())
        root = m_profileDataRoot;

    if (root.isEmpty())
        root = QDir(QDir::tempPath()).filePath(QStringLiteral("EvolveMusic"));

    const QString path = QDir(root).filePath(QStringLiteral("audio-cache"));
    QDir().mkpath(path);
    return path;
}

QString CloudMusicClient::audioCacheStem(
    const QString &trackId,
    const QString &quality) const
{
    const QByteArray key =
        trackId.toUtf8()
        + QByteArrayLiteral("|")
        + quality.toUtf8();

    return QString::fromLatin1(
        QCryptographicHash::hash(
            key,
            QCryptographicHash::Sha256)
            .toHex()
            .left(32));
}

QString CloudMusicClient::cachedAudioFile(
    const QString &trackId,
    const QString &quality) const
{
    const QDir dir(
        audioCacheDirectory());

    const QString stem =
        audioCacheStem(
            trackId,
            quality);

    const QStringList suffixes{
        QStringLiteral("mp3"),
        QStringLiteral("flac"),
        QStringLiteral("m4a"),
        QStringLiteral("aac"),
        QStringLiteral("ogg"),
        QStringLiteral("media")
    };

    for (const QString &suffix :
         suffixes) {
        const QString path =
            dir.filePath(
                stem
                + QLatin1Char('.')
                + suffix);

        const QFileInfo info(path);

        if (info.exists()
            && info.isFile()
            && info.size() > 32 * 1024) {
            return path;
        }
    }

    return {};
}

void CloudMusicClient::pruneAudioCache()
{
    QDir dir(
        audioCacheDirectory());

    if (!dir.exists())
        return;

    QFileInfoList files =
        dir.entryInfoList(
            QDir::Files
            | QDir::NoDotAndDotDot,
            QDir::Time);

    constexpr qint64 maxBytes =
        768LL * 1024LL * 1024LL;

    qint64 total = 0;

    for (const QFileInfo &info :
         std::as_const(files)) {
        total += info.size();
    }

    if (total <= maxBytes)
        return;

    // entryInfoList(QDir::Time) is newest first.
    for (auto it = files.crbegin();
         it != files.crend()
         && total > maxBytes;
         ++it) {
        const qint64 size =
            it->size();

        if (QFile::remove(
                it->absoluteFilePath())) {
            total -= size;
        }
    }
}

void CloudMusicClient::prepareCachedStream(
    const QVariantMap &track,
    const QString &quality,
    const QString &remoteUrl,
    const QString &providerName,
    const QString &access,
    const QString &fallbackUrl)
{
    const QString trackId = track.value(QStringLiteral("id")).toString();

    if (trackId.isEmpty() || remoteUrl.trimmed().isEmpty()) {
        emit streamFailed(trackId, QStringLiteral("云端播放缺少有效的音频地址"));
        return;
    }

    // A complete local cache can start instantly and is still the cheapest path.
    const QString cached = cachedAudioFile(trackId, quality);
    if (!cached.isEmpty()) {
        emit streamReady(
            trackId,
            QUrl::fromLocalFile(cached).toString(QUrl::FullyEncoded),
            providerName + QStringLiteral(" · 本地缓存"),
            access,
            {});
        return;
    }

    const QUrl primary(remoteUrl.trimmed());
    if (!primary.isValid()
        || (primary.scheme() != QStringLiteral("https")
            && primary.scheme() != QStringLiteral("http"))) {
        emit streamFailed(trackId, QStringLiteral("云端返回了无效的播放 URL"));
        return;
    }

    QStringList alternatives;
    const QString alternate = fallbackUrl.trimmed();
    if (!alternate.isEmpty() && alternate != remoteUrl) {
        const QUrl fallback(alternate);
        if (fallback.isValid()
            && (fallback.scheme() == QStringLiteral("https")
                || fallback.scheme() == QStringLiteral("http"))) {
            alternatives.append(alternate);
        }
    }

    // v0.16.3: do not block playback on a separate 64 KiB HTTP probe. The old
    // probe added one full network round trip before QMediaPlayer was even
    // allowed to connect. PlayerController already has decoder/network error
    // handling and candidate failover, so hand the URL to the media backend
    // immediately and let it start buffering at once.
    emit streamReady(
        trackId,
        remoteUrl.trimmed(),
        providerName + QStringLiteral(" · 极速直连"),
        access,
        alternatives);
}

void CloudMusicClient::requestStreamTicket(
    const QVariantMap &track,
    const QString &quality)
{
    const QString trackId = track.value(QStringLiteral("id")).toString();
    const QString providerId = track.value(
        QStringLiteral("providerId"), QStringLiteral("netease")).toString();
    const QString sourceId = track.value(QStringLiteral("sourceId")).toString();

    if (trackId.isEmpty() || sourceId.isEmpty()) {
        emit errorOccurred(QStringLiteral("云端播放缺少歌曲标识"));
        return;
    }

    if (!m_ready) {
        runWhenReady([this, track, quality] { requestStreamTicket(track, quality); });
        return;
    }

    const QStringList endpoints = requestEndpoints();
    if (endpoints.isEmpty()) {
        emit streamFailed(trackId, QStringLiteral("Evolve Cloud 地址为空"));
        return;
    }

    // Playback is latency-sensitive. Unlike ordinary account/state requests,
    // do not wait for cloud node A to time out before trying node B. Send the
    // same authenticated stream-ticket request to every configured node and
    // accept the first valid response. The nodes share the same D1/session data.
    struct StreamTicketRace {
        int pending = 0;
        bool done = false;
        quint64 generation = 0;
        QStringList errors;
    };
    const auto race = QSharedPointer<StreamTicketRace>::create();
    race->pending = endpoints.size();
    race->generation = m_contextGeneration;

    const QVariantMap body{
        {QStringLiteral("trackId"), trackId},
        {QStringLiteral("providerId"), providerId},
        {QStringLiteral("sourceId"), sourceId},
        {QStringLiteral("quality"), quality},
        {QStringLiteral("title"), track.value(QStringLiteral("title")).toString()},
        {QStringLiteral("artist"), track.value(QStringLiteral("artist")).toString()},
        {QStringLiteral("album"), track.value(QStringLiteral("album")).toString()}
    };
    const QByteArray payload = QJsonDocument(
        QJsonObject::fromVariantMap(body)).toJson(QJsonDocument::Compact);

    for (const QString &endpoint : endpoints) {
        QUrl url(endpoint + QStringLiteral("/v1/stream-ticket"));
        QNetworkRequest request{url};
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EvolveMusic/0.18.1"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Accept", "application/json");
        request.setRawHeader("X-Evolve-Endpoint", endpoint.toUtf8());
        request.setRawHeader("Authorization", QByteArray("Bearer ") + m_token.toUtf8());
        request.setTransferTimeout(4800);

        QNetworkReply *reply = m_network.post(request, payload);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, endpoint, race, track, quality, trackId]() {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray bytes = reply->readAll();
            const QNetworkReply::NetworkError networkError = reply->error();
            const QString networkMessage = reply->errorString();
            reply->deleteLater();

            if (race->generation != m_contextGeneration || race->done)
                return;

            QJsonParseError parseError{};
            const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
            const QVariantMap response = document.isObject()
                ? document.object().toVariantMap() : QVariantMap{};
            const QString url = response.value(QStringLiteral("url")).toString().trimmed();
            const bool okResponse = networkError == QNetworkReply::NoError
                && status >= 200 && status < 300
                && response.value(QStringLiteral("ok"), true).toBool()
                && !url.isEmpty();

            if (okResponse) {
                race->done = true;
                setActiveEndpoint(endpoint);
                const QString directUrl = response.value(
                    QStringLiteral("directUrl")).toString().trimmed();
                prepareCachedStream(
                    track,
                    quality,
                    directUrl.isEmpty() ? url : directUrl,
                    response.value(QStringLiteral("providerName"),
                                   QStringLiteral("Cloudflare")).toString(),
                    response.value(QStringLiteral("access"),
                                   QStringLiteral("cloud")).toString(),
                    directUrl.isEmpty() ? QString() : url);
                return;
            }

            QString message = response.value(QStringLiteral("message")).toString().trimmed();
            if (message.isEmpty())
                message = response.value(QStringLiteral("error")).toString().trimmed();
            if (message.isEmpty())
                message = networkMessage.trimmed();
            if (message.isEmpty())
                message = status > 0
                    ? QStringLiteral("HTTP %1").arg(status)
                    : QStringLiteral("网络请求失败");
            race->errors << QStringLiteral("%1: %2").arg(endpoint, message);

            --race->pending;
            if (race->pending <= 0 && !race->done) {
                race->done = true;
                emit streamFailed(
                    trackId,
                    QStringLiteral("双云端并发解析失败：")
                        + race->errors.join(QStringLiteral("；")));
            }
        });
    }
}

void CloudMusicClient::loadState()
{
    if (!m_ready) {
        runWhenReady([this] { loadState(); });
        return;
    }

    requestJson(
        QByteArray("GET"),
        QStringLiteral("/v1/me/state"),
        {},
        {},
        true,
        [this](const QVariantMap &response) {
            const QVariantMap state =
                response.value(
                    QStringLiteral("state"))
                    .toMap();

            emit stateReady(
                state.value(
                    QStringLiteral("favorites"))
                    .toList(),
                state.value(
                    QStringLiteral("history"))
                    .toList(),
                state.value(
                    QStringLiteral("settings"))
                    .toMap());
        },
        [this](const QString &message) {
            emit errorOccurred(
                QStringLiteral("云端用户数据加载失败：")
                + message);
        });
}

void CloudMusicClient::saveState(
    const QVariantList &favorites,
    const QVariantList &history,
    const QVariantMap &settings)
{
    if (!m_ready) {
        runWhenReady([this, favorites, history, settings] {
            saveState(favorites, history, settings);
        });
        return;
    }

    requestJson(
        QByteArray("PUT"),
        QStringLiteral("/v1/me/state"),
        {},
        {
            {QStringLiteral("favorites"),
             favorites},
            {QStringLiteral("history"),
             history},
            {QStringLiteral("settings"),
             settings}
        },
        true,
        [](const QVariantMap &) {},
        [this](const QString &message) {
            emit errorOccurred(
                QStringLiteral("云端用户数据保存失败：")
                + message);
        });
}

void CloudMusicClient::loadCustomPlaylists()
{
    if (!m_ready) {
        runWhenReady([this] { loadCustomPlaylists(); });
        return;
    }
    requestJson(QByteArray("GET"), QStringLiteral("/v1/me/playlists"), {}, {}, true,
        [this](const QVariantMap &response) {
            emit customPlaylistsReady(response.value(QStringLiteral("playlists")).toList());
        },
        [this](const QString &message) {
            emit errorOccurred(QStringLiteral("歌单同步失败：") + message);
        });
}

void CloudMusicClient::createCustomPlaylist(const QString &name)
{
    if (!m_ready) return;
    requestJson(QByteArray("POST"), QStringLiteral("/v1/me/playlists"), {},
        {{QStringLiteral("name"), name}}, true,
        [this](const QVariantMap &response) {
            emit customPlaylistSaved(response.value(QStringLiteral("playlist")).toMap());
        },
        [this](const QString &message) { emit toastRequested(QStringLiteral("创建歌单失败：") + message); });
}

void CloudMusicClient::updateCustomPlaylist(const QString &playlistId,
                                            const QString &name,
                                            const QVariantList &tracks,
                                            const QString &description)
{
    if (!m_ready || playlistId.trimmed().isEmpty()) return;
    const QString path = QStringLiteral("/v1/me/playlists/") + QString::fromLatin1(QUrl::toPercentEncoding(playlistId));
    requestJson(QByteArray("PUT"), path, {},
        {{QStringLiteral("name"), name}, {QStringLiteral("description"), description}, {QStringLiteral("tracks"), tracks}}, true,
        [this](const QVariantMap &response) {
            emit customPlaylistSaved(response.value(QStringLiteral("playlist")).toMap());
        },
        [this](const QString &message) { emit toastRequested(QStringLiteral("保存歌单失败：") + message); });
}

void CloudMusicClient::deleteCustomPlaylist(const QString &playlistId)
{
    if (!m_ready || playlistId.trimmed().isEmpty()) return;
    const QString path = QStringLiteral("/v1/me/playlists/") + QString::fromLatin1(QUrl::toPercentEncoding(playlistId));
    requestJson(QByteArray("DELETE"), path, {}, {}, true,
        [this, playlistId](const QVariantMap &) { emit customPlaylistDeleted(playlistId); },
        [this](const QString &message) { emit toastRequested(QStringLiteral("删除歌单失败：") + message); });
}


void CloudMusicClient::updateCustomPlaylistMetadata(const QString &playlistId,
                                                     const QString &name,
                                                     const QString &description,
                                                     const QString &coverData,
                                                     bool isPublic)
{
    if (!m_ready || playlistId.trimmed().isEmpty()) return;
    const QString path = QStringLiteral("/v1/me/playlists/")
        + QString::fromLatin1(QUrl::toPercentEncoding(playlistId));
    requestJson(QByteArray("PUT"), path, {},
        {
            {QStringLiteral("name"), name},
            {QStringLiteral("description"), description},
            {QStringLiteral("cover"), coverData},
            {QStringLiteral("isPublic"), isPublic}
        }, true,
        [this](const QVariantMap &response) {
            emit customPlaylistSaved(response.value(QStringLiteral("playlist")).toMap());
        },
        [this](const QString &message) {
            emit toastRequested(QStringLiteral("保存歌单信息失败：") + message);
        });
}

void CloudMusicClient::loadProfile()
{
    if (!m_ready) { runWhenReady([this] { loadProfile(); }); return; }
    requestJson(QByteArray("GET"), QStringLiteral("/v1/me/profile"), {}, {}, true,
        [this](const QVariantMap &response) {
            emit profileReady(response.value(QStringLiteral("profile")).toMap());
        },
        [this](const QString &message) {
            emit toastRequested(QStringLiteral("个人资料加载失败：") + message);
        });
}

void CloudMusicClient::updateProfile(const QString &displayName,
                                     const QString &bio,
                                     const QString &avatarData)
{
    if (!m_ready) return;
    QVariantMap body{
        {QStringLiteral("displayName"), displayName},
        {QStringLiteral("bio"), bio}
    };
    if (!avatarData.trimmed().isEmpty())
        body.insert(QStringLiteral("avatar"), avatarData);
    requestJson(QByteArray("PUT"), QStringLiteral("/v1/me/profile"), {}, body, true,
        [this](const QVariantMap &response) {
            emit profileSaved(response.value(QStringLiteral("profile")).toMap());
        },
        [this](const QString &message) {
            emit toastRequested(QStringLiteral("个人资料保存失败：") + message);
        });
}

void CloudMusicClient::searchUsers(const QString &queryText)
{
    if (!m_ready || queryText.trimmed().isEmpty()) return;
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), queryText.trimmed());
    requestJson(QByteArray("GET"), QStringLiteral("/v1/users/search"), query, {}, true,
        [this](const QVariantMap &response) {
            emit userSearchReady(response.value(QStringLiteral("users")).toList());
        },
        [this](const QString &message) {
            emit toastRequested(QStringLiteral("搜索用户失败：") + message);
        });
}

void CloudMusicClient::setFollowing(const QString &userId, bool follow)
{
    if (!m_ready || userId.trimmed().isEmpty()) return;
    const QString path = QStringLiteral("/v1/users/")
        + QString::fromLatin1(QUrl::toPercentEncoding(userId))
        + QStringLiteral("/follow");
    requestJson(follow ? QByteArray("POST") : QByteArray("DELETE"), path, {}, {}, true,
        [this](const QVariantMap &) {
            loadFollowing();
        },
        [this](const QString &message) {
            emit toastRequested(QStringLiteral("关注操作失败：") + message);
        });
}

void CloudMusicClient::loadFollowing()
{
    if (!m_ready) return;
    requestJson(QByteArray("GET"), QStringLiteral("/v1/me/following"), {}, {}, true,
        [this](const QVariantMap &response) {
            emit followingReady(response.value(QStringLiteral("users")).toList());
        },
        [this](const QString &message) {
            emit toastRequested(QStringLiteral("关注列表加载失败：") + message);
        });
}

void CloudMusicClient::loadCommunityPlaylists(int limit)
{
    if (!m_ready) {
        runWhenReady([this, limit] { loadCommunityPlaylists(limit); });
        return;
    }
    // Background refreshes and page-enter refreshes can fire close together.
    // Keep one list request in flight so a slow node never builds a queue of
    // identical requests.
    if (m_communityListInFlight)
        return;

    m_communityListInFlight = true;
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("limit"), QString::number(qBound(6, limit, 40)));
    requestJson(QByteArray("GET"), QStringLiteral("/v1/community/playlists"), query, {}, true,
        [this](const QVariantMap &response) {
            m_communityListInFlight = false;
            emit communityPlaylistsReady(response.value(QStringLiteral("playlists")).toList());
        },
        [this](const QString &message) {
            m_communityListInFlight = false;
            const QString lower = message.trimmed().toLower();
            // QNetworkReply reports transfer timeout/cancel as "Operation canceled".
            // Keep the last good community snapshot and let the next timer retry.
            if (lower.contains(QStringLiteral("operation canceled"))
                || lower.contains(QStringLiteral("operation cancelled"))
                || lower == QStringLiteral("canceled")
                || lower == QStringLiteral("cancelled")) {
                qWarning() << "Community refresh canceled; keeping stale snapshot.";
                return;
            }
            emit toastRequested(QStringLiteral("社区歌单加载失败：") + message);
        });
}

void CloudMusicClient::loadCommunityPlaylist(const QString &playlistId)
{
    if (!m_ready || playlistId.trimmed().isEmpty()) return;
    const QString path = QStringLiteral("/v1/community/playlists/")
        + QString::fromLatin1(QUrl::toPercentEncoding(playlistId));
    requestJson(QByteArray("GET"), path, {}, {}, true,
        [this](const QVariantMap &response) {
            emit communityPlaylistReady(response.value(QStringLiteral("playlist")).toMap(),
                                        response.value(QStringLiteral("tracks")).toList());
        },
        [this](const QString &message) {
            emit toastRequested(QStringLiteral("社区歌单加载失败：") + message);
        });
}

void CloudMusicClient::createTogetherRoom()
{
    if (!m_ready) return;
    requestJson(QByteArray("POST"), QStringLiteral("/v1/together/rooms"), {}, {}, true,
        [this](const QVariantMap &response) {
            emit togetherRoomReady(response.value(QStringLiteral("room")).toMap());
        },
        [this](const QString &message) {
            emit toastRequested(QStringLiteral("创建一起听房间失败：") + message);
        });
}

void CloudMusicClient::joinTogetherRoom(const QString &code)
{
    if (!m_ready || code.trimmed().isEmpty()) return;
    requestJson(QByteArray("POST"), QStringLiteral("/v1/together/join"), {},
        {{QStringLiteral("code"), code.trimmed().toUpper()}}, true,
        [this](const QVariantMap &response) {
            emit togetherRoomReady(response.value(QStringLiteral("room")).toMap());
        },
        [this](const QString &message) {
            emit toastRequested(QStringLiteral("加入一起听失败：") + message);
        });
}

void CloudMusicClient::loadTogetherRoom(const QString &code)
{
    if (!m_ready || code.trimmed().isEmpty()) return;
    const QString path = QStringLiteral("/v1/together/room/")
        + QString::fromLatin1(QUrl::toPercentEncoding(code.trimmed().toUpper()));
    const QString requestedCode = code.trimmed().toUpper();
    requestJson(QByteArray("GET"), path, {}, {}, true,
        [this](const QVariantMap &response) {
            emit togetherRoomReady(response.value(QStringLiteral("room")).toMap());
        },
        [this, requestedCode](const QString &message) {
            const QString lower = message.toLower();
            if (message.contains(QStringLiteral("房间不存在"))
                || message.contains(QStringLiteral("已过期"))
                || message.contains(QStringLiteral("不在房间"))
                || lower.contains(QStringLiteral("room_not_found"))
                || lower.contains(QStringLiteral("404"))) {
                emit togetherRoomUnavailable(requestedCode, message);
            }
        });
}

void CloudMusicClient::updateTogetherState(const QString &code,
                                           const QVariantMap &state)
{
    if (!m_ready || code.trimmed().isEmpty()) return;
    const QString path = QStringLiteral("/v1/together/room/")
        + QString::fromLatin1(QUrl::toPercentEncoding(code.trimmed().toUpper()))
        + QStringLiteral("/state");
    requestJson(QByteArray("PUT"), path, {}, {{QStringLiteral("state"), state}}, true,
        [](const QVariantMap &) {}, [](const QString &) {});
}

void CloudMusicClient::leaveTogetherRoom(const QString &code)
{
    if (!m_ready || code.trimmed().isEmpty()) { emit togetherLeft(); return; }
    const QString path = QStringLiteral("/v1/together/room/")
        + QString::fromLatin1(QUrl::toPercentEncoding(code.trimmed().toUpper()))
        + QStringLiteral("/leave");
    requestJson(QByteArray("POST"), path, {}, {}, true,
        [this](const QVariantMap &) { emit togetherLeft(); },
        [this](const QString &) { emit togetherLeft(); });
}

void CloudMusicClient::inviteTogetherUser(const QString &code,
                                          const QString &username)
{
    if (!m_ready || code.trimmed().isEmpty() || username.trimmed().isEmpty()) return;
    const QString path = QStringLiteral("/v1/together/room/")
        + QString::fromLatin1(QUrl::toPercentEncoding(code.trimmed().toUpper()))
        + QStringLiteral("/invite");
    requestJson(QByteArray("POST"), path, {},
        {{QStringLiteral("username"), username.trimmed()}}, true,
        [this](const QVariantMap &) {
            emit toastRequested(QStringLiteral("邀请已发送"));
        },
        [this](const QString &message) {
            emit toastRequested(QStringLiteral("邀请失败：") + message);
        });
}

void CloudMusicClient::loadTogetherInvites()
{
    if (!m_ready) return;
    requestJson(QByteArray("GET"), QStringLiteral("/v1/together/invites"), {}, {}, true,
        [this](const QVariantMap &response) {
            emit togetherInvitesReady(response.value(QStringLiteral("invites")).toList());
        },
        [this](const QString &) {});
}

void CloudMusicClient::heartbeatPresence()
{
    if (!m_ready || !loggedIn()) return;
    requestJson(QByteArray("POST"), QStringLiteral("/v1/presence"), {}, {}, true,
        [this](const QVariantMap &response) {
            emit presenceReady(response.value(QStringLiteral("onlineUsers")).toInt());
        },
        [this](const QString &) {});
}

void CloudMusicClient::dismissTogetherInvite(const QString &inviteId)
{
    if (!m_ready || inviteId.trimmed().isEmpty()) return;
    const QString path = QStringLiteral("/v1/together/invites/")
        + QString::fromLatin1(QUrl::toPercentEncoding(inviteId.trimmed()));
    requestJson(QByteArray("DELETE"), path, {}, {}, true,
        [this](const QVariantMap &) { loadTogetherInvites(); },
        [this](const QString &) {});
}


void CloudMusicClient::loadTogetherMessages(const QString &code, qint64 afterSequence)
{
    const QString cleanCode = code.trimmed().toUpper();
    if (cleanCode.isEmpty()) return;
    if (!m_ready) {
        runWhenReady([this, cleanCode, afterSequence] { loadTogetherMessages(cleanCode, afterSequence); });
        return;
    }
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("after"), QString::number(qMax<qint64>(0, afterSequence)));
    const QString path = QStringLiteral("/v1/together/room/")
        + QString::fromLatin1(QUrl::toPercentEncoding(cleanCode))
        + QStringLiteral("/messages");
    requestJson(QByteArray("GET"), path, query, {}, true,
        [this](const QVariantMap &response) {
            emit togetherMessagesReady(response.value(QStringLiteral("messages")).toList(),
                                       response.value(QStringLiteral("cursor")).toLongLong());
        },
        [this](const QString &message) {
            // Chat polling is best-effort; avoid replacing the whole app error panel
            // for a transient message refresh failure.
            qWarning() << "Together chat refresh failed:" << message;
        });
}

void CloudMusicClient::sendTogetherText(const QString &code, const QString &text)
{
    const QString cleanCode = code.trimmed().toUpper();
    const QString cleanText = text.trimmed().left(2000);
    if (cleanCode.isEmpty() || cleanText.isEmpty()) return;
    if (!m_ready) {
        runWhenReady([this, cleanCode, cleanText] { sendTogetherText(cleanCode, cleanText); });
        return;
    }
    const QString path = QStringLiteral("/v1/together/room/")
        + QString::fromLatin1(QUrl::toPercentEncoding(cleanCode))
        + QStringLiteral("/messages");
    requestJson(QByteArray("POST"), path, {},
                QVariantMap{{QStringLiteral("text"), cleanText}}, true,
        [this](const QVariantMap &) { emit togetherMessageSent(); },
        [this](const QString &message) { emit errorOccurred(QStringLiteral("房间消息发送失败：") + message); });
}

void CloudMusicClient::uploadTogetherAttachment(const QString &code,
                                                const QString &kind,
                                                const QString &fileName,
                                                const QString &mimeType,
                                                const QByteArray &bytes)
{
    const QString cleanCode = code.trimmed().toUpper();
    const QString cleanKind = kind == QStringLiteral("image") ? QStringLiteral("image") : QStringLiteral("file");
    if (cleanCode.isEmpty() || bytes.isEmpty()) return;
    if (!m_ready) {
        runWhenReady([this, cleanCode, cleanKind, fileName, mimeType, bytes] {
            uploadTogetherAttachment(cleanCode, cleanKind, fileName, mimeType, bytes);
        });
        return;
    }
    const QString endpoint = requestEndpoints().value(0);
    if (endpoint.isEmpty()) {
        emit errorOccurred(QStringLiteral("Evolve Cloud 地址为空"));
        return;
    }
    const QString path = QStringLiteral("/v1/together/room/")
        + QString::fromLatin1(QUrl::toPercentEncoding(cleanCode))
        + QStringLiteral("/attachments");
    QUrl url(endpoint + path);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("kind"), cleanKind);
    query.addQueryItem(QStringLiteral("name"), fileName.left(180));
    url.setQuery(query);
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EvolveMusic/0.18.1"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      mimeType.trimmed().isEmpty() ? QStringLiteral("application/octet-stream") : mimeType.trimmed());
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", QByteArray("Bearer ") + m_token.toUtf8());
    request.setTransferTimeout(30000);
    QNetworkReply *reply = m_network.post(request, bytes);
    const quint64 generation = m_contextGeneration;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, endpoint] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBytes = reply->readAll();
        const auto networkError = reply->error();
        const QString networkMessage = reply->errorString();
        reply->deleteLater();
        if (generation != m_contextGeneration) return;
        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(responseBytes, &parseError);
        const QVariantMap response = document.isObject() ? document.object().toVariantMap() : QVariantMap{};
        if (networkError == QNetworkReply::NoError && status >= 200 && status < 300
            && response.value(QStringLiteral("ok"), true).toBool()) {
            setActiveEndpoint(endpoint);
            emit togetherMessageSent();
            return;
        }
        QString message = response.value(QStringLiteral("message")).toString().trimmed();
        if (message.isEmpty()) message = response.value(QStringLiteral("error")).toString().trimmed();
        if (message.isEmpty()) message = networkMessage;
        emit errorOccurred(QStringLiteral("房间附件发送失败：") + message);
    });
}

void CloudMusicClient::downloadTogetherAttachment(const QString &code,
                                                  const QString &attachmentId)
{
    const QString cleanCode = code.trimmed().toUpper();
    const QString cleanId = attachmentId.trimmed();
    if (cleanCode.isEmpty() || cleanId.isEmpty()) return;
    if (!m_ready) {
        runWhenReady([this, cleanCode, cleanId] { downloadTogetherAttachment(cleanCode, cleanId); });
        return;
    }
    const QString endpoint = requestEndpoints().value(0);
    if (endpoint.isEmpty()) return;
    const QString path = QStringLiteral("/v1/together/room/")
        + QString::fromLatin1(QUrl::toPercentEncoding(cleanCode))
        + QStringLiteral("/attachments/")
        + QString::fromLatin1(QUrl::toPercentEncoding(cleanId));
    QNetworkRequest request{QUrl(endpoint + path)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EvolveMusic/0.18.1"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + m_token.toUtf8());
    request.setTransferTimeout(30000);
    QNetworkReply *reply = m_network.get(request);
    const quint64 generation = m_contextGeneration;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, cleanId] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->readAll();
        const auto networkError = reply->error();
        const QString message = reply->errorString();
        const QString fileName = QUrl::fromPercentEncoding(reply->rawHeader("X-Evolve-File-Name"));
        const QString mimeType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        reply->deleteLater();
        if (generation != m_contextGeneration) return;
        if (networkError == QNetworkReply::NoError && status >= 200 && status < 300) {
            emit togetherAttachmentReady(cleanId, bytes, fileName, mimeType);
            return;
        }
        qWarning() << "Together attachment download failed:" << cleanId << message;
    });
}

void CloudMusicClient::sendTogetherVoiceChunk(const QString &code,
                                              const QByteArray &compressedPcm)
{
    const QString cleanCode = code.trimmed().toUpper();
    if (!m_ready || cleanCode.isEmpty() || compressedPcm.isEmpty()) return;
    const QString endpoint = requestEndpoints().value(0);
    if (endpoint.isEmpty()) return;
    const QString path = QStringLiteral("/v1/together/room/")
        + QString::fromLatin1(QUrl::toPercentEncoding(cleanCode))
        + QStringLiteral("/voice");
    QNetworkRequest request{QUrl(endpoint + path)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EvolveMusic/0.18.1"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-evolve-pcm-zlib"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + m_token.toUtf8());
    request.setTransferTimeout(3500);
    QNetworkReply *reply = m_network.post(request, compressedPcm);
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void CloudMusicClient::loadTogetherVoiceChunks(const QString &code, qint64 afterSequence)
{
    const QString cleanCode = code.trimmed().toUpper();
    if (!m_ready || cleanCode.isEmpty()) return;
    const QString endpoint = requestEndpoints().value(0);
    if (endpoint.isEmpty()) return;
    const QString path = QStringLiteral("/v1/together/room/")
        + QString::fromLatin1(QUrl::toPercentEncoding(cleanCode))
        + QStringLiteral("/voice");
    QUrl url(endpoint + path);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("after"), QString::number(qMax<qint64>(0, afterSequence)));
    url.setQuery(query);
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EvolveMusic/0.18.1"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", QByteArray("Bearer ") + m_token.toUtf8());
    request.setTransferTimeout(2500);
    QNetworkReply *reply = m_network.get(request);
    const quint64 generation = m_contextGeneration;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->readAll();
        const auto networkError = reply->error();
        reply->deleteLater();
        if (generation != m_contextGeneration || networkError != QNetworkReply::NoError
            || status < 200 || status >= 300) return;
        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
        if (!document.isObject()) return;
        const QVariantMap response = document.object().toVariantMap();
        emit togetherVoiceChunksReady(response.value(QStringLiteral("chunks")).toList(),
                                      response.value(QStringLiteral("cursor")).toLongLong());
    });
}

void CloudMusicClient::uploadProviderSession(
    const QString &providerId,
    const QString &cookieHeader,
    const QString &userAgent,
    const QString &pageUrl)
{
    if (providerId != QStringLiteral("netease")
        || cookieHeader.trimmed().isEmpty()) {
        return;
    }

    if (!m_ready) {
        runWhenReady([this, providerId, cookieHeader, userAgent, pageUrl] {
            uploadProviderSession(providerId, cookieHeader, userAgent, pageUrl);
        });
        return;
    }

    requestJson(
        QByteArray("PUT"),
        QStringLiteral("/v1/me/provider-session/netease"),
        {},
        {
            {QStringLiteral("cookie"),
             cookieHeader},
            {QStringLiteral("userAgent"),
             userAgent},
            {QStringLiteral("pageUrl"),
             pageUrl}
        },
        true,
        [this, providerId](
            const QVariantMap &) {
            emit providerSessionStored(providerId);
            emit toastRequested(
                QStringLiteral(
                    "网易云会话已同步到当前 Cloudflare 用户"));
        },
        [this](const QString &message) {
            emit errorOccurred(
                QStringLiteral("网易云云端会话同步失败：")
                + message);
        });
}

void CloudMusicClient::removeProviderSession(
    const QString &providerId)
{
    if (providerId != QStringLiteral("netease"))
        return;

    if (!m_ready) {
        runWhenReady([this, providerId] { removeProviderSession(providerId); });
        return;
    }

    requestJson(
        QByteArray("DELETE"),
        QStringLiteral("/v1/me/provider-session/netease"),
        {},
        {},
        true,
        [this, providerId](
            const QVariantMap &) {
            emit providerSessionRemoved(providerId);
        },
        [this](const QString &message) {
            emit errorOccurred(
                QStringLiteral("云端网易会话删除失败：")
                + message);
        });
}
