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
            inputMask: "99999999"
            text: kcm.settings.port
            onTextChanged: kcm.settings.port = value
        }

        QQC2.TextField {
            Kirigami.FormData.label: i18n("Username:")
            text: kcm.settings.user
            onTextChanged: kcm.settings.user = text
        }

        QQC2.TextField {
            Kirigami.FormData.label: i18n("Password:")
            echoMode: QQC2.TextInput.Password
            text: kcm.settings.password
            onTextChanged: kcm.settings.password = text
        }
    }
}

