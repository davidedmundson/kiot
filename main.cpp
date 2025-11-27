#include <QApplication>
#include <QDebug>

#include <KAboutData>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KDBusService>

#include "core.h"

int main(int argc, char ** argv)
{
    QApplication app(argc, argv);
    KAboutData aboutData(
        QStringLiteral("kiot"),
        "KDE IOT",
        QStringLiteral("0.1"),
        "KDE Internet of Things Connection",
        KAboutLicense::GPL_V3,
        "© 2024");
    KDBusService service(KDBusService::Unique);
    HaControl appControl;
    app.exec();
}
// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
