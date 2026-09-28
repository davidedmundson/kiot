// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file platformhelper.cpp
 * @brief Implementation of the platform and process detection helpers.
 *
 * @ingroup kiot
 */

#include "platformhelper.h"


#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QStringList>
#include <QApplication>
#include <QUrl>
#include <QString>
#include <QStringList>
#include <QLoggingCategory>
#include <QSettings>
#include <QSysInfo>
#include <QGuiApplication>


/**
 * @brief Logging category used by the platform helper.
 */
DEFINE_LOGGER(helper, Shared.PlatformHelper)


namespace KIOTShared
{
/**
 * @brief Detect the current platform using compile-time macros.
 *
 * @return the detected @ref PlatformHelper::Platform value.
 */
PlatformHelper::Platform PlatformHelper::currentPlatform()
{
    #if defined(Q_OS_WIN)
        return Platform::Windows;
    #elif defined(Q_OS_LINUX)
        return Platform::Linux;
    #elif defined(Q_OS_MACOS)
        return Platform::macOS;
    #elif defined(Q_OS_ANDROID)
        return Platform::Android;
    #elif defined(Q_OS_IOS)
        return Platform::iOS;
    #else
        return Platform::Unknown;
    #endif
}

/**
 * @brief Detect the current CPU architecture.
 *
 * @return the detected @ref PlatformHelper::Architecture value.
 */
PlatformHelper::Architecture PlatformHelper::currentArchitecture()
{
    QString arch = QSysInfo::currentCpuArchitecture();
    
    if (arch == "x86") return Architecture::X86;
    if (arch == "x86_64") return Architecture::X86_64;
    if (arch == "arm") return Architecture::ARM;
    if (arch == "arm64") return Architecture::ARM64;
    
    return Architecture::UnknownArch;
}

/**
 * @brief Check whether the application runs inside a Flatpak sandbox.
 *
 * The result is computed once and then cached for the lifetime of the process.
 *
 * @return @c true if running inside a Flatpak sandbox, otherwise @c false.
 */
bool PlatformHelper::isFlatpak()
{
    // Sjekk standard Flatpak miljøvariabler
    static bool cached = false;
    static bool isFlatpakValue = false;
    
    if (!cached) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        isFlatpakValue = env.contains("FLATPAK_ID") || 
                         env.value("container") == "flatpak" ||
                         env.contains("FLATPAK_SANDBOX_DIR");
        cached = true;
    }
    
    return isFlatpakValue;
}

/**
 * @brief Check whether the application runs inside a Snap confinement.
 *
 * @return @c true if the @c SNAP environment variable is set.
 */
bool PlatformHelper::isSnap()
{
    return !qEnvironmentVariableIsEmpty("SNAP");
}

/**
 * @brief Returns the path to the autostart directory for the current platform.
 *
 * @return the autostart directory, or an empty QString if unsupported.
 */
QString PlatformHelper::autostartPath()
{
    Platform platform = currentPlatform();
    
    switch (platform) {
    case Platform::Linux:
        if (isFlatpak()) {
            return QDir::homePath() + "/.config/autostart";
        }
        return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
        
    case Platform::Windows:
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) 
               + "/Microsoft/Windows/Start Menu/Programs/Startup";
               
    case Platform::macOS:
        return QDir::homePath() + "/Library/LaunchAgents";
        
    default:
        return QString();
    }
}

/**
 * @brief Human-readable name of the current platform.
 *
 * @return e.g. @c "Windows", @c "Linux", @c "macOS", @c "Android", @c "iOS"
 *         or @c "Unknown".
 */
QString PlatformHelper::platformName()
{
    switch (currentPlatform()) {
    case Platform::Windows: return "Windows";
    case Platform::Linux: return "Linux";
    case Platform::macOS: return "macOS";
    case Platform::Android: return "Android";
    case Platform::iOS: return "iOS";
    default: return "Unknown";
    }
}

/**
 * @brief Generate a reverse-DNS service name from the organisation domain.
 *
 * @return the generated service name.
 */
QString PlatformHelper::generateServiceName()
{
    return QString(APP_ID);
    /* Kept for remembrance of how the cmakelists function was created
    QString domain = resolveOrganizationDomain(QString(PROJECT_DOMAIN));
    if (domain.isEmpty())
        domain = "local";

    QStringList parts = domain.split('.', Qt::SkipEmptyParts);
    QString reversed;
    for (const auto &p : parts)
        reversed.prepend(p + ".");
    return reversed + QString(PROJECT_NAME);
    */
}


/**
 * @brief Generate a reverse-DNS service name from the organisation domain.
 *
 * @return the generated service name.
 */
QString PlatformHelper::getProjectName()
{
    return QString(PROJECT_NAME);    
}

/**
 * @brief Normalise a raw domain string into a lower-cased host name.
 *
 * If no scheme is present the input is interpreted as a host and prefixed
 * with @c https:// so that @c QUrl can parse it as such.
 *
 * @param input the raw domain string.
 * @return the lower-cased host name, or @c "theoddpirate.com" when invalid.
 */
QString PlatformHelper::resolveOrganizationDomain(const QString &input)
{
    if (input.trimmed().isEmpty())
        return QStringLiteral("davidedmundson.org");

    QUrl url(input);

    if (!url.isValid() || url.scheme().isEmpty())
        url = QUrl(QStringLiteral("https://") + input);

    QString host = url.host();

    if (host.isEmpty())
        return QStringLiteral("davidedmundson.org");

    return host.toLower();
}

/**
 * @brief Path of this application's configuration file.
 *
 * @return e.g. <tt>~/.config/odd-macrosrc</tt> on Linux.
 */
QString PlatformHelper::configFilePath(const QString &fileType)
{
    QString configFilePath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +  "/" + QStringLiteral(PROJECT_NAME) + fileType;
    QFile file(configFilePath);
    if(!file.exists())
    {
        if (!file.open(QIODevice::ReadWrite)) {
            qDebug() << "Failed to create/open file:" << file.errorString();
            return configFilePath;
        }
        file.seek(0); 
        file.close();
    }
    return configFilePath;
}
/**
 * @brief Path of this application's configuration directory.
 *
 * @return e.g. <tt>~/.config/kiot</tt> on Linux.
 */
QString PlatformHelper::configDirPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/" + QStringLiteral(PROJECT_NAME) ;
}

/**
 * @brief Paths of this application's app data locations.
 *
 * @return e.g. <tt>~{"/home/theoddpirate/.local/share/kiot/","/usr/share/kiot/"}</tt> on Linux.
 */
QStringList PlatformHelper::appdataDirPaths()
{
    // TODO Cleanup this to make it more usable?
    QStringList paths;
   
    QString userPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    qDebug() << userPath;
    if(!QDir(userPath).exists())
        QDir().mkpath(userPath);
    if(QDir(userPath).exists())
    {
        if(!QDir(userPath).exists())
             QDir().mkpath(userPath);
        paths.append(userPath);
    }
    QStringList dataPaths = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    for (const QString &dataPath :  dataPaths)
    {
        QDir dir(dataPath);
        if(dir.exists())
        {
            paths.append(dataPath);
        }
    }
    if(isFlatpak())
        paths.append("/app/plugins");
    return paths;
}



/**
 * @brief The full build ABI of the Qt runtime.
 *
 * @return value from @c QSysInfo::buildAbi().
 */
QString PlatformHelper::buildAbi()
{
    return QSysInfo::buildAbi();
}

/**
 * @brief The version string of the running kernel.
 *
 * @return value from @c QSysInfo::kernelVersion().
 */
QString PlatformHelper::kernelVersion()
{
    return QSysInfo::kernelVersion();
}

/**
 * @brief A human-readable product name of the operating system.
 *
 * @return value from @c QSysInfo::prettyProductName().
 */
QString PlatformHelper::productName()
{
    return QSysInfo::prettyProductName();
}




/**
 * @brief Check whether the current session is running under Wayland.
 *
 * @return @c true if running in a Wayland session, otherwise @c false.
 */
bool PlatformHelper::isWayland()
{
    if (QGuiApplication::instance()) {
        QString platformName = QGuiApplication::platformName().toLower();
        if (platformName.contains("wayland")) {
            return true;
        }
        if (platformName.contains("xcb") || platformName.contains("windows") || platformName.contains("cocoa")) {
            return false;
        }
    }

    QString waylandDisplay = qEnvironmentVariable("WAYLAND_DISPLAY");
    if (!waylandDisplay.isEmpty()) {
        return true;
    }

    QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();
    if (sessionType == "wayland") {
        return true;
    }

    return false;


}

/**
 * @brief A helper function to detect the Desktop Environment
 *
 * Checks @c XDG_CURRENT_DESKTOP, @c DESKTOP_SESSION and
 * @c XDG_SESSION_DESKTOP in order and returns a normalised, lower-cased name.
 *
 * @return Normalized desktop name: "kde", "gnome", "hyprland", "cosmic", "xfce", etc., or "unknown".
 */
QString PlatformHelper::detectDesktopEnvironment()
{
    // 1. Hent den første non-empty env-variabelen etter prioritet
    QString rawDesktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    if (rawDesktop.isEmpty()) {
        rawDesktop = qEnvironmentVariable("DESKTOP_SESSION");
    }
    if (rawDesktop.isEmpty()) {
        rawDesktop = qEnvironmentVariable("XDG_SESSION_DESKTOP");
    }

    if (rawDesktop.isEmpty()) {
        return "unknown";
    }

    QString lowerDesktop = rawDesktop.toLower();

    // 2. Skille ut spesifikke miljøer basert på nøkkelord

    // KDE Plasma
    if (lowerDesktop.contains("kde") || lowerDesktop.contains("plasma")) {
        return "kde";
    }

    // COSMIC (System76) - Må sjekkes FØR gnome/pop pga ny Rust-desktop
    if (lowerDesktop.contains("cosmic")) {
        return "cosmic";
    }

    // GNOME og derivater (Ubuntu, Pop!_OS GTK-era, Budgie, Pantheon)
    if (lowerDesktop.contains("gnome") || lowerDesktop.contains("ubuntu") || 
        lowerDesktop.contains("pop") || lowerDesktop.contains("pantheon") || 
        lowerDesktop.contains("budgie")) {
        return "gnome";
    }

    // Hyprland
    if (lowerDesktop.contains("hyprland")) {
        return "hyprland";
    }

    // Sway
    if (lowerDesktop.contains("sway")) {
        return "sway";
    }

    // XFCE
    if (lowerDesktop.contains("xfce")) {
        return "xfce";
    }

    // LXQt / LXDE
    if (lowerDesktop.contains("lxqt") || lowerDesktop.contains("lxde")) {
        return "lxqt";
    }

    // Cinnamon
    if (lowerDesktop.contains("cinnamon")) {
        return "cinnamon";
    }

    // MATE
    if (lowerDesktop.contains("mate")) {
        return "mate";
    }

    // Deepin (DDE)
    if (lowerDesktop.contains("dde") || lowerDesktop.contains("deepin")) {
        return "dde";
    }

    // Hvis variablene inneholder kolon (f.eks. "X-FOO:BAR"), returner det første leddet
    if (lowerDesktop.contains(':')) {
        return lowerDesktop.split(':').first();
    }

    return lowerDesktop;
}

// ======================= QProcess helper functions for sandbox escapes ==========================/ 

/**
 * @brief Build a command line that runs on the host system.
 *
 * @param program   the program to run.
 * @param arguments optional program arguments.
 * @return the host-ready command line.
 */
QStringList PlatformHelper::makeHostContext(const QString &program, const QStringList &arguments)
{
    QStringList result;
    if (isFlatpak()) {
        // Bruk flatpak-spawn for å kjøre kommandoer på vertssystemet
        result << "flatpak-spawn" << "--host" << program;
        result << arguments;
    } else {
        result << program;
        result << arguments;
    }
    return result;
}

/**
 * @brief Check whether the running Flatpak grants @c flatpak-spawn talk access.
 *
 * @return @c true if the sandbox configuration allows host process spawning.
 */
bool checkHasFlatpakSpawnPrivileges()
{
    if(PlatformHelper::checkFlatpakFeature("Session Bus Policy", "org.freedesktop.Flatpak", "talk"))
        return true;
    QFile f(QStringLiteral("/.flatpak-info"));
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }

    return f.readAll().contains("\norg.freedesktop.Flatpak=talk\n");
}

/**
 * @brief Check for a specific feature or permission in the Flatpak sandbox info file.
 *
 * @param group         the INI section group. 'Application', 'Instance', 'Context', 'Session Bus Policy', 'Environments' and more
 * @param key           the INI key to inspect. 'name',   'branch' ,  'filesystems' ,   'org.freedesktop.Flatpak' , 'XDG_DATA_DIRS'
 * @param expectedValue the value to search for. 'org.davidedmundson.kiot', 'master', '/run/docker.sock', 'talk' , '/usr/share/runtime/share'
 * @return @c true if the value is present in the specified section, otherwise @c false.
 */
/**
 * @brief Check for a specific feature or permission in the Flatpak sandbox info file.
 */
bool PlatformHelper::checkFlatpakFeature(const QString &group, const QString &key, const QString &expectedValue)
{
    QFile file(QStringLiteral("/.flatpak-info"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCDebug(helper) << "Could not open /.flatpak-info file";
        return false;
    }

    QTextStream in(&file);
    QString currentGroup;
    bool inTargetGroup = false;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        // Ignorer tomme linjer og kommentarer (standard for keyfile er # eller ;)
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';'))
            continue;

        // Sjekk etter seksjoner, f.eks. [Application]
        if (line.startsWith('[') && line.endsWith(']')) {
            currentGroup = line.mid(1, line.length() - 2);
            inTargetGroup = (currentGroup == group);
            continue;
        }

        // Hvis vi er i riktig gruppe, let etter nøkkelen
        if (inTargetGroup) {
            int equalIndex = line.indexOf('=');
            if (equalIndex != -1) {
                QString currentKey = line.left(equalIndex).trimmed();
                if (currentKey == key) {
                    QString rawValue = line.mid(equalIndex + 1).trimmed();
                    
                    // Flatpak bruker semikolon som separator (og ofte en avsluttende ';' på slutten)
                    QStringList values = rawValue.split(';', Qt::SkipEmptyParts);
                    
                    qCDebug(helper) << "Flatpak feature check (manual):" << group << key << expectedValue << values;
                    return values.contains(expectedValue);
                }
            }
        }
    }

    qCDebug(helper) << "Group or Key not found in /.flatpak-info:" << group << key;
    return false;
}
/*
ORiginale not working because ; is comments
bool PlatformHelper::checkFlatpakFeature(const QString &group, const QString &key, const QString &expectedValue)
{
    QSettings info(QStringLiteral("/.flatpak-info"), QSettings::IniFormat);
    if (!info.childGroups().contains(group)) {
    // Group exists
        qCDebug(helper) << "Group:" << group << "does not exist in the flatpak-info file";
        qCDebug(helper) << "Flatpak groups found:" << info.childGroups(); 
        return false;
    }
    info.beginGroup(group);
    if(!info.childKeys().contains(key)) {
        qCDebug(helper) << "Key:" << key << "does not exist in the flatpak-info file";
        qCDebug(helper) << "Flat keys found:" << info.childKeys();
        return false;
    }
    // Hent strengen fra seksjonen (f.eks. "Session Bus Policy/org.freedesktop.Flatpak")
    auto rawValue = info.value(key);
    qCDebug(helper) << "Flatpak feature check:" << group << key << expectedValue << rawValue;
    QString ll = rawValue.toString();
    // Del opp på semikolon og kommategn hvis det er en liste
    QStringList values = ll.split(";");
    qCDebug(helper) << "Flatpak feature check:" << group << key << expectedValue << values;
    return values.contains(expectedValue);
}
*/

/**
 * @brief Build a host-ready @ref PlatformHelper::ProcessContext from a QProcess.
 *
 * @param process the source process definition.
 * @return the resulting host context.
 */
PlatformHelper::ProcessContext PlatformHelper::makeHostContext(QProcess &process)
{
    if (!isFlatpak()) {
        return {process.program(), process.arguments()};
    }
    static const bool hasFlatpakSpawnPrivileges = checkHasFlatpakSpawnPrivileges();
    if (!hasFlatpakSpawnPrivileges) {
        qCWarning(helper) << "Process execution expects 'org.freedesktop.Flatpak=talk'" << process.program();
        return {process.program(), process.arguments()};
    }
    QStringList args{QStringLiteral("--watch-bus"), QStringLiteral("--host"), QStringLiteral("--forward-fd=1"), QStringLiteral("--forward-fd=2")};
    if (!process.workingDirectory().isEmpty()) {
        args << QStringLiteral("--directory=%1").arg(process.workingDirectory());
    }
    const auto systemEnvironment = QProcessEnvironment::systemEnvironment().toStringList();
    const auto processEnvironment = process.processEnvironment().toStringList();
    for (const auto &variable : processEnvironment) {
        if (systemEnvironment.contains(variable)) {
            continue;
        }
        args << QStringLiteral("--env=%1").arg(variable);
    }
    if (!process.program().isEmpty()) { // some callers are cheeky and pass no program but put it into the arguments (e.g. konsole)
        args << process.program();
    }
    args += process.arguments();
    return {QStringLiteral("/usr/bin/flatpak-spawn"), args};
}

/**
 * @brief Start a process on the host system.
 *
 * @param process the process to start.
 * @param mode    whether to start synchronously or detached.
 */
void PlatformHelper::startHostProcess(QProcess &process, PlatformHelper::ProcessMode mode)
{
    const auto context = makeHostContext(process);
    switch (mode) {
        case PlatformHelper::ProcessMode::start:
            process.start(context.program, context.arguments);
            return;
        case PlatformHelper::ProcessMode::startDetached:
            process.startDetached(context.program, context.arguments);
            return;
       
    }

}


} // namespace KIOTShared