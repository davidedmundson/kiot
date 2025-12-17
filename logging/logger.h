#ifndef KIOT_LOGGER_H
#define KIOT_LOGGER_H

#include <QFile>
#include <QMutex>
#include <QString>

/**
 * @file logger.h
 * @brief File-based logging with rotation support
 */

/**
 * @class KiotFileLogger
 * @brief Singleton class for file-based logging with rotation support
 * 
 * This class provides thread-safe file logging with automatic log rotation
 * when the log file exceeds MAX_LOG_FILE_SIZE. It maintains one backup file
 * with the .old extension.
 */
class KiotFileLogger
{
public:
    /**
     * @brief Get the singleton instance of KiotFileLogger
     * @return Reference to the singleton instance
     */
    static KiotFileLogger& instance();

    /**
     * @brief Write a log line to the file
     * @param line The formatted log line to write
     * 
     * This method is thread-safe and will automatically rotate the log file
     * if it exceeds MAX_LOG_FILE_SIZE. The rotation keeps one backup file.
     * @note Log messages may be lost if the file cannot be reopened after rotation
     */
    void write(const QString &line);

private:
    KiotFileLogger();
    ~KiotFileLogger();
    
    // Delete copy constructor and assignment operator
    KiotFileLogger(const KiotFileLogger&) = delete;
    KiotFileLogger& operator=(const KiotFileLogger&) = delete;
    
    void rotateLogFile();
    void openLogFile();
    
    QFile m_file;        ///< The log file handle
    QMutex m_mutex;      ///< Mutex for thread-safe operations
    
    /// Maximum log file size before rotation (2 MB)
    static constexpr qint64 MAX_LOG_FILE_SIZE = 2 * 1024 * 1024;
};

#endif // KIOT_LOGGER_H
