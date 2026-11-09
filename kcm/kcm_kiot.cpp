// SPDX-FileCopyrightText: 2025
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <KDirWatch>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KQuickManagedConfigModule>

#include "kiotsettings.h"
#include "platformhelper.h"
#include "Transport/passwordstore.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QLoggingCategory>
#include <QStandardPaths>

Q_DECLARE_LOGGING_CATEGORY(ui)
Q_LOGGING_CATEGORY(ui, "kiot.UI")

using KIOTShared::PlatformHelper;
using KIOTShared::Transport::PasswordStore;

class KCMKiot : public KQuickManagedConfigModule
{
    Q_OBJECT
    Q_PROPERTY(KiotSettings *settings READ settings CONSTANT)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
public:
    explicit KCMKiot(QObject *parent, const KPluginMetaData &metaData, const QVariantList &args)
        : KQuickManagedConfigModule(parent, metaData)
        , m_settings(new KiotSettings(KSharedConfig::openConfig("kiotrc", KSharedConfig::CascadeConfig), this))
    {
        setButtons(Apply | Default);
        qCDebug(ui) << m_settings->host() << m_settings->config()->name();

        // Show what kiot's own settings window saves, unless there are changes to apply here
        const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/kiotrc");
        KDirWatch::self()->addFile(path);
        auto reload = [this, path](const QString &changed) {
            if (changed == path && !needsSave()) {
                m_settings->config()->reparseConfiguration();
                load();
            }
        };
        connect(KDirWatch::self(), &KDirWatch::dirty, this, reload);
        connect(KDirWatch::self(), &KDirWatch::created, this, reload);
        connect(m_settings, &KiotSettings::hostChanged, this, &KCMKiot::loadPassword);
        connect(m_settings, &KiotSettings::userChanged, this, &KCMKiot::loadPassword);
    }

    KiotSettings *settings() const
    {
        return m_settings;
    }

    QString password() const
    {
        return m_password;
    }

    void setPassword(const QString &password)
    {
        if (password != m_password) {
            m_password = password;
            Q_EMIT passwordChanged();
            settingsChanged();
        }
    }

    void load() override
    {
        KQuickManagedConfigModule::load();
        loadPassword();
    }

    void save() override
    {
        KQuickManagedConfigModule::save();
        if (m_password == m_savedPassword) {
            restartKiot();
            return;
        }
        m_savedPassword = m_password;
        PasswordStore::write(m_settings->user(), m_settings->host(), m_password, this, [](bool) {
            restartKiot();
        });
    }

    void defaults() override
    {
        KQuickManagedConfigModule::defaults();
        setPassword(QString());
    }

    bool isSaveNeeded() const override
    {
        return m_password != m_savedPassword;
    }

    bool isDefaults() const override
    {
        return m_password.isEmpty();
    }

Q_SIGNALS:
    void passwordChanged();

public:
    Q_INVOKABLE void openKiotSettings()
    {
        if (isKiotRunning()) {
            activateKiot();
            return;
        }
        auto watcher = new QDBusServiceWatcher(serviceName(), QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForRegistration, this);
        connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, [this, watcher]() {
            watcher->deleteLater();
            activateKiot();
        });
        PlatformHelper::startApplication();
    }

private:
    void loadPassword()
    {
        PasswordStore::read(m_settings->user(), m_settings->host(), this, [this](const QString &password) {
            m_savedPassword = password;
            setPassword(password);
        });
    }

    static void restartKiot()
    {
        if (isKiotRunning()) {
            PlatformHelper::restartApplication();
        }
    }

    // The name KDBusService registers: the reversed organization domain and the application name
    static QString serviceName()
    {
        QStringList parts = PlatformHelper::resolveOrganizationDomain(QStringLiteral(PROJECT_DOMAIN)).split(QLatin1Char('.'), Qt::SkipEmptyParts);
        std::reverse(parts.begin(), parts.end());
        return parts.join(QLatin1Char('.')) + QLatin1Char('.') + QStringLiteral(PROJECT_NAME);
    }

    static bool isKiotRunning()
    {
        return QDBusConnection::sessionBus().interface()->isServiceRegistered(serviceName());
    }

    static void activateKiot()
    {
        auto message = QDBusMessage::createMethodCall(serviceName(),
                                                      QLatin1Char('/') + serviceName().replace(QLatin1Char('.'), QLatin1Char('/')),
                                                      QStringLiteral("org.freedesktop.Application"),
                                                      QStringLiteral("Activate"));
        message << QVariantMap();
        QDBusConnection::sessionBus().asyncCall(message);
    }

    KiotSettings *m_settings;
    QString m_password;
    QString m_savedPassword;
};

K_PLUGIN_CLASS_WITH_JSON(KCMKiot, "kcm_kiot.json")

#include "kcm_kiot.moc"
