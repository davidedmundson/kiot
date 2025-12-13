// SPDX-FileCopyrightText: 2025 Odd Østlie <theoddpirate@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file docker.cpp
 * @brief Docker integration for Home Assistant control
 * 
 * Provides real-time monitoring and control of Docker containers
 * through Home Assistant switches and event listening.
 */
 
#include "core.h"
#include "entities/entities.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QThread>
#include <QVariantMap>

#include <atomic>

/**
 * Manages Docker container events by listening to Docker socket
 * 
 * @details Runs in a separate thread to capture container state changes
 *          without blocking the main application thread
 */
class DockerEventListener : public QThread
{
    Q_OBJECT
    
public:
    explicit DockerEventListener(QObject *parent = nullptr)
        : QThread(parent) {}
    
    void stop() { 
        m_stop = true;
        // Interrupt any blocking read operations
        quit();
    }
    
signals:
    void containerEvent(const QString &name, const QVariantMap &attrs);

protected:
    void run() override {
        QLocalSocket socket;
        socket.connectToServer("/var/run/docker.sock", QIODevice::ReadWrite);
        if (!socket.waitForConnected(1000)) {
            qWarning() << "[docker] Failed to connect to Docker socket";
            return;
        }

        const QByteArray request = "GET /events HTTP/1.0\r\nHost: localhost\r\n\r\n";
        if (socket.write(request) != request.size()) {
            qWarning() << "[docker] Failed to write request to socket";
            return;
        }
        socket.flush();

        qDebug() << "[docker] Event listener started";
        
        while (!m_stop && socket.state() == QLocalSocket::ConnectedState) {
            if (!socket.waitForReadyRead(1000)) {
                // Timeout is expected, continue if not stopping
                continue;
            }

            const QByteArray line = socket.readLine().trimmed();
            if (line.isEmpty()) continue;

            if (!processEventLine(line)) continue;
        }

        socket.disconnectFromServer();
        qDebug() << "[docker] Event listener stopped";
    }

private:
    bool processEventLine(const QByteArray &line) {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            //qWarning() << "[docker] JSON parse error:" << parseError.errorString();
            return false;
        }
        
        if (!doc.isObject()) return false;

        const QJsonObject obj = doc.object();
        if (obj.value("Type").toString() != "container") return false;

        const QJsonObject actor = obj.value("Actor").toObject();
        const QJsonObject attrsObj = actor.value("Attributes").toObject();
        const QString name = attrsObj.value("name").toString();
        
        if (name.isEmpty()) return false;

        QVariantMap attrs;
        attrs["status"] = obj.value("status").toString();
        attrs["id"] = obj.value("id").toString();
        attrs["image"] = attrsObj.value("image").toString();

        emit containerEvent(name, attrs);
        return true;
    }

private:
    std::atomic<bool> m_stop{false};
};


/**
 * Main Docker integration class for Home Assistant
 * 
 * @details Creates switches for Docker containers and provides
 *          real-time state management and event handling
 */
class DockerSwitch : public QObject
{
    Q_OBJECT
    
public:
    explicit DockerSwitch(QObject *parent = nullptr)
        : QObject(parent) 
    {
        if (!isDockerAvailable()) {
            qWarning() << "[docker] Docker socket not available at: " << DOCKER_SOCKET_PATH << " stopping integration";
            return;
        }

        if (!ensureConfigDefaults()) {
            qWarning() << "[docker] Failed to initialize configuration";
            return;
        }

        initializeSwitches();
        startEventListener();
        
        qDebug() << "[docker] Integration initialized with" << m_containers.size() << "containers";
    }
    ~DockerSwitch() override
    {
        stopEventListener();
    }
private:
    struct ContainerInfo { 
        QString name; 
        Switch *sw; 
    };
    
    QList<ContainerInfo> m_containers;
    DockerEventListener *m_listener = nullptr;
    
    static constexpr int SOCKET_TIMEOUT_MS = 5000;
    static constexpr const char* DOCKER_SOCKET_PATH = "/var/run/docker.sock";
    
    /**
     * Checks if Docker socket is accessible
     * 
     * @return bool True if Docker socket can be connected, false otherwise
     * @brief Verifies Docker daemon connectivity
     */
    bool isDockerAvailable() const {
        QLocalSocket testSocket;
        testSocket.connectToServer(DOCKER_SOCKET_PATH, QIODevice::ReadWrite);
        const bool available = testSocket.waitForConnected(1000);
        if (available) {
            testSocket.disconnectFromServer();
        }
        return available;
    }
    
    /**
     * Initializes switches for Docker containers based on configuration
     * 
     * Process:
     * 1. Retrieves Docker group from configuration
     * 2. Creates switch for each enabled container
     * 3. Sets initial state and attributes for each switch
     * 
     * @brief Sets up Home Assistant switches for manageable Docker containers
     */
    void initializeSwitches() {
        const auto cfg = KSharedConfig::openConfig();
        const KConfigGroup grp = cfg->group("docker");

        // Create switches for enabled containers
        for (const auto &key : grp.keyList()) {
            if (!grp.readEntry(key, false)) continue;

            qDebug() << "[docker] Enabling control for container" << key;
            createContainerSwitch(key);
        }
    }
    
    /**
     * Creates a single switch for a specific Docker container
     * 
     * @param name - Name of the Docker container
     * @brief Configures a switch with Docker-specific ID and icon
     * @note Connects switch to container start/stop functionality
     */
    void createContainerSwitch(const QString &name) {
        auto *sw = new Switch(this);
        sw->setId("docker_" + name);
        sw->setName(name);
        sw->setDiscoveryConfig("icon", "mdi:docker");

        // Initial state + attributes
        updateSwitch(name, sw);

        connect(sw, &Switch::stateChangeRequested, this, [this, name](bool state){
            toggleContainer(name, state);
        });

        m_containers.append({name, sw});
    }

    /**
    * Starts event listener for Docker container events
    * 
    * Sets up a separate thread that listens to Docker socket
    * for real-time container status updates
    * 
    * @brief Enables live monitoring of container state changes
    */
    void startEventListener() {
        m_listener = new DockerEventListener(this);
        connect(m_listener, &DockerEventListener::containerEvent,
                this, &DockerSwitch::handleEvent, Qt::QueuedConnection);
        m_listener->start();
    }
    
    void stopEventListener() {
        if (!m_listener) return;
        
        m_listener->stop();
        if (!m_listener->wait(3000)) {
            qWarning() << "[docker] Event listener did not stop gracefully, terminating";
            m_listener->terminate(); // terminate() is a last resort to avoid hanging shutdown
            m_listener->wait(1000);
        }
    }
    
    /**
     * Synchronizes configuration with current Docker containers
     * 
     * @details 
     * - Adds new containers to configuration
     * - Removes containers no longer existing
     * - Ensures configuration reflects current system state
     * 
     * @return bool True if configuration was successfully updated
    */
    bool ensureConfigDefaults() {
        const auto cfg = KSharedConfig::openConfig();
        KConfigGroup grp = cfg->group("docker");
        
        const QStringList currentContainers = listAllContainers();
        if (currentContainers.isEmpty()) {
            qWarning() << "[docker] No containers found";
            return false;
        }
        
        // Get existing config entries
        const QStringList configContainers = grp.keyList();
        bool configChanged = false;
        
        // Add new containers that aren't in config yet (default to false)
        for (const QString &containerName : currentContainers) {
            if (!grp.hasKey(containerName)) {
                grp.writeEntry(containerName, false);
                configChanged = true;
                qDebug() << "[docker] Added new container to config:" << containerName;
            }
                }
        
        // Remove containers from config that no longer exist
        for (const QString &configContainer : configContainers) {
            if (!currentContainers.contains(configContainer)) {
                grp.deleteEntry(configContainer);
                configChanged = true;
                qDebug() << "[docker] Removed unavailable container from config:" << configContainer;
            }
        }
        
        if (configChanged) {
            cfg->sync();
            qDebug() << "[docker] Configuration updated with current containers";
        }
        
        return true;
    }
   
    /**
    * Performs a call to Docker socket to retrieve information
    * 
    * @param request - HTTP request to Docker API
    * @param response - Reference where response is stored
    * @return bool - true if call was successful
    *   
    * @brief Handles socket connection, request sending, and response receiving
    * @note Includes timeout mechanism for robustness
    */
    bool callDockerSocket(const QByteArray &request, QByteArray &response) {
        QLocalSocket socket;
        socket.connectToServer(DOCKER_SOCKET_PATH, QIODevice::ReadWrite);
        if (!socket.waitForConnected(1000)) {
            qWarning() << "[docker] Failed to connect to Docker socket";
            return false;
        }

        if (socket.write(request) != request.size()) {
            qWarning() << "[docker] Failed to write request to socket";
            return false;
        }
        
        socket.flush();
        if (!socket.waitForReadyRead(SOCKET_TIMEOUT_MS)) {
            qWarning() << "[docker] Timeout waiting for response";
            return false;
        }

        response = socket.readAll();
        socket.disconnectFromServer();
        return true;
    }
    
    /**
     * Retrieves list of all Docker containers on the system
     * 
     * @return QStringList Container names, including stopped containers
     * @brief Discovers all available Docker containers
     */
    QStringList listAllContainers() {
        QStringList names;
        QByteArray response;
        
        const QByteArray request = "GET /containers/json?all=1 HTTP/1.0\r\n\r\n";
        if (!callDockerSocket(request, response)) {
            return names;
        }
        
        const QByteArray body = extractHttpBody(response);
        if (body.isEmpty()) return names;
        
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isArray()) {
            qWarning() << "[docker] Unexpected response format for container list";
            return names;
        }

        for (const auto &value : doc.array()) {
            if (!value.isObject()) continue;
            
            const QJsonArray namesArray = value.toObject()["Names"].toArray();
            if (namesArray.isEmpty()) continue;
            
            QString name = namesArray.first().toString();
            if (name.startsWith("/")) {
                name.remove(0, 1);
            }
            
            if (!name.isEmpty()) {
                names.append(name);
            }
        }
        return names;
    }
    
    /**
     * Checks if a specific container is currently running
     * 
     * @param name Name of the container to check
     * @return bool True if container is running, false otherwise
     */
    bool isRunning(const QString &name) {
        QByteArray response;
        const QByteArray request = "GET /containers/json?all=0 HTTP/1.0\r\n\r\n";
        if (!callDockerSocket(request, response)) {
            return false;
        }
        
        const QByteArray body = extractHttpBody(response);
        if (body.isEmpty()) return false;
        
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isArray()) return false;

        for (const auto &value : doc.array()) {
            if (!value.isObject()) continue;
            
            const QJsonArray namesArray = value.toObject()["Names"].toArray();
            if (namesArray.isEmpty()) continue;
            
            QString containerName = namesArray.first().toString();
            if (containerName.startsWith("/")) {
                containerName.remove(0, 1);
            }
            
            if (containerName == name) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * Extracts HTTP response body from Docker socket response
     * 
     * @param response Full HTTP response
     * @return QByteArray Extracted response body
     * @brief Parses HTTP responses from Docker API
     */
    QByteArray extractHttpBody(const QByteArray &response) {
        const int headerEnd = response.indexOf("\r\n\r\n");
        if (headerEnd == -1) {
            qWarning() << "[docker] Invalid HTTP response format";
            return QByteArray();
        }
        return response.mid(headerEnd + 4);
    }
    
    /**
     * Starts or stops a specific Docker container
     * 
     * @param name Name of the container to toggle
     * @param start True to start, false to stop the container
     * @brief Provides start/stop functionality for Docker containers
     */
    void toggleContainer(const QString &name, bool start) {
        const QString action = start ? "start" : "stop";
        const QByteArray request = QString("POST /containers/%1/%2 HTTP/1.0\r\n\r\n")
                                  .arg(name, action).toUtf8();
        
        QByteArray response;
        if (!callDockerSocket(request, response)) {
            qWarning() << "[docker] Failed to" << action << "container" << name;
            return;
        }
        
        qDebug() << "[docker] Container" << name << (start ? "started" : "stopped");
        
        // Update the specific switch
        for (auto &containerInfo : m_containers) {
            if (containerInfo.name == name) {
                updateSwitch(name, containerInfo.sw);
                break;
            }
        }
    }
    
    /**
     * Updates Home Assistant switch with current container state and attributes
     * 
     * @param name Container name
     * @param sw Pointer to Switch entity to update
     * @brief Synchronizes switch state with actual container status
     */
    void updateSwitch(const QString &name, Switch *sw) {
        const bool running = isRunning(name);
        sw->setState(running);

        // Get detailed container information
        QByteArray response;
        const QByteArray request = QString("GET /containers/%1/json HTTP/1.0\r\n\r\n")
                                  .arg(name).toUtf8();
        
        if (!callDockerSocket(request, response)) {
            qWarning() << "[docker] Failed to get container details for" << name;
            return;
        }
        
        const QByteArray body = extractHttpBody(response);
        if (body.isEmpty()) return;
        
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            qWarning() << "[docker] Invalid container details response for" << name;
            return;
        }
        
        const QJsonObject containerObj = doc.object();
        const QJsonObject config = containerObj["Config"].toObject();
        const QJsonObject state = containerObj["State"].toObject();
        const QJsonObject networkSettings = containerObj["NetworkSettings"].toObject();

        QVariantMap attributes;
        attributes["image"] = config["Image"].toString();
        attributes["status"] = state["Status"].toString();
        attributes["running"] = QVariant(state["Running"].toBool()).toString(); 
        attributes["created"] = containerObj["Created"].toString();
        attributes["ports"] = networkSettings["Ports"].toVariant();
        
        sw->setAttributes(attributes);
    }

private slots:
    void handleEvent(const QString &name, const QVariantMap &attrs) {
        Q_UNUSED(attrs) // Currently not using event attributes, but available for future use
        
        // Find and update the corresponding switch
        for (auto &containerInfo : m_containers) {
            if (containerInfo.name == name) {
                updateSwitch(name, containerInfo.sw);
                break;
            }
        }
    }
};
/**
 * Initializes Docker integration for Home Assistant
 * 
 * @brief Creates DockerSwitch instance to manage Docker container switches
 */
void setupDockerSwitch() {
    new DockerSwitch(qApp);
}

REGISTER_INTEGRATION("Docker", setupDockerSwitch, false)
#include "docker.moc"
