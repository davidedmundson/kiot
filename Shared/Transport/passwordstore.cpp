// SPDX-FileCopyrightText: 2026
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "passwordstore.h"
#include "platformhelper.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <qt6keychain/keychain.h>

DEFINE_LOGGER(passwordlogger, Shared.PasswordStore)

namespace KIOTShared {
namespace Transport {

namespace {
const QString service = QStringLiteral(APP_ID);
// written by the first run for "not configured yet", not a password to move
const QString placeholder = QStringLiteral("mqtt-password-here");

KSharedConfigPtr config()
{
    return KSharedConfig::openConfig(PlatformHelper::configFilePath(), KConfig::SimpleConfig);
}

QString accountKey(const QString &user, const QString &host)
{
    return user.isEmpty() ? host : user + QLatin1Char('@') + host;
}
} // namespace

void PasswordStore::read(const QString &user, const QString &host, QObject *context, std::function<void(const QString &password)> callback)
{
    auto cfg = config();
    cfg->reparseConfiguration();
    const KConfigGroup group = cfg->group(QStringLiteral("general"));
    const QString inFile = group.readEntry("password");
    if (!inFile.isEmpty() && inFile != placeholder) {
        const QString fileUser = group.readEntry("user");
        const QString fileHost = group.readEntry("host");
        qCInfo(passwordlogger) << "Moving the MQTT password from" << PlatformHelper::configFilePath() << "into the wallet";
        write(fileUser, fileHost, inFile);
        if (user == fileUser && host == fileHost) {
            callback(inFile);
            return;
        }
    }

    auto job = new QKeychain::ReadPasswordJob(service);
    job->setKey(accountKey(user, host));
    QObject::connect(job, &QKeychain::Job::finished, context, [job, callback]() {
        if (job->error() == QKeychain::NoError) {
            callback(job->textData());
            return;
        }
        if (job->error() != QKeychain::EntryNotFound) {
            qCWarning(passwordlogger) << "Cannot read the MQTT password from the wallet:" << job->errorString();
        }
        callback(QString());
    });
    job->start();
}

void PasswordStore::write(const QString &user, const QString &host, const QString &password, QObject *context, std::function<void(bool stored)> callback)
{
    QKeychain::Job *job;
    if (password.isEmpty()) {
        auto deleteJob = new QKeychain::DeletePasswordJob(service);
        deleteJob->setKey(accountKey(user, host));
        job = deleteJob;
    } else {
        auto writeJob = new QKeychain::WritePasswordJob(service);
        writeJob->setKey(accountKey(user, host));
        writeJob->setTextData(password);
        job = writeJob;
    }

    QObject::connect(job, &QKeychain::Job::finished, context ? context : job, [job, password, callback]() {
        const bool stored = job->error() == QKeychain::NoError || (password.isEmpty() && job->error() == QKeychain::EntryNotFound);
        auto cfg = config();
        auto group = cfg->group(QStringLiteral("general"));
        if (stored) {
            if (group.hasKey("password")) {
                group.deleteEntry("password");
                cfg->sync();
            }
        } else {
            qCWarning(passwordlogger) << "Cannot store the MQTT password in the wallet, keeping it in"
                                      << PlatformHelper::configFilePath() << ":" << job->errorString();
            group.writeEntry("password", password);
            cfg->sync();
        }
        if (callback) {
            callback(stored);
        }
    });
    job->start();
}

} // namespace Transport
} // namespace KIOTShared
