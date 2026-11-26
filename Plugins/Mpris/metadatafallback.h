/*
    SPDX-FileCopyrightText: 2012 Alex Merry <alex.merry@kdemail.net>
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
    SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/
// The track and album PlayerContainer shows when a player's metadata has no xesam:title or xesam:album,
// taken from xesam:url.

#pragma once

#include <QList>
#include <QString>
#include <QStringView>
#include <QUrl>
#include <iterator>

// The last part of the URL, percent-decoded
inline QString trackFromXesamUrl(QStringView xesamUrl)
{
    if (xesamUrl.isEmpty()) {
        return QString();
    }
    if (int lastSlashPos = xesamUrl.lastIndexOf(QLatin1Char('/')); lastSlashPos < 0 || lastSlashPos == xesamUrl.size() - 1) {
        return QString();
    } else {
        const QStringView lastUrlPart = xesamUrl.sliced(lastSlashPos + 1);
        return QUrl::fromPercentEncoding(lastUrlPart.toLatin1());
    }
}

// The folder a local file (file:///) is in, percent-decoded
inline QString albumFromXesamUrl(QStringView xesamUrl)
{
    if (!xesamUrl.startsWith(QLatin1String("file:///"))) {
        return QString();
    }
    const QList<QStringView> urlParts = xesamUrl.split(QLatin1Char('/'));
    if (urlParts.size() < 3) {
        return QString();
    }
    if (auto lastFolderPathIt = std::next(urlParts.crbegin()); !lastFolderPathIt->isEmpty()) {
        return QUrl::fromPercentEncoding(lastFolderPathIt->toLatin1());
    }
    return QString();
}
