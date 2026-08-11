#include "AccountManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrlQuery>
#include <QTimer>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

AccountManager::AccountManager(QObject *parent) : QObject(parent)
{
    m_qrPollTimer.setInterval(1600);
    m_qrPollTimer.setSingleShot(false);
    connect(&m_qrPollTimer, &QTimer::timeout, this, &AccountManager::pollNeteaseQrStatus);
}

void AccountManager::setProfileId(const QString &profileId)
{
    QString normalized = profileId.trimmed();

    normalized.replace(
        QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")),
        QStringLiteral("_"));

    if (normalized.isEmpty())
        normalized = QStringLiteral("default");

    if (normalized == m_profileId) {
        migrateLegacySessionsIfNeeded();
        return;
    }

    stopNeteaseQrLogin(true);

    m_switchingProfile = true;

    for (const QString &id : m_order) {
        if (auto *source = provider(id))
            source->logout();

        if (m_accounts.contains(id)) {
            m_accounts[id].loggedIn = false;
            m_accounts[id].displayName.clear();
            m_accounts[id].status =
                QStringLiteral("未登录 · 当前本地用户无会话");
        }
    }

    m_profileId = normalized;
    migrateLegacySessionsIfNeeded();

    m_switchingProfile = false;

    emit profileChanged();
    emit accountsChanged();

    QTimer::singleShot(
        0,
        this,
        &AccountManager::restoreSavedSessions);
}

QString AccountManager::profileDataRoot() const
{
    QString dataRoot =
        qEnvironmentVariable("EVOLVE_MUSIC_DATA_ROOT").trimmed();

    if (dataRoot.isEmpty()) {
        dataRoot =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }

    return QDir(dataRoot)
        .filePath(
            QStringLiteral("profiles/")
            + m_profileId);
}

QString AccountManager::legacySessionDataRoot() const
{
    QString dataRoot =
        qEnvironmentVariable("EVOLVE_MUSIC_DATA_ROOT").trimmed();

    if (dataRoot.isEmpty()) {
        dataRoot =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }

    return QDir(dataRoot)
        .filePath(QStringLiteral("sessions"));
}

void AccountManager::migrateLegacySessionsIfNeeded()
{
    if (m_profileId != QStringLiteral("default"))
        return;

    const QString oldRoot = legacySessionDataRoot();
    QDir oldDir(oldRoot);

    if (!oldDir.exists())
        return;

    const QString newRoot = sessionDataRoot();
    QDir().mkpath(newRoot);

    const QStringList files =
        oldDir.entryList(
            {QStringLiteral("*.session")},
            QDir::Files | QDir::Readable);

    for (const QString &name : files) {
        const QString source =
            oldDir.filePath(name);
        const QString target =
            QDir(newRoot).filePath(name);

        if (QFile::exists(target))
            continue;

        if (!QFile::rename(source, target))
            QFile::copy(source, target);
    }
}

void AccountManager::restoreSavedSessions()
{
    for (const QString &id : m_order) {
        const SessionSnapshot session =
            loadSession(id);

        if (session.isEmpty())
            continue;

        if (m_accounts.contains(id)) {
            m_accounts[id].status =
                QStringLiteral("正在恢复当前本地用户的登录状态…");
        }

        if (auto *source = provider(id)) {
            source->importWebSession(
                session.cookieHeader,
                session.userAgent,
                session.storageJson,
                session.pageUrl);
        }
    }

    emit accountsChanged();
}

void AccountManager::registerProvider(IMusicProvider *p)
{
    if (!p || p->providerId().isEmpty()) return;
    const QString id = p->providerId();

    AccountState state;
    state.provider = p;
    state.loggedIn = p->isAuthenticated();
    state.status = state.loggedIn ? QStringLiteral("已登录") : QStringLiteral("未登录");
    m_accounts[id] = state;
    if (!m_order.contains(id)) m_order << id;

    connect(p, &IMusicProvider::authStateReady, this,
            [this, id](bool loggedIn, const QString &displayName, const QString &message) {
        updateProviderState(id, loggedIn, displayName, message);
    });

    if (p->supportsWebLogin()) {
        QTimer::singleShot(900, this, [this, id] {
            const SessionSnapshot session = loadSession(id);
            if (session.isEmpty()) return;
            if (auto *source = provider(id)) {
                if (m_accounts.contains(id))
                    m_accounts[id].status = QStringLiteral("正在恢复当前本地用户的登录状态…");
                emit accountsChanged();
                source->importWebSession(session.cookieHeader, session.userAgent,
                                         session.storageJson, session.pageUrl);
            }
        });
    }

    emit accountsChanged();
}

QVariantList AccountManager::accounts() const
{
    QVariantList result;
    for (const QString &id : m_order) {
        if (!m_accounts.contains(id)) continue;
        const AccountState &s = m_accounts[id];
        if (!s.provider) continue;
        result << QVariantMap{
            {"id", id},
            {"name", s.provider->displayName()},
            {"loggedIn", s.loggedIn},
            {"displayName", s.displayName},
            {"status", s.status},
            {"supportsWebLogin", s.provider->supportsWebLogin()},
            {"loginUrl", s.provider->loginUrl()},
            {"embeddedBrowserAvailable", embeddedBrowserAvailable()},
            {"hasSavedSession", !loadSession(id).isEmpty()},
            {"playbackOnly", id == QStringLiteral("netease")},
            {"sessionScope", QStringLiteral("当前本地用户")}
        };
    }
    return result;
}

bool AccountManager::embeddedBrowserAvailable() const
{
#ifdef EVOLVEMUSIC_HAS_WEBVIEW
    return true;
#else
    return false;
#endif
}

QString AccountManager::activeProviderName() const
{
    if (auto *p = provider(m_activeProviderId)) return p->displayName();
    return {};
}

QUrl AccountManager::activeLoginUrl() const
{
    if (auto *p = provider(m_activeProviderId)) return QUrl(p->loginUrl());
    return {};
}

void AccountManager::beginWebLogin(const QString &providerId)
{
    IMusicProvider *p = provider(providerId);
    if (!p || !p->supportsWebLogin()) {
        emit toastRequested(QStringLiteral("该平台没有配置登录入口"));
        return;
    }

    m_activeProviderId = providerId;
    emit activeProviderChanged();

    // NetEase uses a native QML QR-login flow. It deliberately does not depend
    // on Qt WebView/WebView2 because the full desktop website is unnecessary
    // for QR authentication and is less reliable inside embedded browsers.
    if (providerId == QStringLiteral("netease")) {
        if (m_accounts.contains(providerId))
            m_accounts[providerId].status = QStringLiteral("正在生成登录二维码…");
        emit accountsChanged();
        emit browserRequested();
        QTimer::singleShot(0, this, &AccountManager::startNeteaseQrLogin);
        return;
    }

#ifndef EVOLVEMUSIC_HAS_WEBVIEW
    emit toastRequested(QStringLiteral("当前 Qt Kit 没有 Qt WebView，无法打开该平台的内嵌网页登录"));
    return;
#else
    if (m_accounts.contains(providerId))
        m_accounts[providerId].status = QStringLiteral("等待网页登录");
    emit accountsChanged();
    emit browserRequested();
#endif
}

void AccountManager::refreshNativeLogin()
{
    if (m_activeProviderId == QStringLiteral("netease"))
        startNeteaseQrLogin();
}

void AccountManager::cancelLogin()
{
    stopNeteaseQrLogin(true);
}

void AccountManager::setQrState(const QString &status, bool busy, const QString &imageSource)
{
    m_qrLoginStatus = status;
    m_qrLoginBusy = busy;
    if (!imageSource.isNull())
        m_qrImageSource = imageSource;
    emit nativeQrLoginChanged();
}

void AccountManager::stopNeteaseQrLogin(bool clearVisuals)
{
    m_qrPollTimer.stop();
    if (m_qrReply) {
        m_qrReply->abort();
        m_qrReply.clear();
    }
    m_qrKey.clear();
    m_nativeQrLoginActive = false;
    m_qrLoginBusy = false;
    if (clearVisuals) {
        m_qrImageSource.clear();
        m_qrLoginStatus.clear();
    }
    emit nativeQrLoginChanged();
}

QNetworkReply *AccountManager::neteaseLoginGet(
    const QString &path, const QList<QPair<QString, QString>> &query)
{
    auto *p = provider(QStringLiteral("netease"));
    if (!p) return nullptr;

    QString base = p->baseUrl().trimmed();
    while (base.endsWith('/')) base.chop(1);
    QUrl url(base + path);
    QUrlQuery q;
    for (const auto &item : query)
        q.addQueryItem(item.first, item.second);
    q.addQueryItem(QStringLiteral("timestamp"), QString::number(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(q);

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("User-Agent", "EvolveMusic/0.3.2");
    return m_loginNetwork.get(request);
}

void AccountManager::startNeteaseQrLogin()
{
    stopNeteaseQrLogin(false);
    m_nativeQrLoginActive = true;
    m_qrNoCookieFallback = false;
    m_qrImageSource.clear();
    setQrState(QStringLiteral("正在生成二维码…"), true, QString());

    m_qrReply = neteaseLoginGet(QStringLiteral("/login/qr/key"), {});
    if (!m_qrReply) {
        setQrState(QStringLiteral("网易云音乐源未注册"), false, QString());
        return;
    }

    connect(m_qrReply, &QNetworkReply::finished, this, [this] {
        QPointer<QNetworkReply> reply = m_qrReply;
        m_qrReply.clear();
        if (!reply) return;
        const QByteArray body = reply->readAll();
        const auto netError = reply->error();
        const QString netErrorText = reply->errorString();
        reply->deleteLater();

        if (netError != QNetworkReply::NoError) {
            setQrState(QStringLiteral("二维码生成失败：") + netErrorText, false, QString());
            return;
        }

        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(body, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            setQrState(QStringLiteral("二维码接口返回了无法解析的数据"), false, QString());
            return;
        }

        const QJsonObject root = doc.object();
        const QJsonObject data = root.value(QStringLiteral("data")).toObject();
        m_qrKey = data.value(QStringLiteral("unikey")).toString();
        if (m_qrKey.isEmpty())
            m_qrKey = data.value(QStringLiteral("data")).toObject().value(QStringLiteral("unikey")).toString();
        if (m_qrKey.isEmpty()) {
            setQrState(QStringLiteral("没有取得网易云二维码 Key"), false, QString());
            return;
        }
        requestNeteaseQrImage(m_qrKey);
    });
}

void AccountManager::requestNeteaseQrImage(const QString &key)
{
    // Keep these parameters aligned with api-enhanced's own qrlogin.html.
    // `platform=web` produces the web login QR format and `ua=pc` keeps
    // create/check requests on the same client profile.
    m_qrReply = neteaseLoginGet(QStringLiteral("/login/qr/create"), {
        {QStringLiteral("key"), key},
        {QStringLiteral("platform"), QStringLiteral("web")},
        {QStringLiteral("qrimg"), QStringLiteral("true")},
        {QStringLiteral("ua"), QStringLiteral("pc")}
    });
    if (!m_qrReply) {
        setQrState(QStringLiteral("无法请求二维码图片"), false, QString());
        return;
    }

    connect(m_qrReply, &QNetworkReply::finished, this, [this] {
        QPointer<QNetworkReply> reply = m_qrReply;
        m_qrReply.clear();
        if (!reply) return;
        const QByteArray body = reply->readAll();
        const auto netError = reply->error();
        const QString netErrorText = reply->errorString();
        reply->deleteLater();

        if (netError != QNetworkReply::NoError) {
            setQrState(QStringLiteral("二维码图片请求失败：") + netErrorText, false, QString());
            return;
        }

        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(body, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            setQrState(QStringLiteral("二维码图片接口返回异常"), false, QString());
            return;
        }

        const QJsonObject root = doc.object();
        const QJsonObject data = root.value(QStringLiteral("data")).toObject();
        QString image = data.value(QStringLiteral("qrimg")).toString();
        if (image.isEmpty())
            image = root.value(QStringLiteral("qrimg")).toString();
        if (image.isEmpty()) {
            setQrState(QStringLiteral("网易云没有返回二维码图片"), false, QString());
            return;
        }

        QString imageSource = image;
        if (image.startsWith(QStringLiteral("data:image"), Qt::CaseInsensitive)) {
            const qsizetype comma = image.indexOf(',');
            if (comma > 0) {
                const QByteArray png = QByteArray::fromBase64(image.mid(comma + 1).toLatin1());
                if (!png.isEmpty()) {
                    const QString loginDir =
                        QDir(profileDataRoot())
                            .filePath(QStringLiteral("login"));
                    QDir().mkpath(loginDir);
                    QDir login(loginDir);
                    const QStringList stale = login.entryList({QStringLiteral("netease-qr-*.png")}, QDir::Files);
                    for (const QString &name : stale) login.remove(name);
                    const QString qrFile = login.filePath(
                        QStringLiteral("netease-qr-%1.png").arg(QDateTime::currentMSecsSinceEpoch()));
                    QSaveFile file(qrFile);
                    if (file.open(QIODevice::WriteOnly) && file.write(png) == png.size() && file.commit())
                        imageSource = QUrl::fromLocalFile(qrFile).toString();
                }
            }
        }

        m_qrImageSource = imageSource;
        setQrState(QStringLiteral("等待扫码"), false, imageSource);
        m_qrPollTimer.start();
        QTimer::singleShot(120, this, &AccountManager::pollNeteaseQrStatus);
    });
}

void AccountManager::pollNeteaseQrStatus()
{
    if (!m_nativeQrLoginActive || m_qrKey.isEmpty() || m_qrReply) return;

    QList<QPair<QString, QString>> query{
        {QStringLiteral("key"), m_qrKey},
        {QStringLiteral("ua"), QStringLiteral("pc")}
    };
    if (m_qrNoCookieFallback)
        query.append({QStringLiteral("noCookie"), QStringLiteral("true")});

    m_qrReply = neteaseLoginGet(QStringLiteral("/login/qr/check"), query);
    if (!m_qrReply) return;

    connect(m_qrReply, &QNetworkReply::finished, this, [this] {
        QPointer<QNetworkReply> reply = m_qrReply;
        m_qrReply.clear();
        if (!reply) return;

        const QByteArray body = reply->readAll();
        const auto netError = reply->error();
        const QString netErrorText = reply->errorString();

        // Some server/runtime combinations expose the authenticated session
        // through Set-Cookie even when the JSON wrapper differs.
        QStringList headerCookies;
        for (const auto &header : reply->rawHeaderPairs()) {
            if (header.first.compare("Set-Cookie", Qt::CaseInsensitive) != 0)
                continue;
            const QString raw = QString::fromUtf8(header.second).trimmed();
            const QString pair = raw.section(';', 0, 0).trimmed();
            if (!pair.isEmpty())
                headerCookies << pair;
        }

        reply->deleteLater();

        if (netError != QNetworkReply::NoError) {
            setQrState(QStringLiteral("本地网易云服务暂时无响应：") + netErrorText,
                       false, m_qrImageSource);
            return;
        }

        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(body, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            setQrState(QStringLiteral("扫码状态返回异常，正在重试…"),
                       false, m_qrImageSource);
            return;
        }

        const QJsonObject root = doc.object();
        const QJsonObject data = root.value(QStringLiteral("data")).toObject();
        const QJsonObject wrappedBody = root.value(QStringLiteral("body")).toObject();

        auto codeFrom = [](const QJsonObject &object) -> int {
            const QJsonValue value = object.value(QStringLiteral("code"));
            if (value.isDouble()) return value.toInt();
            if (value.isString()) {
                bool ok = false;
                const int code = value.toString().toInt(&ok);
                return ok ? code : 0;
            }
            return 0;
        };

        int code = codeFrom(root);
        if (code == 0) code = codeFrom(data);
        if (code == 0) code = codeFrom(wrappedBody);

        auto stringFrom = [](const QJsonObject &object, const QString &key) -> QString {
            const QJsonValue value = object.value(key);
            if (value.isString()) return value.toString().trimmed();
            return {};
        };

        QString message = stringFrom(root, QStringLiteral("message"));
        if (message.isEmpty()) message = stringFrom(data, QStringLiteral("message"));
        if (message.isEmpty()) message = stringFrom(wrappedBody, QStringLiteral("message"));

        QString cookie = stringFrom(root, QStringLiteral("cookie"));
        if (cookie.isEmpty()) cookie = stringFrom(data, QStringLiteral("cookie"));
        if (cookie.isEmpty()) cookie = stringFrom(wrappedBody, QStringLiteral("cookie"));

        // Be tolerant of wrappers that return cookie arrays rather than the
        // joined string used by login_qr_check.js.
        auto cookieArrayFrom = [](const QJsonObject &object) -> QString {
            const QJsonArray values = object.value(QStringLiteral("cookie")).toArray();
            QStringList out;
            for (const QJsonValue &v : values) {
                const QString item = v.toString().trimmed();
                if (!item.isEmpty()) out << item;
            }
            return out.join(';');
        };
        if (cookie.isEmpty()) cookie = cookieArrayFrom(root);
        if (cookie.isEmpty()) cookie = cookieArrayFrom(data);
        if (cookie.isEmpty()) cookie = cookieArrayFrom(wrappedBody);

        if (cookie.isEmpty() && !headerCookies.isEmpty())
            cookie = headerCookies.join(';');

        if (code == 800) {
            m_qrPollTimer.stop();
            setQrState(QStringLiteral("二维码已过期，请点击刷新"), false, m_qrImageSource);
            return;
        }
        if (code == 801) {
            setQrState(QStringLiteral("等待扫码"), false, m_qrImageSource);
            return;
        }
        if (code == 802) {
            setQrState(QStringLiteral("已扫码 · 请在手机上确认"), false, m_qrImageSource);
            return;
        }

        // api-enhanced documentation explicitly recommends retrying QR status
        // with noCookie=true when the post-scan check returns 502.
        if (code == 502 && !m_qrNoCookieFallback) {
            m_qrNoCookieFallback = true;
            setQrState(QStringLiteral("手机已确认 · 正在兼容模式获取登录会话…"),
                       true, m_qrImageSource);
            QTimer::singleShot(120, this, &AccountManager::pollNeteaseQrStatus);
            return;
        }

        if (code == 803 || !cookie.isEmpty()) {
            m_qrPollTimer.stop();

            // A few deployments report 803 before exposing the cookie. Retry
            // once using the documented noCookie compatibility mode.
            if (cookie.isEmpty() && !m_qrNoCookieFallback) {
                m_qrNoCookieFallback = true;
                setQrState(QStringLiteral("授权成功 · 正在提取登录会话…"),
                           true, m_qrImageSource);
                QTimer::singleShot(120, this, &AccountManager::pollNeteaseQrStatus);
                return;
            }

            if (cookie.isEmpty()) {
                setQrState(QStringLiteral("手机已确认，但没有取得登录 Cookie，请刷新二维码重试"),
                           false, m_qrImageSource);
                return;
            }

            setQrState(QStringLiteral("登录成功 · 正在验证账号…"), true, m_qrImageSource);

            SessionSnapshot session;
            session.cookieHeader = cookie;
            session.userAgent = QStringLiteral(
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                "AppleWebKit/537.36 Chrome/131 Safari/537.36");
            session.pageUrl = QStringLiteral("netease-qr://login");
            saveSession(QStringLiteral("netease"), session);

            if (m_accounts.contains(QStringLiteral("netease")))
                m_accounts[QStringLiteral("netease")].status = QStringLiteral("正在验证账号…");
            emit accountsChanged();

            if (auto *source = provider(QStringLiteral("netease")))
                source->importWebSession(session.cookieHeader, session.userAgent, {},
                                         session.pageUrl);
            return;
        }

        if (!message.isEmpty()) {
            setQrState(QStringLiteral("%1 · 状态 %2").arg(message).arg(code),
                       false, m_qrImageSource);
        } else if (code != 0) {
            setQrState(QStringLiteral("正在等待登录结果 · 状态 %1").arg(code),
                       false, m_qrImageSource);
        }
    });
}

void AccountManager::finishWebLoginPayload(const QString &providerId,
                                           const QString &payloadJson,
                                           const QString &userAgent,
                                           const QString &pageUrl)
{
    IMusicProvider *p = provider(providerId);
    if (!p) return;

    const QJsonDocument doc = QJsonDocument::fromJson(payloadJson.toUtf8());
    const QJsonObject object = doc.isObject() ? doc.object() : QJsonObject{};

    SessionSnapshot session;
    session.cookieHeader = object.value("cookie").toString().trimmed();
    session.userAgent = userAgent.trimmed();
    session.pageUrl = pageUrl.trimmed();

    QJsonObject storage;
    storage.insert("localStorage", object.value("localStorage"));
    storage.insert("sessionStorage", object.value("sessionStorage"));
    session.storageJson = QString::fromUtf8(
        QJsonDocument(storage).toJson(QJsonDocument::Compact));

    const bool storageEmpty =
        object.value("localStorage").toObject().isEmpty()
        && object.value("sessionStorage").toObject().isEmpty();

    if (session.cookieHeader.isEmpty() && storageEmpty) {
        emit toastRequested(QStringLiteral(
            "当前页面没有可同步的网页会话。请确认已经在网页内完成登录，再点“完成登录”。"));
        return;
    }

    if (m_accounts.contains(providerId))
        m_accounts[providerId].status = QStringLiteral("正在验证账号…");
    emit accountsChanged();

    saveSession(providerId, session);
    p->importWebSession(session.cookieHeader, session.userAgent,
                        session.storageJson, session.pageUrl);
}

void AccountManager::refreshAccount(const QString &providerId)
{
    IMusicProvider *p = provider(providerId);
    if (!p) return;

    if (!p->isAuthenticated()) {
        const SessionSnapshot session = loadSession(providerId);
        if (!session.isEmpty()) {
            if (m_accounts.contains(providerId))
                m_accounts[providerId].status = QStringLiteral("正在重新验证保存的会话…");
            emit accountsChanged();
            p->importWebSession(session.cookieHeader, session.userAgent,
                                session.storageJson, session.pageUrl);
            return;
        }
    }
    p->checkAuth();
}

void AccountManager::logout(const QString &providerId)
{
    if (auto *p = provider(providerId)) p->logout();
    removeSession(providerId);

    if (m_accounts.contains(providerId)) {
        m_accounts[providerId].loggedIn = false;
        m_accounts[providerId].displayName.clear();
        m_accounts[providerId].status = QStringLiteral("已退出登录");
        emit accountsChanged();
    }
}

QString AccountManager::sessionDataRoot() const
{
    return QDir(profileDataRoot())
        .filePath(QStringLiteral("sessions"));
}

IMusicProvider *AccountManager::provider(const QString &providerId) const
{
    return m_accounts.contains(providerId) ? m_accounts.value(providerId).provider : nullptr;
}

void AccountManager::updateProviderState(const QString &providerId, bool loggedIn,
                                         const QString &displayName, const QString &message)
{
    if (!m_accounts.contains(providerId)) return;
    AccountState &s = m_accounts[providerId];
    s.loggedIn = loggedIn;
    s.displayName = loggedIn ? displayName : QString();
    s.status = message.isEmpty()
        ? (loggedIn ? QStringLiteral("已登录") : QStringLiteral("未登录"))
        : message;
    emit accountsChanged();

    if (!m_switchingProfile)
        emit toastRequested(s.status);

    if (loggedIn && providerId == m_activeProviderId) {
        stopNeteaseQrLogin(false);
        emit browserLoginSucceeded();
    }
}

QString AccountManager::sessionFilePath(const QString &providerId) const
{
    QString safe = providerId;
    safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), "_");
    return QDir(sessionDataRoot()).filePath(safe + ".session");
}

bool AccountManager::saveSession(const QString &providerId, const SessionSnapshot &session)
{
    emit providerSessionCaptured(
        providerId,
        session.cookieHeader,
        session.userAgent,
        session.pageUrl);

    QJsonObject object{
        {"cookie", session.cookieHeader},
        {"userAgent", session.userAgent},
        {"storage", session.storageJson},
        {"pageUrl", session.pageUrl}
    };
    const QByteArray plain = QJsonDocument(object).toJson(QJsonDocument::Compact);
    const QByteArray encrypted = protectForCurrentUser(plain);
    if (encrypted.isEmpty()) return false;

    QDir().mkpath(sessionDataRoot());
    QSaveFile file(sessionFilePath(providerId));
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (file.write(encrypted) != encrypted.size()) return false;
    return file.commit();
}

AccountManager::SessionSnapshot AccountManager::loadSession(const QString &providerId) const
{
    QFile file(sessionFilePath(providerId));
    if (!file.open(QIODevice::ReadOnly)) return {};

    const QByteArray plain = unprotectForCurrentUser(file.readAll());
    if (plain.isEmpty()) return {};

    const QJsonDocument doc = QJsonDocument::fromJson(plain);
    if (!doc.isObject()) return {};
    const QJsonObject object = doc.object();

    SessionSnapshot session;
    session.cookieHeader = object.value("cookie").toString();
    session.userAgent = object.value("userAgent").toString();
    session.storageJson = object.value("storage").toString();
    session.pageUrl = object.value("pageUrl").toString();
    return session;
}

void AccountManager::removeSession(const QString &providerId)
{
    QFile::remove(sessionFilePath(providerId));
    emit providerSessionRemoved(providerId);
}

QByteArray AccountManager::protectForCurrentUser(const QByteArray &plain)
{
    if (plain.isEmpty()) return {};
#ifdef Q_OS_WIN
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plain.constData()));
    input.cbData = static_cast<DWORD>(plain.size());

    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"EvolveMusic provider session", nullptr, nullptr,
                          nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
        return {};

    QByteArray encrypted(reinterpret_cast<const char *>(output.pbData),
                         static_cast<qsizetype>(output.cbData));
    LocalFree(output.pbData);
    return encrypted;
#else
    return {};
#endif
}

QByteArray AccountManager::unprotectForCurrentUser(const QByteArray &encrypted)
{
    if (encrypted.isEmpty()) return {};
#ifdef Q_OS_WIN
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(encrypted.constData()));
    input.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output))
        return {};

    QByteArray plain(reinterpret_cast<const char *>(output.pbData),
                     static_cast<qsizetype>(output.cbData));
    LocalFree(output.pbData);
    return plain;
#else
    return {};
#endif
}
