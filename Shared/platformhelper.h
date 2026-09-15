#pragma once
#include <KSandbox>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QLoggingCategory>
#include <qloggingcategory.h>
/**
 * @file platformhelper.h
 * @brief Platform-specific helpers for runtime environment detection.
 *
 * @brief Infrastructure to detect the current platform, desktop environment,
 * packaging format (Flatpak/Snap) and various system paths, as well as helpers
 * to run processes on the host system when launched inside a Flatpak sandbox.
 *
 * @author Odd Østlie &lt;theoddpirate@gmail.com&gt;
 * @since 0.1
 */

/**
 * @brief Stateless helpers for platform and process detection.
 *
 * The class is designed as a pure collection of static functions and cannot
 * be instantiated: both the constructor and the destructor are deleted.
 */
class PlatformHelper
{
public:


    /**
     * @brief Supported operating system platforms.
     */
    enum Platform {
        Windows,  ///< Microsoft Windows
        Linux,    ///< Linux
        macOS,    ///< Apple macOS
        Android,  ///< Google Android
        iOS,      ///< Apple iOS
        Unknown   ///< Any other/undetected platform
    };

    /**
     * @brief Supported CPU architectures.
     */
    enum Architecture {
        X86,          ///< 32-bit x86
        X86_64,       ///< 64-bit x86_64
        ARM,          ///< 32-bit ARM
        ARM64,        ///< 64-bit ARM (AArch64)
        UnknownArch   ///< Any other/undetected architecture
    };



    /**
     * @brief Detect the current operating system platform.
     *
     * @return a @ref Platform value based on compile-time macros.
     */
    static Platform currentPlatform();

    /**
     * @brief Detect the current CPU architecture.
     *
     * @return an @ref Architecture value based on @c QSysInfo.
     */
    static Architecture currentArchitecture();

    /**
     * @brief Check whether the application is running inside a Flatpak sandbox.
     *
     * @return @c true if any Flatpak environment marker is present.
     */
    static bool isFlatpak();

    /**
     * @brief Check whether the application is running inside a Snap confinement.
     *
     * @return @c true if the @c SNAP environment variable is set.
     */
    static bool isSnap();

    /**
     * @brief Path of the autostart directory for the current platform.
     *
     * @return the directory used for autostart entries, or an empty string if
     *         the current platform does not support autostart discovery.
     */
    static QString autostartPath();

    /**
     * @brief Path of this application's configuration file.
     *
     * @return the @c <PROJECT_NAME>rc file inside the platform configuration
     *         directory (e.g. @c ~/.config/odd-macrosrc on Linux).
     */
    static QString configFilePath(const QString &fileType="rs");
    /**
     * @brief Path of this application's configuration Directory path.
     *
     * @return the @c <PROJECT_NAME> dir inside the platform configuration
     *         directory (e.g. @c ~/.config/kiot_qt on Linux).
     */
    static QString configDirPath();
    /**
     * @brief Human-readable name of the current platform.
     *
     * @return e.g. @c "Windows", @c "Linux", @c "macOS", ...
     */
    static QString platformName();

    /**
     * @brief Generate a reverse-DNS style service name.
     *
     * Reverses the organisation domain and appends the project name, e.g.
     * <tt>com.theoddpirate.odd-macros</tt>.
     *
     * @return the generated service name.
     */
    static QString generateServiceName();

    /**
     * @brief Normalise a user supplied domain string into a host name.
     *
     * @param input the raw domain string (may contain a scheme or be bare).
     * @return a lower-cased host name, or @c "theoddpirate.com" on failure.
     */
    static QString resolveOrganizationDomain(const QString &input);

    /**
     * @brief The full build ABI of the Qt runtime.
     *
     * @return value from @c QSysInfo::buildAbi().
     */
    static QString buildAbi();

    /**
     * @brief The version string of the running operating system kernel.
     *
     * @return value from @c QSysInfo::kernelVersion().
     */
    static QString kernelVersion();

    /**
     * @brief A human-readable product name of the operating system.
     *
     * @return value from @c QSysInfo::prettyProductName().
     */
    static QString productName();

    /**
     * @brief Check whether the current session is running under Wayland.
     *
     * Inspects the active @c QGuiApplication platform plugin name, as well
     * as the @c WAYLAND_DISPLAY and @c XDG_SESSION_TYPE environment variables.
     *
     * @return @c true if running in a Wayland session, otherwise @c false.
     */
    static bool isWayland();
    
    /**
     * @brief Detect the running desktop environment.
     *
     * Checks the @c XDG_CURRENT_DESKTOP, @c DESKTOP_SESSION and
     * @c XDG_SESSION_DESKTOP environment variables in that order and returns a
     * normalised name.
     *
     * @return @c "kde", @c "gnome", @c "hyprland", @c "cosmic", the raw desktop name, or @c "unknown".
     */
    static QString detectDesktopEnvironment();

    /**
     * @brief Check for a specific feature or permission in the Flatpak sandbox info file.
     *
     * Parses @c /.flatpak-info using INI format and checks if the specified @p key
     * in @p group contains the @p expectedValue in its semicolon-separated list.
     *
     * @param group         the INI section group (e.g. @c "Session Bus Policy" or @c "Context").
     * @param key           the INI key to inspect (e.g. @c "org.freedesktop.Flatpak" or @c "filesystems").
     * @param expectedValue the value to search for within the key's list (e.g. @c "talk" or @c "host").
     * @return @c true if the value is present in the specified section, otherwise @c false.
     */
    static bool checkFlatpakFeature(const QString &group, const QString &key, const QString &expectedValue);
    
    /**
     * @brief Build a command line that runs on the host system.
     *
     * When running inside a Flatpak sandbox the returned list wraps
     * @p program with @c flatpak-spawn --host; otherwise it is returned
     * unmodified.
     *
     * @param program   the program to run on the host.
     * @param arguments optional arguments for @p program.
     * @return the host-ready command line.
     */
    static QStringList makeHostContext(const QString &program, const QStringList &arguments = QStringList());

    /**
     * @brief Build a host-ready @ref ProcessContext from a QProcess.
     *
     * Translates @p process into a form that can be executed on the host
     * system, forwarding stdout/stderr and environment variables as needed
     * when running inside a Flatpak with @c flatpak-spawn.
     *
     * @param process the source process definition.
     * @return the resulting host context.
     */
    static KSandbox::ProcessContext makeHostContext(QProcess &process);
    
    /**
     * @brief Start a process on the host system.
     *
     * @param process the process to start (its arguments are rewritten for
     *                host execution as needed).
     * @param mode    whether to start synchronously or detached.
     */
    static void startHostProcess(QProcess &process, QProcess::OpenMode mode = QProcess::ReadWrite);

private:
    /**
     * @brief Partially deleted constructor.
     *
     * PlatformHelper is an all-static helper and cannot be instantiated.
     */
    PlatformHelper() = delete;

    /**
     * @brief Partially deleted destructor.
     *
     * PlatformHelper is an all-static helper and cannot be instantiated.
     */
    ~PlatformHelper() = delete;
};




// Enkel makro for å deklarere en kategori i en header (.h)
#define DECLARE_LOGGER(Name) Q_DECLARE_LOGGING_CATEGORY(Name)

// Macro for creating a log category matching project_name from cmakelists
#define LOG_CAT(suffix) PROJECT_NAME "." #suffix
// Enkel makro for å definere kategorien i en .cpp-fil med automatisk prosjekt-prefix
#define DEFINE_LOGGER(Name, SubCategory) \
        DECLARE_LOGGER(Name); \
        Q_LOGGING_CATEGORY(Name, LOG_CAT(SubCategory))


// clang-format on
