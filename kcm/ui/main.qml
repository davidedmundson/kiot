import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: root

    Kirigami.FormLayout {
        id: form

        QQC2.TextField {
            Kirigami.FormData.label: i18n("Hostname:")
            text: kcm.settings.host
            onTextChanged: kcm.settings.host = text
        }

        QQC2.TextField {
            Kirigami.FormData.label: i18n("Port:")
            validator: IntValidator { bottom: 1; top: 65535 }
            text: kcm.settings.port
            onTextChanged: kcm.settings.port = text
        }

        QQC2.TextField {
            Kirigami.FormData.label: i18n("Username:")
            text: kcm.settings.user
            onTextChanged: kcm.settings.user = text
        }

        QQC2.TextField {
            Kirigami.FormData.label: i18n("Password:")
            echoMode: TextInput.Password
            text: kcm.password
            onTextChanged: kcm.password = text
        }
            QQC2.TextField {
            Kirigami.FormData.label: i18n("Discovery Prefix:")
            text: kcm.settings.discoveryprefix
            onTextChanged: kcm.settings.discoveryprefix = text
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: i18n("Use TLS:")
            checked: kcm.settings.tls
            onToggled: kcm.settings.tls = checked
        }
        QQC2.CheckBox {
            Kirigami.FormData.label: i18n("Show system tray:")
            checked: kcm.settings.systray
            onToggled: kcm.settings.systray = checked
        }
        QQC2.CheckBox {
            Kirigami.FormData.label: i18n("Autostart:")
            checked: kcm.settings.autostart
            onToggled: kcm.settings.autostart = checked
        }

        Item {
            Kirigami.FormData.isSection: true
        }

        QQC2.Button {
            text: i18n("More Settings…")
            icon.name: "configure"
            onClicked: kcm.openKiotSettings()
        }
    }
}

