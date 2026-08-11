#include "LocalProfileManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

LocalProfileManager::LocalProfileManager(QObject *parent)
    : QObject(parent)
{
    loadIndex();
    ensureDefaultProfile();
}

QString LocalProfileManager::profilesRoot() const
{
    QString dataRoot =
        qEnvironmentVariable("EVOLVE_MUSIC_DATA_ROOT").trimmed();

    if (dataRoot.isEmpty()) {
        dataRoot =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }

    return QDir(dataRoot).filePath(QStringLiteral("profiles"));
}

QString LocalProfileManager::indexFilePath() const
{
    return QDir(profilesRoot()).filePath(QStringLiteral("profiles.json"));
}

QString LocalProfileManager::cleanName(const QString &name) const
{
    QString out = name.trimmed().simplified();
    if (out.isEmpty())
        out = QStringLiteral("本地用户");
    if (out.size() > 24)
        out = out.left(24);
    return out;
}

QString LocalProfileManager::safeProfileId(const QString &id) const
{
    QString out = id.trimmed();
    out.replace(
        QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")),
        QStringLiteral("_"));

    if (out.isEmpty())
        out = QStringLiteral("default");

    return out;
}

int LocalProfileManager::indexOf(const QString &id) const
{
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles.at(i).id == id)
            return i;
    }
    return -1;
}

void LocalProfileManager::loadIndex()
{
    QFile file(indexFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll());

    if (!document.isObject())
        return;

    const QJsonObject root = document.object();
    const QJsonArray array =
        root.value(QStringLiteral("profiles")).toArray();

    QList<Profile> loaded;

    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();

        Profile profile;
        profile.id =
            safeProfileId(
                object.value(QStringLiteral("id")).toString());
        profile.name =
            cleanName(
                object.value(QStringLiteral("name")).toString());
        profile.createdAt =
            static_cast<qint64>(
                object.value(QStringLiteral("createdAt")).toDouble());

        if (profile.createdAt <= 0)
            profile.createdAt = QDateTime::currentMSecsSinceEpoch();

        bool duplicate = false;
        for (const Profile &existing : loaded) {
            if (existing.id == profile.id) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate)
            loaded << profile;
    }

    m_profiles = loaded;
    m_currentId =
        safeProfileId(
            root.value(QStringLiteral("currentProfileId")).toString());
}

void LocalProfileManager::ensureDefaultProfile()
{
    if (m_profiles.isEmpty()) {
        m_profiles << Profile{
            QStringLiteral("default"),
            QStringLiteral("默认用户"),
            QDateTime::currentMSecsSinceEpoch()
        };
    }

    if (indexOf(m_currentId) < 0)
        m_currentId = m_profiles.first().id;

    QDir().mkpath(profilesRoot());

    for (const Profile &profile : m_profiles)
        QDir().mkpath(profileDataRoot(profile.id));

    saveIndex();
}

bool LocalProfileManager::saveIndex() const
{
    QDir().mkpath(profilesRoot());

    QJsonArray array;
    for (const Profile &profile : m_profiles) {
        array.append(QJsonObject{
            {QStringLiteral("id"), profile.id},
            {QStringLiteral("name"), profile.name},
            {QStringLiteral("createdAt"),
             static_cast<double>(profile.createdAt)}
        });
    }

    const QJsonObject root{
        {QStringLiteral("currentProfileId"), m_currentId},
        {QStringLiteral("profiles"), array}
    };

    QSaveFile file(indexFilePath());
    if (!file.open(QIODevice::WriteOnly))
        return false;

    const QByteArray data =
        QJsonDocument(root).toJson(QJsonDocument::Indented);

    if (file.write(data) != data.size())
        return false;

    return file.commit();
}

QVariantList LocalProfileManager::profiles() const
{
    QVariantList result;

    for (const Profile &profile : m_profiles) {
        result << QVariantMap{
            {QStringLiteral("id"), profile.id},
            {QStringLiteral("name"), profile.name},
            {QStringLiteral("current"),
             profile.id == m_currentId},
            {QStringLiteral("createdAt"),
             profile.createdAt},
            {QStringLiteral("dataRoot"),
             profileDataRoot(profile.id)}
        };
    }

    return result;
}

QString LocalProfileManager::currentProfileName() const
{
    const int index = indexOf(m_currentId);
    return index >= 0
        ? m_profiles.at(index).name
        : QStringLiteral("默认用户");
}

QString LocalProfileManager::profileDataRoot(const QString &profileId) const
{
    return QDir(profilesRoot())
        .filePath(safeProfileId(profileId));
}

QString LocalProfileManager::currentProfileDataRoot() const
{
    return profileDataRoot(m_currentId);
}

QString LocalProfileManager::createProfile(const QString &name)
{
    const QString id =
        QStringLiteral("u_")
        + QUuid::createUuid()
              .toString(QUuid::WithoutBraces);

    const Profile profile{
        safeProfileId(id),
        cleanName(name),
        QDateTime::currentMSecsSinceEpoch()
    };

    m_profiles << profile;
    QDir().mkpath(profileDataRoot(profile.id));

    saveIndex();
    emit profilesChanged();

    switchProfile(profile.id);

    emit toastRequested(
        QStringLiteral("已创建本地用户：")
        + profile.name);

    return profile.id;
}

bool LocalProfileManager::switchProfile(const QString &profileId)
{
    const QString id = safeProfileId(profileId);
    const int index = indexOf(id);

    if (index < 0)
        return false;

    if (id == m_currentId)
        return true;

    const QString oldId = m_currentId;
    emit profileAboutToChange(oldId, id);

    m_currentId = id;
    QDir().mkpath(currentProfileDataRoot());
    saveIndex();

    emit currentProfileChanged();
    emit profilesChanged();

    emit toastRequested(
        QStringLiteral("已切换到：")
        + m_profiles.at(index).name);

    return true;
}

bool LocalProfileManager::renameProfile(const QString &profileId,
                                        const QString &name)
{
    const int index =
        indexOf(safeProfileId(profileId));

    if (index < 0)
        return false;

    const QString nextName = cleanName(name);
    if (m_profiles[index].name == nextName)
        return true;

    m_profiles[index].name = nextName;
    saveIndex();

    emit profilesChanged();

    if (m_profiles[index].id == m_currentId)
        emit currentProfileChanged();

    emit toastRequested(
        QStringLiteral("本地用户名已更新"));

    return true;
}

bool LocalProfileManager::deleteProfile(const QString &profileId)
{
    const QString id = safeProfileId(profileId);
    const int index = indexOf(id);

    if (index < 0)
        return false;

    if (id == m_currentId) {
        emit toastRequested(
            QStringLiteral("不能删除正在使用的本地用户，请先切换到其他用户"));
        return false;
    }

    if (m_profiles.size() <= 1) {
        emit toastRequested(
            QStringLiteral("至少需要保留一个本地用户"));
        return false;
    }

    QDir(profileDataRoot(id)).removeRecursively();
    m_profiles.removeAt(index);
    saveIndex();

    emit profileDeleted(id);
    emit profilesChanged();
    emit toastRequested(
        QStringLiteral("本地用户及其本机数据已删除"));

    return true;
}
