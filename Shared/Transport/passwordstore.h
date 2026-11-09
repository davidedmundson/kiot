// SPDX-FileCopyrightText: 2026
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QObject>
#include <QString>
#include <functional>

#include "KIOTShared/kiotshared_export.h"

namespace KIOTShared {
namespace Transport {

/**
 * @brief The MQTT password, kept in the wallet (KWallet or the Secret Service, through QtKeychain;
 * in a Flatpak through libsecret and the Secret portal, so the sandbox needs no access to the wallet).
 *
 * The password belongs to an account, the MQTT user on a broker host: it is only given out for the
 * account it was stored for, so a changed host never gets the password of another broker.
 *
 * Earlier versions kept it in kiotrc ([general] password). read() moves such a password into the
 * wallet, for the account in kiotrc. Without a wallet, the password stays in kiotrc.
 */
class KIOT_SHARED_EXPORT PasswordStore
{
public:
    /**
     * @brief Reads the password of @p user on @p host and passes it to @p callback (empty if there
     * is none). @p callback is not called once @p context is destroyed.
     */
    static void read(const QString &user, const QString &host, QObject *context, std::function<void(const QString &password)> callback);

    /**
     * @brief Stores @p password for @p user on @p host (an empty one removes it) and calls
     * @p callback with the result.
     */
    static void write(const QString &user,
                      const QString &host,
                      const QString &password,
                      QObject *context = nullptr,
                      std::function<void(bool stored)> callback = {});
};

} // namespace Transport
} // namespace KIOTShared
