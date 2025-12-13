// SPDX-FileCopyrightText: 2025
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <KLocalizedString>
#include <KPluginFactory>
#include <KQuickManagedConfigModule>

#include "kiotsettings.h"

class KCMKiot : public KQuickManagedConfigModule
{
    Q_OBJECT
    Q_PROPERTY(KiotSettings *settings READ settings CONSTANT)
public:
    explicit KCMKiot(QObject *parent, const KPluginMetaData &metaData, const QVariantList &args)
        : KQuickManagedConfigModule(parent, metaData)
        , m_settings(new KiotSettings(KSharedConfig::openConfig("kiotrc", KSharedConfig::CascadeConfig), this))
    {
        setButtons(Apply | Default);
        qDebug() << m_settings->host() << m_settings->config()->name();
    }

    KiotSettings *settings() const
    {
        return m_settings;
    }

private:
    KiotSettings *m_settings;
};

K_PLUGIN_CLASS_WITH_JSON(KCMKiot, "kcm_kiot.json")

#include "kcm_kiot.moc"
