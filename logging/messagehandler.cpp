#include "messagehandler.h"

#include <KNotification>
#include <QDateTime>
#include <QStandardPaths>

#include <cstdio>

Q_LOGGING_CATEGORY(main_cpp, "kiot.main")

QtMessageHandler originalHandler = nullptr;

void initLogging()
{
    originalHandler = qInstallMessageHandler(kiotMessageHandler);
}

void kiotMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);

    QString level;
    const char *color = "";
    const char *reset = "\033[0m";

    switch (type) {
    case QtDebugMsg:
        color = "\033[90m"; // bright black (gray)
        level = "DEBUG";
        break;
    case QtInfoMsg:
        color = "\033[32m"; // green
        level = "INFO";
        break;
    case QtWarningMsg:
        color = "\033[33m"; // yellow
        level = "WARN";
        break;
    case QtCriticalMsg:
        color = "\033[31m"; // red
        level = "ERROR";
        break;
    case QtFatalMsg:
        color = "\033[1;31m"; // bold red
        level = "FATAL";
        break;
    }

    QString line = QString("[%1] [%2] [%3] %4").arg(timestamp, level, context.category, msg);
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        KNotification::event(KNotification::Notification, QStringLiteral("Kiot %1").arg(level), msg);
    }
    // 1. Print to the terminal
    fprintf(stderr, "%s%s%s\n", color, line.toUtf8().constData(), reset);

    if (type == QtFatalMsg) {
        abort();
    }
}
