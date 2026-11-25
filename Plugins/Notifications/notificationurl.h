// SPDX-FileCopyrightText: 2026 Thomas Kastenholz
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QUrl>

// Whether the "url" of a notification may be opened when it is clicked. Anyone who can publish to the
// notify topic chooses it, so only web and mail links: no file:// (a downloaded .desktop file or script)
// and no custom scheme that would hand it to another application's URL handler.
inline bool isOpenableNotificationUrl(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty()) {
        return false;
    }
    const QString scheme = url.scheme(); // QUrl keeps schemes in lower case
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https")) {
        return !url.host().isEmpty();
    }
    return scheme == QLatin1String("mailto") && !url.path().isEmpty();
}
