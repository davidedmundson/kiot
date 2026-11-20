import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts

ColumnLayout {
    id: root
    anchors.fill: parent
    anchors.margins: 16 // Gir en fin, luftig ramme rundt hele panelet
    spacing: 12

    property var settingsManager: null

    // ---- Top branding header (Mer elegant og luftig) ----
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 4

        QQC2.Label {
            text: "KDE Internet of Things"
            font.bold: true
            font.pixelSize: 22
        }

        QQC2.Label {
            text: "MQTT Connection"
            font.bold: true
            font.pixelSize: 14
            opacity: 0.7 // Gir en fin dempet effekt på undertittelen
        }
    }

    // Liten avstand før skjemaet starter
    Item { Layout.preferredHeight: 4 }

    GridLayout {
        columns: 2
        columnSpacing: 12
        rowSpacing: 12
        Layout.fillWidth: true

        QQC2.Label { text: "Hostname:"; Layout.alignment: Qt.AlignRight }
        QQC2.TextField {
            id: hostField
            Layout.fillWidth: true
            text: root.settingsManager ? root.settingsManager.getHost() : ""
            onEditingFinished: if (root.settingsManager) root.settingsManager.setHost(text)
        }

        QQC2.Label { text: "Port:"; Layout.alignment: Qt.AlignRight }
        QQC2.TextField {
            id: portField
            Layout.fillWidth: true
            text: root.settingsManager ? root.settingsManager.getPort() : 1883
            onEditingFinished: if (root.settingsManager) root.settingsManager.setPort(parseInt(text) || 1883)
        }

        QQC2.Label { text: "Username:"; Layout.alignment: Qt.AlignRight }
        QQC2.TextField {
            id: userField
            Layout.fillWidth: true
            text: root.settingsManager ? root.settingsManager.getUser() : ""
            onEditingFinished: if (root.settingsManager) root.settingsManager.setUser(text)
        }

        QQC2.Label { text: "Password:"; Layout.alignment: Qt.AlignRight }
        QQC2.TextField {
            id: passwordField
            Layout.fillWidth: true
            text: root.settingsManager ? root.settingsManager.password : ""
            echoMode: TextInput.Password
            onEditingFinished: if (root.settingsManager) root.settingsManager.password = text
        }

        QQC2.Label { text: "Discovery Prefix:"; Layout.alignment: Qt.AlignRight }
        QQC2.TextField {
            id: discoveryField
            Layout.fillWidth: true
            text: root.settingsManager ? root.settingsManager.getDiscoveryPrefix() : "homeassistant"
            onEditingFinished: if (root.settingsManager) root.settingsManager.setDiscoveryPrefix(text)
        }

        QQC2.Label { text: "Use TLS:"; Layout.alignment: Qt.AlignRight }
        QQC2.CheckBox {
            id: sslCheckbox
            checked: root.settingsManager ? root.settingsManager.getConfigValue("general", "tls", false) : false
            onToggled: if (root.settingsManager) root.settingsManager.saveConfigValue("general", "tls", checked)
        }

        QQC2.Label { text: "Show system tray:"; Layout.alignment: Qt.AlignRight }
        QQC2.CheckBox {
            id: systrayCheckbox
            checked: root.settingsManager ? root.settingsManager.getConfigValue("general", "systray", true) : true
            onToggled: if (root.settingsManager) root.settingsManager.saveConfigValue("general", "systray", checked)
        }

        QQC2.Label { text: "Autostart:"; Layout.alignment: Qt.AlignRight }
        QQC2.CheckBox {
            id: autostartCheckbox
            checked: root.settingsManager ? root.settingsManager.getConfigValue("general", "autostart", false) : false
            onToggled: if (root.settingsManager) root.settingsManager.saveConfigValue("general", "autostart", checked)
        }
    }

    Item { 
        Layout.fillHeight: true 
        Layout.fillWidth: true
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 8
        Layout.bottomMargin: 4
        spacing: 16

        QQC2.Label {
            text: '<a href="https://kde.org/community/donations">Donate to KDE</a>'
            onLinkActivated: (link) => Qt.openUrlExternally(link)
            
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.NoButton
            }
        }

        Item { Layout.fillWidth: true } 

        QQC2.Label {
            text: '<a href="https://github.com/davidedmundson/kiot">GitHub Repository</a>'
            onLinkActivated: (link) => Qt.openUrlExternally(link)
            
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.NoButton
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        QQC2.Button {
            text: "Apply"
            Layout.fillWidth: true
            onClicked: if (root.settingsManager) root.settingsManager.applySettings()
        }

        QQC2.Button {
            text: "Defaults"
            Layout.fillWidth: true
            onClicked: if (root.settingsManager) root.settingsManager.restoreDefaults()
        }
    }
}