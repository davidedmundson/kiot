#ifndef KIOT_MESSAGEHANDLER_H
#define KIOT_MESSAGEHANDLER_H

#include <QLoggingCategory>
#include <QtGlobal>

// Forward declaration
class KSharedConfig;

/**
 * @file messagehandler.h
 * @brief Custom Qt message handler and logging configuration
 */

// Declare logging category
Q_DECLARE_LOGGING_CATEGORY(main_cpp)

extern QtMessageHandler originalHandler;
extern bool logToFile;

/**
 * @brief Initialize logging configuration
 * 
 * Loads logging settings from KConfig and sets up the message handler.
 * Should be called early in main() before any logging occurs.
 */
void initLogging(KSharedConfig *config = nullptr);

/**
 * @brief Custom Qt message handler for kiot application
 * @param type The type of message (debug, info, warning, etc.)
 * @param context The logging context (file, line, function)
 * @param msg The actual log message
 * 
 * This handler formats log messages with timestamp, log level, and category,
 * then outputs them to both stderr (with colors) and a log file (if enabled).
 * For fatal messages, it calls abort() after logging.
 */
void kiotMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

#endif // KIOT_MESSAGEHANDLER_H
