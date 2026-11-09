#pragma once

#include <QObject>
#include <QMqttClient>
#include <QTimer>

#include "KIOTShared/kiotshared_export.h"

namespace KIOTShared {
namespace Transport {

class KIOT_SHARED_EXPORT TransportManager : public QObject
{
    Q_OBJECT

public:
    explicit TransportManager(QObject *parent = nullptr);
    ~TransportManager() override;
    
    // Sikker metode for at plugins/andre moddelar kan hente referanse til klienten
    static QMqttClient *mqttClient()
    {
        if(s_self && s_self->m_client)
            return s_self->m_client;
        return nullptr;
    }
    void doConnect();
    void doDisconnect();

Q_SIGNALS:
    void connectionStateChanged(QMqttClient::ClientState state);
    void mqttConfigMissing();

public Q_SLOTS:
    void handleStateChanged(QMqttClient::ClientState state);

private:
    static TransportManager *s_self;
    QMqttClient *m_client;
    QTimer *reconnectTimer;
    bool m_passwordRead = false;
    bool m_connectRequested = false;
    bool initiateMqttClient();
};

} // namespace Transport
} // namespace KIOTShared