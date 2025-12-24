#include "core.h"
#include "entities/update.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <KConfigGroup>
#include <KProcess>
#include <KSandbox>
#include <KSharedConfig>

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(auf)
Q_LOGGING_CATEGORY(auf, "integration.Updater-Flatpak")

class FlatpakUpdater : public QObject
{
    Q_OBJECT
public:
    explicit FlatpakUpdater(QObject *parent = nullptr)
        : QObject(parent)
        , m_updateTimer(new QTimer(this))
    {
        m_updater = new Update(this);
        m_updater->setName("KIOT Flatpak Updater");
        m_updater->setId("flatpak_updates");
        m_updater->setInstalledVersion(QStringLiteral(KIOT_VERSION));
        m_updater->setUpdatePercentage(-1);

        // Connects the update entity to the update function
        connect(m_updater, &Update::installRequested, this, &FlatpakUpdater::update);

        // Reads the config file to get the timestamp for last time we checked for å update
        config = KSharedConfig::openConfig();
        updaterGroup = config->group("Updater");
        lastCheck = updaterGroup.readEntry("LastCheck", QDateTime());

        // Timer to check for updates every 3 hours, will only check once every 24 hours
        m_updateTimer->setInterval(1000 * 60 * 60 * 3); // 3 hours
        connect(m_updateTimer, &QTimer::timeout, this, &FlatpakUpdater::checkForUpdates);

        // Grabs the latest release data from github
        lastRepoData = fetchLatestRelease(repo_url);
        // Sets the update entity to latest release info
        m_updater->setLatestVersion(lastRepoData.value("tag_name", QStringLiteral(KIOT_VERSION)).toString());
        m_updater->setReleaseSummary(lastRepoData.value("body", "No release summary found").toString());
        m_updater->setTitle(lastRepoData.value("name", "kiot").toString());
        m_updater->setReleaseUrl(lastRepoData.value("html_url", repo_url).toString());

        // Update config with current time
        updaterGroup.writeEntry("LastCheck", QDateTime::currentDateTimeUtc());
        config->sync();
    }

    ~FlatpakUpdater()
    {
    }

    void update()
    {
        QString download_path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(download_path); // Sørg for at mappen eksisterer

        // Hent download URL fra GitHub API
        QVariantList assets = lastRepoData.value("assets").toList();
        QString downloadUrl = "failed";
        QString filename;

        if (!assets.isEmpty()) {
            QVariantMap firstAsset = assets.first().toMap();
            downloadUrl = firstAsset.value("browser_download_url", "failed").toString();
            filename = firstAsset.value("name", "kiot-update.flatpak").toString();
        }

        if (downloadUrl == "failed") {
            qCWarning(auf) << "Failed to get download URL from GitHub release";
            return;
        }
        m_updater->setInProgress(true);
        QString fullFilePath = QDir(download_path).filePath(filename);
        qCDebug(auf) << "Downloading update to:" << fullFilePath;

        // Download file
        QNetworkAccessManager mgr;
        QEventLoop loop;
        QNetworkReply *reply = mgr.get(QNetworkRequest(QUrl(downloadUrl)));
        QObject::connect(reply, &QNetworkReply::downloadProgress, [this](qint64 bytesReceived, qint64 bytesTotal) {
            if (bytesTotal > 0) {
                int percent = static_cast<int>((bytesReceived * 100) / bytesTotal);
                m_updater->setUpdatePercentage(percent);
            }
        });
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(auf) << "Download failed:" << reply->errorString();
            reply->deleteLater();
            return;
        }

        QFile file(fullFilePath);
        if (!file.open(QIODevice::WriteOnly)) {
            qCWarning(auf) << "Failed to open file for writing:" << fullFilePath;
            reply->deleteLater();
            return;
        }
        file.write(reply->readAll());
        file.close();
        reply->deleteLater();

        // Installer Flatpak på host
        QStringList installArgs = QProcess::splitCommand(QString("flatpak install -y --user \"%1\"").arg(fullFilePath));
        QString installProgram = installArgs.takeFirst();
        KProcess *installProc = new KProcess();
        installProc->setProgram(installProgram);
        installProc->setArguments(installArgs);
        KSandbox::ProcessContext ctxInstall = KSandbox::makeHostContext(*installProc);
        installProc->setProgram(ctxInstall.program);
        installProc->setArguments(ctxInstall.arguments);
        installProc->execute();
        delete installProc;

        m_updater->setUpdatePercentage(-1);
        m_updater->setInProgress(false);

        // Removes the file after we installed the update
        if (QFile(fullFilePath).exists())
            QFile(fullFilePath).remove();

        // Starts a new instance of kiot in flatpak to replace our old one
        QStringList runArgs = QProcess::splitCommand(QString("flatpak run org.davidedmundson.kiot"));
        QString runProgram = runArgs.takeFirst();
        KProcess *runProc = new KProcess();
        runProc->setProgram(runProgram);
        runProc->setArguments(runArgs);
        KSandbox::ProcessContext ctxRun = KSandbox::makeHostContext(*runProc);
        runProc->setProgram(ctxRun.program);
        runProc->setArguments(ctxRun.arguments);
        qCDebug(auf) << "Update completed, restarting kiot";
        runProc->startDetached();
        delete runProc;
    }

    void checkForUpdates()
    {
        lastCheck = updaterGroup.readEntry("LastCheck", QDateTime());

        // Check to be sure we dont spam the github api
        int time = 86400; // 24 hours
        if (lastCheck.isValid() && lastCheck.secsTo(QDateTime::currentDateTimeUtc()) < time)
            return;
        qCDebug(auf) << "Checking for updates";
        lastRepoData = fetchLatestRelease(repo_url);
        m_updater->setLatestVersion(lastRepoData.value("tag_name", QStringLiteral(KIOT_VERSION)).toString());
        m_updater->setReleaseSummary(lastRepoData.value("body", "No release summary found").toString());
        m_updater->setTitle(lastRepoData.value("name", "kiot").toString());
        m_updater->setReleaseUrl(lastRepoData.value("html_url", repo_url).toString());

        updaterGroup.writeEntry("LastCheck", QDateTime::currentDateTimeUtc());
        config->sync();
    }

    // Grabs latest release info from github so we can check if there is a new release
    QVariantMap fetchLatestRelease(const QString &repoUrl)
    {
        QNetworkAccessManager manager;

        QRegularExpression re(R"(github\.com/([^/]+)/([^/]+))");
        auto match = re.match(repoUrl);
        if (!match.hasMatch())
            return {};

        const QString owner = match.captured(1);
        const QString repo = match.captured(2);

        const QUrl apiUrl(QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest").arg(owner, repo));

        QNetworkRequest request(apiUrl);
        request.setHeader(QNetworkRequest::UserAgentHeader, "Kiot-Updater");

        QNetworkReply *reply = manager.get(request);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            return {};
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();

        if (!doc.isObject())
            return {};

        const QJsonObject obj = doc.object();

        QVariantMap result;
        result["tag_name"] = obj.value("tag_name").toString();
        result["name"] = obj.value("name").toString();
        result["published_at"] = obj.value("published_at").toString();
        result["html_url"] = obj.value("html_url").toString();
        result["body"] = obj.value("body").toString();

        QVariantList assetsList;
        const QJsonArray assets = obj.value("assets").toArray();

        for (const QJsonValue &val : assets) {
            const QJsonObject assetObj = val.toObject();

            QVariantMap asset;
            asset["name"] = assetObj.value("name").toString();
            asset["size"] = assetObj.value("size").toInt();
            asset["content_type"] = assetObj.value("content_type").toString();
            asset["browser_download_url"] = assetObj.value("browser_download_url").toString();
            asset["download_count"] = assetObj.value("download_count").toInt();

            assetsList.append(asset);
        }

        result["assets"] = assetsList;

        return result;
    }

private:
    KSharedConfig::Ptr config;
    KConfigGroup updaterGroup;
    QDateTime lastCheck;
    // TODO change to upstream repo
    QString repo_url = "https://github.com/davidedmundson/kiot";
    QVariantMap lastRepoData;
    QTimer *m_updateTimer = nullptr;
    Update *m_updater;
};

void setupFlatpakUpdater()
{
    if (!KSandbox::isFlatpak()) {
        qCWarning(auf) << "FlatpakUpdater is only supported in Flatpak environments,aborting";
        return;
    }
    new FlatpakUpdater(qApp);
}

REGISTER_INTEGRATION("UpdaterFlatpak", setupFlatpakUpdater, true)
#include "updater_flatpak.moc"