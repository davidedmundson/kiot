import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: root
    
    // New Script Dialog
    Kirigami.Dialog {
        id: newScriptDialog
        title: "Create New Script"
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        
        ColumnLayout {
            Kirigami.FormLayout {
                QQC2.TextField {
                    id: newScriptNameField
                    Kirigami.FormData.label: "Script ID:"
                    placeholderText: "e.g., browser, steam, custom"
                }
                QQC2.TextField {
                    id: newScriptDisplayNameField
                    Kirigami.FormData.label: "Display Name:"
                    placeholderText: "e.g., Launch Browser, Open Steam"
                }
                QQC2.TextField {
                    id: newScriptCommandField
                    Kirigami.FormData.label: "Command:"
                    placeholderText: "e.g., /usr/bin/brave, steam steam://..."
                }
                QQC2.TextField {
                    id: newScriptIconField
                    Kirigami.FormData.label: "Icon:"
                    placeholderText: "e.g., mdi:web, mdi:steam"
                    text: "mdi:script-text"
                }
            }
        }
        
        onAccepted: {
            if (newScriptNameField.text.trim() !== "") {
                var scriptId = newScriptNameField.text.trim()
                var displayName = newScriptDisplayNameField.text.trim() || scriptId
                var command = newScriptCommandField.text.trim()
                var icon = newScriptIconField.text.trim() || "mdi:script-text"
                
                kcm.saveNestedConfigValue("Scripts", scriptId, "Name", displayName)
                kcm.saveNestedConfigValue("Scripts", scriptId, "Exec", command)
                kcm.saveNestedConfigValue("Scripts", scriptId, "icon", icon)
                
                // Reset fields
                newScriptNameField.text = ""
                newScriptDisplayNameField.text = ""
                newScriptCommandField.text = ""
                newScriptIconField.text = "mdi:script-text"
            }
        }
        onRejected: {
            newScriptNameField.text = ""
            newScriptDisplayNameField.text = ""
            newScriptCommandField.text = ""
            newScriptIconField.text = "mdi:script-text"
        }
    }
    // Delete Script Confirmation Dialog
    Kirigami.Dialog {
        id: deleteScriptDialog
        title: "Delete Script"
        standardButtons: Kirigami.Dialog.Yes | Kirigami.Dialog.No
    
        property string scriptId: ""
    
        ColumnLayout {
            Kirigami.Heading {
               level: 4
                text: "Are you sure you want to delete this script?"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        
            QQC2.Label {
                text: {
                    if (deleteScriptDialog.scriptId) {
                        var parts = deleteScriptDialog.scriptId.split("][")
                        return parts.length > 1 ? "Script: " + parts[1] : "Script: " + scriptId
                    }
                    return ""
                }
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    
        onAccepted: {
            if (deleteScriptDialog.scriptId) {
                var parts = deleteScriptDialog.scriptId.split("][")
                if (parts.length > 1) {
                    kcm.deleteNestedConfig(parts[0], parts[1])
                }
            }
            deleteScriptDialog.scriptId = ""
        }
    
        onRejected: {
            deleteScriptDialog.scriptId = ""
        }
    }

    // Delete Shortcut Confirmation Dialog
    Kirigami.Dialog {
        id: deleteShortcutDialog
        title: "Delete Shortcut"
        standardButtons: Kirigami.Dialog.Yes | Kirigami.Dialog.No
    
        property string shortcutId: ""
    
        ColumnLayout {
            Kirigami.Heading {
                level: 4
                text: "Are you sure you want to delete this shortcut?"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        
            QQC2.Label {
                text: {
                    if (deleteShortcutDialog.shortcutId) {
                        var parts = deleteShortcutDialog.shortcutId.split("][")
                        return parts.length > 1 ? "Shortcut: " + parts[1] : "Shortcut: " + shortcutId
                    }
                    return ""
                }
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    
        onAccepted: {
            if (deleteShortcutDialog.shortcutId) {
                var parts = deleteShortcutDialog.shortcutId.split("][")
                if (parts.length > 1) {
                    kcm.deleteNestedConfig(parts[0], parts[1])
                }
            }
            deleteShortcutDialog.shortcutId = ""
        }
    
        onRejected: {
            deleteShortcutDialog.shortcutId = ""
        }
    }
    // New Shortcut Dialog  
    Kirigami.Dialog {
        id: newShortcutDialog
        title: "Create New Shortcut"
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        
        ColumnLayout {
            Kirigami.FormLayout {
                QQC2.TextField {
                    id: newShortcutNameField
                    Kirigami.FormData.label: "Shortcut ID:"
                    placeholderText: "e.g., myShortcut, customAction"
                }
                QQC2.TextField {
                    id: newShortcutDisplayNameField
                    Kirigami.FormData.label: "Display Name:"
                    placeholderText: "e.g., Do a thing, Custom Action"
                }
            }
        }
        
        onAccepted: {
            if (newShortcutNameField.text.trim() !== "") {
                var shortcutId = newShortcutNameField.text.trim()
                var displayName = newShortcutDisplayNameField.text.trim() || shortcutId
                
                kcm.saveNestedConfigValue("Shortcuts", shortcutId, "Name", displayName)
                
                // Reset fields
                newShortcutNameField.text = ""
                newShortcutDisplayNameField.text = ""
            }
        }
        onRejected: {
            newShortcutNameField.text = ""
            newShortcutDisplayNameField.text = ""
        }
    }
    // Top level tab bar   
    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing
        
        QQC2.TabBar {
            id: tabBar
            Layout.fillWidth: true
            
            Repeater {
                model: kcm.sectionOrder
                
                QQC2.TabButton {
                    text: {
                        var section = modelData
                        // Format section name for display
                        if (section === "general") return "General"
                        if (section === "Integrations") return "Integrations"
                        if (section === "Scripts") return "Scripts"
                        if (section === "Shortcuts") return "Shortcuts"
                        if (section === "docker") return "Docker"
                        if (section === "heroic") return "Heroic Games"
                        if (section === "steam") return "Steam"
                        if (section === "systemd") return "Systemd Services"
                        // Capitalize first letter
                        return section.charAt(0).toUpperCase() + section.slice(1)
                    }
                }
            }
        }
        
        StackLayout {
            id: stackLayout
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex
            
            Repeater {
                model: kcm.sectionOrder
                
                Loader {
                    property string section: modelData
                    
                    sourceComponent: {
                        if (section === "general") {
                            return generalSettingsComponent
                        } else if (section === "Scripts") {
                            return scriptsGroupComponent
                        } else if (section === "Shortcuts") {
                            return shortcutsGroupComponent
                        } else {
                            return genericSettingsComponent
                        }
                    }
                }
            }
        }
    }
    
    // General settings component
    Component {
        id: generalSettingsComponent
        
        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            
            Kirigami.Heading {
                level: 2
                text: "MQTT Connection"
                Layout.fillWidth: true
            }
            
            GridLayout {
                columns: 2
                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: Kirigami.Units.largeSpacing
                Layout.fillWidth: true
                
                QQC2.Label {
                    text: "Hostname:"
                    Layout.alignment: Qt.AlignRight
                }
                QQC2.TextField {
                    id: hostField
                    Layout.fillWidth: true
                    text: kcm.settings && kcm.settings.host !== undefined ? kcm.settings.host : ""
                    onEditingFinished: if (kcm.settings) kcm.settings.host = text
                }
                
                QQC2.Label {
                    text: "Port:"
                    Layout.alignment: Qt.AlignRight
                }
                QQC2.TextField {
                    id: portField
                    Layout.fillWidth: true
                    inputMask: "99999999"
                    text: kcm.settings && kcm.settings.port !== undefined ? kcm.settings.port : 1883
                    onEditingFinished: if (kcm.settings) kcm.settings.port = parseInt(text) || 1883
                }
                
                QQC2.Label {
                    text: "Username:"
                    Layout.alignment: Qt.AlignRight
                }
                QQC2.TextField {
                    id: userField
                    Layout.fillWidth: true
                    text: kcm.settings && kcm.settings.user !== undefined ? kcm.settings.user : ""
                    onEditingFinished: if (kcm.settings) kcm.settings.user = text
                }
                
                QQC2.Label {
                    text: "Password:"
                    Layout.alignment: Qt.AlignRight
                }
                QQC2.TextField {
                    id: passwordField
                    Layout.fillWidth: true
                    echoMode: QQC2.TextInput.Password
                    text: kcm.settings && kcm.settings.password !== undefined ? kcm.settings.password : ""
                    onEditingFinished: if (kcm.settings) kcm.settings.password = text
                }
                
                QQC2.Label {
                    text: "Use SSL:"
                    Layout.alignment: Qt.AlignRight
                }
                QQC2.CheckBox {
                    id: sslCheckbox
                    checked: kcm.getConfigValue("general", "useSSL", false)
                    onCheckedChanged: kcm.saveConfigValue("general", "useSSL", checked)
                }

                QQC2.Label {
                    text: "Show system tray:"
                    Layout.alignment: Qt.AlignRight
                }
                QQC2.CheckBox {
                    id: systrayCheckbox
                    checked: kcm.getConfigValue("general", "systray", true)
                    onCheckedChanged: kcm.saveConfigValue("general", "systray", checked)
                }
            }
        }
    }
    
    // Generic settings component for boolean/value sections
    Component {
        id: genericSettingsComponent
        
        Flickable {
            contentWidth: width
            contentHeight: contentColumn.height
            clip: true
            
            ColumnLayout {
                id: contentColumn
                width: parent.width
                spacing: Kirigami.Units.largeSpacing
                
                property var sectionKeys: {
                    var sectionData = kcm.configSections[section]
                    if (!sectionData) return []
                    var keys = Object.keys(sectionData)
                    return keys.sort()
                }
                
                Connections {
                    target: kcm
                    function onConfigSectionsChanged() {
                        contentColumn.sectionKeys = contentColumn.sectionKeys
                    }
                }
                
                Repeater {
                    model: contentColumn.sectionKeys
                    
                    Item {
                        Layout.fillWidth: true
                        implicitHeight: childrenRect.height
                        
                        property string configKey: modelData
                        property var configValue: {
                            var sectionData = kcm.configSections[section]
                            return sectionData ? sectionData[configKey] : undefined
                        }
                        
                        QQC2.CheckBox {
                            visible: typeof configValue === "boolean"
                            width: parent.width
                            
                            text: {
                                var displayKey = configKey
                                displayKey = displayKey.replace(/_/g, " ")
                                displayKey = displayKey.replace(/\b\w/g, function(l) { return l.toUpperCase() })
                                return displayKey
                            }
                            checked: typeof configValue === "boolean" ? configValue : false
                            onCheckedChanged: kcm.saveConfigValue(section, configKey, checked)
                        }
                        
                        GridLayout {
                            visible: typeof configValue !== "boolean"
                            columns: 2
                            columnSpacing: Kirigami.Units.largeSpacing
                            width: parent.width
                            
                            QQC2.Label {
                                text: {
                                    var displayKey = configKey
                                    displayKey = displayKey.replace(/_/g, " ")
                                    displayKey = displayKey.replace(/\b\w/g, function(l) { return l.toUpperCase() })
                                    return displayKey + ":"
                                }
                                Layout.alignment: Qt.AlignRight
                            }
                            
                            QQC2.TextField {
                                Layout.fillWidth: true
                                text: configValue ? configValue.toString() : ""
                                onEditingFinished: kcm.saveConfigValue(section, configKey, text)
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Scripts group component
    Component {
        id: scriptsGroupComponent
        
        Flickable {
            contentWidth: width
            contentHeight: contentColumn.height
            clip: true
            
            ColumnLayout {
                id: contentColumn
                width: parent.width
                spacing: Kirigami.Units.largeSpacing
                
                Kirigami.Heading {
                    level: 2
                    text: "Scripts"
                    Layout.fillWidth: true
                }
                    
                Kirigami.ActionToolBar {
                    Layout.fillWidth: true
                    actions: [
                        Kirigami.Action {
                            text: "Create New Script"
                            icon.name: "list-add"
                            onTriggered: newScriptDialog.open()
                        }
                    ]
                }
                
                property var scriptKeys: {
                    var sectionData = kcm.configSections[section]
                    if (!sectionData) return []
                    var keys = Object.keys(sectionData)
                    return keys.sort()
                }
                
                Connections {
                    target: kcm
                    function onConfigSectionsChanged() {
                        contentColumn.scriptKeys = contentColumn.scriptKeys
                    }
                }
                
                Repeater {
                    model: contentColumn.scriptKeys
                    
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing
                        
                        property string scriptId: modelData
                        property var scriptData: {
                            var sectionData = kcm.configSections[section]
                            return sectionData ? sectionData[scriptId] : {}
                        }
                        
                        Kirigami.Separator {
                            Layout.fillWidth: true
                            visible: model.index > 0
                        }
                        
                        Kirigami.Heading {
                            level: 3
                            text: {
                                var parts = scriptId.split("][")
                                return parts.length > 1 ? parts[1] : scriptId
                            }
                            Layout.fillWidth: true
                        }
                        // TODO make it show on same line as id but far right side?
                        QQC2.Button {
                            text: "Delete"
                            icon.name: "edit-delete"
                            onClicked: {
                                deleteScriptDialog.scriptId = scriptId
                                deleteScriptDialog.open()
                            }
                        }
                        
                        GridLayout {
                            columns: 2
                            columnSpacing: Kirigami.Units.largeSpacing
                            rowSpacing: Kirigami.Units.largeSpacing
                            Layout.fillWidth: true
                            
                            QQC2.Label {
                                text: "Name:"
                                Layout.alignment: Qt.AlignRight
                            }
                            QQC2.TextField {
                                id: scriptNameField
                                Layout.fillWidth: true
                                text: scriptData["Name"] || ""
                                onEditingFinished: {
                                    var parts = scriptId.split("][")
                                    if (parts.length > 1) {
                                        kcm.saveNestedConfigValue(parts[0], parts[1], "Name", text)
                                    }
                                }
                            }
                            
                            QQC2.Label {
                                text: "Command:"
                                Layout.alignment: Qt.AlignRight
                            }
                            QQC2.TextField {
                                id: scriptExecField
                                Layout.fillWidth: true
                                text: scriptData["Exec"] || ""
                                onEditingFinished: {
                                    var parts = scriptId.split("][")
                                    if (parts.length > 1) {
                                        kcm.saveNestedConfigValue(parts[0], parts[1], "Exec", text)
                                    }
                                }
                            }
                            
                            QQC2.Label {
                                text: "Icon:"
                                Layout.alignment: Qt.AlignRight
                            }
                            QQC2.TextField {
                                id: scriptIconField
                                Layout.fillWidth: true
                                text: scriptData["icon"] || ""
                                onEditingFinished: {
                                    var parts = scriptId.split("][")
                                    if (parts.length > 1) {
                                        kcm.saveNestedConfigValue(parts[0], parts[1], "icon", text)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Shortcuts group component
    Component {
        id: shortcutsGroupComponent
        
        Flickable {
            contentWidth: width
            contentHeight: contentColumn.height
            clip: true
            
            ColumnLayout {
                id: contentColumn
                width: parent.width
                spacing: Kirigami.Units.largeSpacing
                
                Kirigami.Heading {
                    level: 2
                    text: "Shortcuts"
                    Layout.fillWidth: true
                }
                
                Kirigami.ActionToolBar {
                    Layout.fillWidth: true
                    actions: [
                        Kirigami.Action {
                            text: "Create New Shortcut"
                            icon.name: "list-add"
                            onTriggered: newShortcutDialog.open()
                        }
                    ]
                }
                
                property var shortcutKeys: {
                    var sectionData = kcm.configSections[section]
                    if (!sectionData) return []
                    var keys = Object.keys(sectionData)
                    return keys.sort()
                }
                
                Connections {
                    target: kcm
                    function onConfigSectionsChanged() {
                        contentColumn.shortcutKeys = contentColumn.shortcutKeys
                    }
                }
                
                Repeater {
                    model: contentColumn.shortcutKeys
                    
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing
                        
                        property string shortcutId: modelData
                        property var shortcutData: {
                            var sectionData = kcm.configSections[section]
                            return sectionData ? sectionData[shortcutId] : {}
                        }
                        
                        Kirigami.Separator {
                            Layout.fillWidth: true
                            visible: model.index > 0
                        }
                        
                        Kirigami.Heading {
                            level: 3
                            text: {
                                var parts = shortcutId.split("][")
                                return parts.length > 1 ? parts[1] : shortcutId
                            }
                            Layout.fillWidth: true
                        }
                        // TODO make it show on same line as id but far right side?
                        QQC2.Button {
                            text: "Delete"
                            icon.name: "edit-delete"
                            onClicked: {
                                deleteShortcutDialog.shortcutId = shortcutId
                                deleteShortcutDialog.open()
                            }
                        }
                        GridLayout {
                            columns: 2
                            columnSpacing: Kirigami.Units.largeSpacing
                            rowSpacing: Kirigami.Units.largeSpacing
                            Layout.fillWidth: true
                            
                            QQC2.Label {
                                text: "Name:"
                                Layout.alignment: Qt.AlignRight
                            }
                            QQC2.TextField {
                                id: shortcutNameField
                                Layout.fillWidth: true
                                text: shortcutData["Name"] || ""
                                onEditingFinished: {
                                    var parts = shortcutId.split("][")
                                    if (parts.length > 1) {
                                        kcm.saveNestedConfigValue(parts[0], parts[1], "Name", text)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}