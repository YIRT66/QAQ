#pragma once

#include <QObject>
#include <QList>
#include <QVariantList>
#include <QVariantMap>
#include <QString>

class LocalProfileManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(QString currentProfileId READ currentProfileId NOTIFY currentProfileChanged)
    Q_PROPERTY(QString currentProfileName READ currentProfileName NOTIFY currentProfileChanged)
    Q_PROPERTY(QString currentProfileDataRoot READ currentProfileDataRoot NOTIFY currentProfileChanged)

public:
    explicit LocalProfileManager(QObject *parent = nullptr);

    QVariantList profiles() const;
    QString currentProfileId() const { return m_currentId; }
    QString currentProfileName() const;
    QString currentProfileDataRoot() const;

    Q_INVOKABLE QString createProfile(const QString &name);
    Q_INVOKABLE bool switchProfile(const QString &profileId);
    Q_INVOKABLE bool renameProfile(const QString &profileId, const QString &name);
    Q_INVOKABLE bool deleteProfile(const QString &profileId);
    Q_INVOKABLE QString profileDataRoot(const QString &profileId) const;

signals:
    void profilesChanged();
    void currentProfileChanged();
    void profileAboutToChange(const QString &oldProfileId,
                              const QString &newProfileId);
    void profileDeleted(const QString &profileId);
    void toastRequested(const QString &message);

private:
    struct Profile {
        QString id;
        QString name;
        qint64 createdAt = 0;
    };

    QString profilesRoot() const;
    QString indexFilePath() const;
    QString cleanName(const QString &name) const;
    QString safeProfileId(const QString &id) const;
    int indexOf(const QString &id) const;
    void loadIndex();
    void ensureDefaultProfile();
    bool saveIndex() const;

    QList<Profile> m_profiles;
    QString m_currentId;
};
