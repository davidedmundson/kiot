#include "core.h"

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QMutex>

#include <KAboutData>
#include <KConfigGroup>
#include <KDBusService>
#include <KSharedConfig>
#include <KSignalHandler>
// Is there a way to get the signal directly from
#include <csignal>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(main_cpp)
Q_LOGGING_CATEGORY(main_cpp, "kiot.main")

QtMessageHandler originalHandler = nullptr;
// bool to disable file logging if not wanted
bool logToFile = true;
// Max log file size
constexpr qint64 MAX_LOG_FILE_SIZE = 2 * 1024 * 1024; // 2 MB

class KiotFileLogger
{
public:
    static KiotFileLogger& instance() {
        static KiotFileLogger inst;
        return inst;
    }

    void write(const QString &line) {
        QMutexLocker locker(&m_mutex);
        if (!m_file.isOpen()) return;

        // Cgeck log size to not exceed MAX_LOG_FILE_SIZE
        if (m_file.size() > MAX_LOG_FILE_SIZE) {
            m_file.close();

            // keeps 1 backup
            QString backupFile = m_file.fileName() + ".old";
            QFile::remove(backupFile); // fjern gammel backup hvis eksisterer
            m_file.rename(backupFile);

            // Opens a new logfile
            m_file.open(QIODevice::WriteOnly | QIODevice::Text);
        }

        QTextStream out(&m_file);
        out << line << "\n";
        out.flush();
    }


private:
    KiotFileLogger() {
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
        dir.mkpath(".");
        m_file.setFileName(dir.filePath("kiot_logs.log"));
        m_file.open(QIODevice::Append | QIODevice::Text);
    }

    ~KiotFileLogger() { m_file.close(); }

    QFile m_file;
    QMutex m_mutex;
};

void kiotMessageHandler(QtMsgType type, const QMessageLogContext &context,const QString &msg)
{
    const QString timestamp =
        QDateTime::currentDateTime().toString(Qt::ISODate);

    QString level;
    switch (type) {
        case QtDebugMsg:    level = "DEBUG"; break;
        case QtInfoMsg:     level = "INFO";  break;
        case QtWarningMsg:  level = "WARN";  break;
        case QtCriticalMsg: level = "ERROR"; break;
        case QtFatalMsg:    level = "FATAL"; break;
    }
    const char *color = "";
    const char *reset = "\033[0m";

    switch (type) {
        case QtDebugMsg:
            color = "\033[90m";  // bright black (gray)
            level = "DEBUG";
            break;
        case QtInfoMsg:
            color = "\033[32m";  // green
            level = "INFO";
            break;
        case QtWarningMsg:
            color = "\033[33m";  // yellow
            level = "WARN";
            break;
        case QtCriticalMsg:
            color = "\033[31m";  // red
            level = "ERROR";
            break;
        case QtFatalMsg:
            color = "\033[1;31m"; // bold red
            level = "FATAL";
            break;
    }

    QString line = QString("[%1] [%2] [%3] %4").arg(timestamp, level, context.category, msg);
    // 1. Prints to the terminal
    fprintf(stderr, "%s%s%s\n", color, line.toUtf8().constData(), reset);

    //2. Write to the log file
    if (logToFile)
        KiotFileLogger::instance().write(line);

    if (type == QtFatalMsg)
        abort();
}

int main(int argc, char **argv)
{
    originalHandler = qInstallMessageHandler(kiotMessageHandler);
    QApplication app(argc, argv);
    KAboutData aboutData(QStringLiteral("kiot"), "KDE IOT", QStringLiteral("0.1"), "KDE Internet of Things Connection", KAboutLicense::GPL_V3, "© 2024");
    KDBusService service(KDBusService::Unique | KDBusService::Replace);
    HaControl appControl;

    KSignalHandler::self()->watchSignal(SIGTERM);
    KSignalHandler::self()->watchSignal(SIGINT);
    QObject::connect(KSignalHandler::self(), &KSignalHandler::signalReceived, [](int sig) {
        if (sig == SIGTERM || sig == SIGINT) {
            qCInfo(main) << "Shuting down kiot";
            QApplication::quit();
        }
    });

    app.exec();
}
// SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LGPL-2.1-or-later
