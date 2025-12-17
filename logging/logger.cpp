#include "logger.h"

#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <cstdio>

KiotFileLogger& KiotFileLogger::instance() {
    static KiotFileLogger inst;
    return inst;
}

void KiotFileLogger::write(const QString &line) {
    QMutexLocker locker(&m_mutex);

    // Check if it's time to rotate the log file
    if (m_file.isOpen() && m_file.size() > MAX_LOG_FILE_SIZE) {
        rotateLogFile();
    }

    // Re-open the logfile if needed
    if (!m_file.isOpen()) {
        openLogFile();
    }
    
    // Write the log line
    if (m_file.isOpen()) {
        QTextStream out(&m_file);
        out << line << "\n";
        out.flush();
    }
}

KiotFileLogger::KiotFileLogger() {
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    dir.mkpath(".");
    m_file.setFileName(dir.filePath("kiot_logs.log"));
    if (!m_file.open(QIODevice::Append | QIODevice::Text)) {
        // Log to stderr since file logging failed
        fprintf(stderr, "Failed to open log file: %s\n", m_file.errorString().toUtf8().constData());
    }
}

KiotFileLogger::~KiotFileLogger() { 
    m_file.close(); 
}

void KiotFileLogger::rotateLogFile() {
    if (m_file.isOpen()) {
        m_file.close();
    }
    QString backupFile = m_file.fileName() + ".old";
    QFile::remove(backupFile);
    m_file.rename(backupFile);
}

void KiotFileLogger::openLogFile() {
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    dir.mkpath(".");
    m_file.setFileName(dir.filePath("kiot_logs.log"));
    if (!m_file.open(QIODevice::Append | QIODevice::Text)) {
        fprintf(stderr, "Failed to reopen log file after rotation: %s\n", m_file.errorString().toUtf8().constData());
    }
}
