import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs

QQC2.Page  {
    id: root
    clip: true
    anchors.fill: parent
    height: 600
    width: 800

// New Custom Sensor QQC2.Dialog
    QQC2.Dialog {
        id: newCustomSensorDialog
        title: "Create New Custom Sensor"
        standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            
            GridLayout {
                columns: 2
                columnSpacing: 10
                rowSpacing: 10
                
                QQC2.Label { text: "Sensor ID:" }
                QQC2.TextField {
                    id: newSensorIdField
                    placeholderText: "e.g., gpu_power, custom_metric"
                    Layout.fillWidth: true
                }
                
                QQC2.Label { text: "Display Name:" }
                QQC2.TextField {
                    id: newSensorNameField
                    placeholderText: "e.g., Gpu power draw"
                    Layout.fillWidth: true
                }
                
                QQC2.Label { text: "Command:" }
                QQC2.TextField {
                    id: newSensorCommandField
                    placeholderText: "e.g., nvidia-smi ..."
                    Layout.fillWidth: true
                }
                
                QQC2.Label { text: "Interval:" }
                QQC2.TextField {
                    id: newSensorIntervalField
                    placeholderText: "e.g., 60s, 10s"
                    text: "60s"
                    Layout.fillWidth: true
                }
                
                QQC2.Label { text: "Unit:" }
                QQC2.TextField {
                    id: newSensorUnitField
                    placeholderText: "e.g., W, C, %, GiB"
                    Layout.fillWidth: true
                }
                
                QQC2.Label { text: "Device Class:" }
                QQC2.TextField {
                    id: newSensorDeviceClassField
                    placeholderText: "e.g., power, temperature"
                    Layout.fillWidth: true
                }
                
                QQC2.Label { text: "State Class:" }
                QQC2.TextField {
                    id: newSensorStateClassField
                    placeholderText: "e.g., measurement"
                    text: "measurement"
                    Layout.fillWidth: true
                }
            }
        }
        
        onAccepted: {
            if (newSensorIdField.text.trim() !== "") {
                var sensorId = newSensorIdField.text.trim()
                var name = newSensorNameField.text.trim() || sensorId
                var command = newSensorCommandField.text.trim()
                var interval = newSensorIntervalField.text.trim() || "60s"
                var unit = newSensorUnitField.text.trim()
                var deviceClass = newSensorDeviceClassField.text.trim()
                var stateClass = newSensorStateClassField.text.trim()
                
                settingsManager.saveNestedConfigValue("CustomSensors", sensorId, "name", name)
                settingsManager.saveNestedConfigValue("CustomSensors", sensorId, "command", command)
                settingsManager.saveNestedConfigValue("CustomSensors", sensorId, "interval", interval)
                if (unit) settingsManager.saveNestedConfigValue("CustomSensors", sensorId, "unit_of_measurement", unit)
                if (deviceClass) settingsManager.saveNestedConfigValue("CustomSensors", sensorId, "device_class", deviceClass)
                if (stateClass) settingsManager.saveNestedConfigValue("CustomSensors", sensorId, "state_class", stateClass)
                
                // Reset fields
                newSensorIdField.text = ""
                newSensorNameField.text = ""
                newSensorCommandField.text = ""
                newSensorIntervalField.text = "60s"
                newSensorUnitField.text = ""
                newSensorDeviceClassField.text = ""
                newSensorStateClassField.text = "measurement"
            }
        }
        onRejected: {
            newSensorIdField.text = ""
            newSensorNameField.text = ""
            newSensorCommandField.text = ""
            newSensorIntervalField.text = "60s"
            newSensorUnitField.text = ""
            newSensorDeviceClassField.text = ""
            newSensorStateClassField.text = "measurement"
        }
    }

// Delete Custom Sensor QQC2.Dialog
    QQC2.Dialog {
        id: deleteCustomSensorDialog
        title: "Delete Custom Sensor"
        standardButtons: QQC2.Dialog.Yes | QQC2.Dialog.No
    
        property string sensorId: ""
    
        ColumnLayout {
            QQC2.Label {
                text: "Are you sure you want to delete this custom sensor?"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        
            QQC2.Label {
                text: deleteCustomSensorDialog.sensorId ? "Sensor: " + deleteCustomSensorDialog.sensorId : ""
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    
        onAccepted: {
            if (deleteCustomSensorDialog.sensorId && settingsManager) {
                settingsManager.deleteNestedConfig("CustomSensors", deleteCustomSensorDialog.sensorId)
            }
            deleteCustomSensorDialog.sensorId = ""
        }
    
        onRejected: {
            deleteCustomSensorDialog.sensorId = ""
        }
    }
    // New Script QQC2.Dialog
    QQC2.Dialog {
        id: newScriptDialog
        title: "Create New Script"
        standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            
            GridLayout {
                columns: 2
                columnSpacing: 10
                rowSpacing: 10
                
                QQC2.Label { text: "Script ID:" }
                QQC2.TextField {
                    id: newScriptNameField
                    placeholderText: "e.g., browser, steam, custom"
                    Layout.fillWidth: true
                }
                
                QQC2.Label { text: "Display Name:" }
                QQC2.TextField {
                    id: newScriptDisplayNameField
                    placeholderText: "e.g., Launch Browser, Open Steam"
                    Layout.fillWidth: true
                }
                
                QQC2.Label { text: "Command:" }
                QQC2.TextField {
                    id: newScriptCommandField
                    placeholderText: "e.g., /usr/bin/brave, steam steam://..."
                    Layout.fillWidth: true
                }
                
                QQC2.Label { text: "Icon:" }
                QQC2.TextField {
                    id: newScriptIconField
                    placeholderText: "e.g., mdi:web, mdi:steam"
                    text: "mdi:script-text"
                    Layout.fillWidth: true
                }
            }
        }
        
        onAccepted: {
            if (newScriptNameField.text.trim() !== "") {
                var scriptId = newScriptNameField.text.trim()
                var displayName = newScriptDisplayNameField.text.trim() || scriptId
                var command = newScriptCommandField.text.trim()
                var icon = newScriptIconField.text.trim() || "mdi:script-text"
                
                settingsManager.saveNestedConfigValue("Scripts", scriptId, "Name", displayName)
                settingsManager.saveNestedConfigValue("Scripts", scriptId, "Exec", command)
                settingsManager.saveNestedConfigValue("Scripts", scriptId, "icon", icon)
                
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
    
    // Delete Script Confirmation QQC2.Dialog
    QQC2.Dialog {
        id: deleteScriptDialog
        title: "Delete Script"
        standardButtons: QQC2.Dialog.Yes | QQC2.Dialog.No
    
        property string scriptId: ""
    
        ColumnLayout {
            QQC2.Label {
                text: "Are you sure you want to delete this script?"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        
            QQC2.Label {
                text: {
                    if (deleteScriptDialog.scriptId) {
                        var parts = deleteScriptDialog.scriptId.split("/")
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
                var parts = deleteScriptDialog.scriptId.split("/")
                if (parts.length > 1 && settingsManager) {
                    settingsManager.deleteNestedConfig(parts[0], parts[1])
                }
            }
            deleteScriptDialog.scriptId = ""
        }
    
        onRejected: {
            deleteScriptDialog.scriptId = ""
        }
    }
    
    // New Shortcut QQC2.Dialog
    QQC2.Dialog {
        id: newShortcutDialog
        title: "Create New Shortcut"
        standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            
            GridLayout {
                columns: 2
                columnSpacing: 10
                rowSpacing: 10
                
                QQC2.Label { text: "Shortcut ID:" }
                QQC2.TextField {
                    id: newShortcutNameField
                    placeholderText: "e.g., myShortcut, customAction"
                    Layout.fillWidth: true
                }
                
                QQC2.Label { text: "Display Name:" }
                QQC2.TextField {
                    id: newShortcutDisplayNameField
                    placeholderText: "e.g., Do a thing, Custom Action"
                    Layout.fillWidth: true
                }
            }
        }
        
        onAccepted: {
            if (newShortcutNameField.text.trim() !== "") {
                var shortcutId = newShortcutNameField.text.trim()
                var displayName = newShortcutDisplayNameField.text.trim() || shortcutId
                
                settingsManager.saveNestedConfigValue("Shortcuts", shortcutId, "Name", displayName)
                
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
    
    // Delete Shortcut Confirmation QQC2.Dialog
    QQC2.Dialog {
        id: deleteShortcutDialog
        title: "Delete Shortcut"
        standardButtons: QQC2.Dialog.Yes | QQC2.Dialog.No
    
        property string shortcutId: ""
    
        ColumnLayout {
            QQC2.Label {
                text: "Are you sure you want to delete this shortcut?"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        
            QQC2.Label {
                text: {
                    if (deleteShortcutDialog.shortcutId) {
                        var parts = deleteShortcutDialog.shortcutId.split("/")
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
                var parts = deleteShortcutDialog.shortcutId.split("/")
                if (parts.length > 1 && settingsManager) {
                    settingsManager.deleteNestedConfig(parts[0], parts[1])
                }
            }
            deleteShortcutDialog.shortcutId = ""
        }
    
        onRejected: {
            deleteShortcutDialog.shortcutId = ""
        }
    }
    
    // Main layout
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        
        // Tab selector
        QQC2.TabBar {
            id: tabBar
            Layout.fillWidth: true
            
            Repeater {
                model: settingsManager ? settingsManager.sectionOrder : []
                
                QQC2.TabButton {
                    text: {
                        var section = modelData
                        if (section === "general") return "General"
                        if (section === "Integrations") return "Integrations"
                        if (section === "Scripts") return "Scripts"
                        if (section === "Shortcuts") return "Shortcuts"
                        if (section === "CustomSensors") return "CustomSensors"
                        if (section === "docker") return "Docker"
                        if (section === "heroic") return "Heroic Games"
                        if (section === "steam") return "Steam"
                        if (section === "systemd") return "Systemd Services"
                        return section.charAt(0).toUpperCase() + section.slice(1)
                    }
                }
            }
        }
        
        // Content area
        StackLayout {
            id: stackLayout
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex
            
            Repeater {
                model: settingsManager ? settingsManager.sectionOrder : []
                
                Loader {
                    property string section: modelData
                    
                    sourceComponent: {
                        if (section === "general") {
                            return generalSettingsComponent
                        } else if (section === "Scripts") {
                            return scriptsGroupComponent
                        } else if (section === "Shortcuts") {
                            return shortcutsGroupComponent
                        } else if (section === "CustomSensors") {
                            return customSensorsGroupComponent
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
            anchors.fill: parent
            spacing: 10

            QQC2.Label {
                text: "MQTT Connection"
                font.bold: true
                font.pixelSize: 16
                Layout.fillWidth: true
            }

            GridLayout {
                columns: 2
                columnSpacing: 10
                rowSpacing: 10
                Layout.fillWidth: true

                QQC2.Label { text: "Hostname:"; Layout.alignment: Qt.AlignRight }
                QQC2.TextField {
                    id: hostField
                    Layout.fillWidth: true
                    text: settingsManager ? settingsManager.getHost() : ""
                    onEditingFinished: if (settingsManager) settingsManager.setHost(text)
                }

                QQC2.Label { text: "Port:"; Layout.alignment: Qt.AlignRight }
                QQC2.TextField {
                    id: portField
                    Layout.fillWidth: true
                    text: settingsManager ? settingsManager.getPort() : 1883
                    onEditingFinished: if (settingsManager) settingsManager.setPort(parseInt(text) || 1883)
                }

                QQC2.Label { text: "Username:"; Layout.alignment: Qt.AlignRight }
                QQC2.TextField {
                    id: userField
                    Layout.fillWidth: true
                    text: settingsManager ? settingsManager.getUser() : ""
                    onEditingFinished: if (settingsManager) settingsManager.setUser(text)
                }

                QQC2.Label { text: "Password:"; Layout.alignment: Qt.AlignRight }
                QQC2.TextField {
                    id: passwordField
                    Layout.fillWidth: true
                    text: settingsManager ? settingsManager.getPassword() : ""
                    echoMode: TextInput.Password
                    onEditingFinished: if (settingsManager) settingsManager.setPassword(text)
                }

                QQC2.Label { text: "Discovery:"; Layout.alignment: Qt.AlignRight }
                QQC2.TextField {
                    id: discoveryField
                    Layout.fillWidth: true
                    text: settingsManager ? settingsManager.getDiscoveryPrefix() : "homeassistant"
                    onEditingFinished: if (settingsManager) settingsManager.setDiscoveryPrefix(text)
                }

                QQC2.Label { text: "Use SSL:"; Layout.alignment: Qt.AlignRight }
                QQC2.CheckBox {
                    id: sslCheckbox
                    checked: settingsManager ? settingsManager.getConfigValue("general", "useSSL", false) : false
                    onToggled: if (settingsManager) settingsManager.saveConfigValue("general", "useSSL", checked)
                }

                QQC2.Label { text: "Show system tray:"; Layout.alignment: Qt.AlignRight }
                QQC2.CheckBox {
                    id: systrayCheckbox
                    checked: settingsManager ? settingsManager.getConfigValue("general", "systray", true) : true
                    onToggled: if (settingsManager) settingsManager.saveConfigValue("general", "systray", checked)
                }

                QQC2.Label { text: "Autostart:"; Layout.alignment: Qt.AlignRight }
                QQC2.CheckBox {
                    id: autostartCheckbox
                    checked: settingsManager ? settingsManager.getConfigValue("general", "autostart", false) : false
                    onToggled: if (settingsManager) settingsManager.saveConfigValue("general", "autostart", checked)
                }
            }

            // ---- SPACER ----
            Item { 
                Layout.fillHeight: true 
                Layout.fillWidth: true
            }

            // ---- Apply and Defaults buttons ----
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                QQC2.Button {
                    text: "Apply"
                    Layout.fillWidth: true
                    onClicked: if (settingsManager) settingsManager.applySettings()
                }

                QQC2.Button {
                    text: "Defaults"
                    Layout.fillWidth: true
                    onClicked: if (settingsManager) settingsManager.restoreDefaults()
                }
            }
        }
    }

// ... existing code ...
    
    // Generic settings component
    Component {
        id: genericSettingsComponent
        
        Flickable {
            contentWidth: width
            contentHeight: contentColumn.height
            clip: true
            
            ColumnLayout {
                id: contentColumn
                width: parent.width
                spacing: 10
                
                property var sectionKeys: {
                    var sectionData = settingsManager ? settingsManager.configSections[section] : {}
                    if (!sectionData) return []
                    var keys = Object.keys(sectionData)
                    return keys.sort()
                }
                
                Connections {
                    target: settingsManager
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
                            var sectionData = settingsManager ? settingsManager.configSections[section] : {}
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
                            onToggled: if (settingsManager) settingsManager.saveConfigValue(section, configKey, checked)
                        }
                        
                        GridLayout {
                            visible: typeof configValue !== "boolean"
                            columns: 2
                            columnSpacing: 10
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
                                onEditingFinished: if (settingsManager) settingsManager.saveConfigValue(section, configKey, text)
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
                spacing: 10
                
                QQC2.Label {
                    text: "Scripts"
                    font.bold: true
                    font.pixelSize: 16
                    Layout.fillWidth: true
                }
                
                QQC2.Button {
                    text: "Create New Script"
                    onClicked: newScriptDialog.open()
                    Layout.fillWidth: true
                }
                
                property var scriptKeys: {
                    var sectionData = settingsManager ? settingsManager.configSections[section] : {}
                    if (!sectionData) return []
                    var keys = Object.keys(sectionData)
                    return keys.sort()
                }
                
                Connections {
                    target: settingsManager
                    function onConfigSectionsChanged() {
                        contentColumn.scriptKeys = contentColumn.scriptKeys
                    }
                }
                
                Repeater {
                    model: contentColumn.scriptKeys
                    
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        
                        property string scriptId: modelData
                        property var scriptData: {
                            var sectionData = settingsManager ? settingsManager.configSections[section] : {}
                            return sectionData ? sectionData[scriptId] : {}
                        }
                        
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "lightgray"
                            visible: model.index > 0
                        }
                        
                        QQC2.Label {
                            text: {
                                var parts = scriptId.split("/")
                                return parts.length > 1 ? scriptId : scriptId
                            }
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        
                        QQC2.Button {
                            text: "Delete"
                            onClicked: {
                                deleteScriptDialog.scriptId = scriptId
                                deleteScriptDialog.open()
                            }
                            Layout.alignment: Qt.AlignRight
                        }
                        
                        GridLayout {
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 10
                            Layout.fillWidth: true
                            
                            QQC2.Label { text: "Name:"; Layout.alignment: Qt.AlignRight }
                            QQC2.TextField {
                                text: scriptData["Name"] || ""
                                onEditingFinished: {
                                    var parts = scriptId.split("/")
                                    if (parts.length > 1 && settingsManager) {
                                        settingsManager.saveNestedConfigValue(parts[0], parts[1], "Name", text)
                                    }
                                }
                                Layout.fillWidth: true
                            }
                            
                            QQC2.Label { text: "Command:"; Layout.alignment: Qt.AlignRight }
                            QQC2.TextField {
                                text: scriptData["Exec"] || ""
                                onEditingFinished: {
                                    var parts = scriptId.split("/")
                                    if (parts.length > 1 && settingsManager) {
                                        settingsManager.saveNestedConfigValue(parts[0], parts[1], "Exec", text)
                                    }
                                }
                                Layout.fillWidth: true
                            }
                            
                            QQC2.Label { text: "Icon:"; Layout.alignment: Qt.AlignRight }
                            QQC2.TextField {
                                text: scriptData["icon"] || ""
                                onEditingFinished: {
                                    var parts = scriptId
                                    if (parts.length > 1 && settingsManager) {
                                        settingsManager.saveNestedConfigValue("Scripts", scriptId, "icon", text)
                                    }
                                }
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
        }
    }

 // CustomSensors group component
    Component {
        id: customSensorsGroupComponent
        
        Flickable {
            contentWidth: width
            contentHeight: contentColumn.height
            clip: true
            
            ColumnLayout {
                id: contentColumn
                width: parent.width
                spacing: 10
                
                QQC2.Label {
                    text: "Custom Sensors"
                    font.bold: true
                    font.pixelSize: 16
                    Layout.fillWidth: true
                }
                
                QQC2.Button {
                    text: "Create New Sensor"
                    onClicked: newCustomSensorDialog.open()
                    Layout.fillWidth: true
                }
                
                property var sensorKeys: {
                    var sectionData = settingsManager ? settingsManager.configSections[section] : {}
                    if (!sectionData) return []
                    var keys = Object.keys(sectionData)
                    return keys.sort()
                }
                
                Connections {
                    target: settingsManager
                    function onConfigSectionsChanged() {
                        contentColumn.sensorKeys = contentColumn.sensorKeys
                    }
                }
                
                Repeater {
                    model: contentColumn.sensorKeys
                    
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        
                        property string sensorId: modelData
                        property var sensorData: {
                            var sectionData = settingsManager ? settingsManager.configSections[section] : {}
                            return sectionData ? sectionData[sensorId] : {}
                        }
                        
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "lightgray"
                            visible: model.index > 0
                        }
                        
                        QQC2.Label {
                            text: sensorId
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        
                        QQC2.Button {
                            text: "Delete"
                            onClicked: {
                                deleteCustomSensorDialog.sensorId = sensorId
                                deleteCustomSensorDialog.open()
                            }
                            Layout.alignment: Qt.AlignRight
                        }
                        
                        GridLayout {
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 10
                            Layout.fillWidth: true
                            
                            QQC2.Label { text: "Name:"; Layout.alignment: Qt.AlignRight }
                            QQC2.TextField {
                                text: sensorData["name"] || ""
                                onEditingFinished: {
                                    if (settingsManager) {
                                        settingsManager.saveNestedConfigValue("CustomSensors", sensorId, "name", text)
                                    }
                                }
                                Layout.fillWidth: true
                            }
                            
                            QQC2.Label { text: "Command:"; Layout.alignment: Qt.AlignRight }
                            QQC2.TextField {
                                text: sensorData["command"] || ""
                                onEditingFinished: {
                                    if (settingsManager) {
                                        settingsManager.saveNestedConfigValue("CustomSensors", sensorId, "command", text)
                                    }
                                }
                                Layout.fillWidth: true
                            }
                            
                            QQC2.Label { text: "Interval:"; Layout.alignment: Qt.AlignRight }
                            QQC2.TextField {
                                text: sensorData["interval"] || ""
                                onEditingFinished: {
                                    if (settingsManager) {
                                        settingsManager.saveNestedConfigValue("CustomSensors", sensorId, "interval", text)
                                    }
                                }
                                Layout.fillWidth: true
                            }
                            
                            QQC2.Label { text: "Unit:"; Layout.alignment: Qt.AlignRight }
                            QQC2.TextField {
                                text: sensorData["unit_of_measurement"] || ""
                                onEditingFinished: {
                                    if (settingsManager) {
                                        settingsManager.saveNestedConfigValue("CustomSensors", sensorId, "unit_of_measurement", text)
                                    }
                                }
                                Layout.fillWidth: true
                            }
                            
                            QQC2.Label { text: "Device Class:"; Layout.alignment: Qt.AlignRight }
                            QQC2.TextField {
                                text: sensorData["device_class"] || ""
                                onEditingFinished: {
                                    if (settingsManager) {
                                        settingsManager.saveNestedConfigValue("CustomSensors", sensorId, "device_class", text)
                                    }
                                }
                                Layout.fillWidth: true
                            }
                            
                            QQC2.Label { text: "State Class:"; Layout.alignment: Qt.AlignRight }
                            QQC2.TextField {
                                text: sensorData["state_class"] || ""
                                onEditingFinished: {
                                    if (settingsManager) {
                                        settingsManager.saveNestedConfigValue("CustomSensors", sensorId, "state_class", text)
                                    }
                                }
                                Layout.fillWidth: true
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
                spacing: 10
                
                QQC2.Label {
                    text: "Shortcuts"
                    font.bold: true
                    font.pixelSize: 16
                    Layout.fillWidth: true
                }
                
                QQC2.Button {
                    text: "Create New Shortcut"
                    onClicked: newShortcutDialog.open()
                    Layout.fillWidth: true
                }
                
                property var shortcutKeys: {
                    var sectionData = settingsManager ? settingsManager.configSections[section] : {}
                    if (!sectionData) return []
                    var keys = Object.keys(sectionData)
                    return keys.sort()
                }
                
                Connections {
                    target: settingsManager
                    function onConfigSectionsChanged() {
                        contentColumn.shortcutKeys = contentColumn.shortcutKeys
                    }
                }
                
                Repeater {
                    model: contentColumn.shortcutKeys
                    
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        
                        property string shortcutId: modelData
                        property var shortcutData: {
                            var sectionData = settingsManager ? settingsManager.configSections[section] : {}
                            return sectionData ? sectionData[shortcutId] : {}
                        }
                        
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "lightgray"
                            visible: model.index > 0
                        }
                        
                        QQC2.Label {
                            text: {
                                var parts = shortcutId.split("/")
                                return parts.length > 1 ? parts[1] : shortcutId
                            }
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        
                        QQC2.Button {
                            text: "Delete"
                            onClicked: {
                                deleteShortcutDialog.shortcutId = shortcutId
                                deleteShortcutDialog.open()
                            }
                            Layout.alignment: Qt.AlignRight
                        }
                        
                        GridLayout {
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 10
                            Layout.fillWidth: true
                            
                            QQC2.Label { text: "Name:"; Layout.alignment: Qt.AlignRight }
                            QQC2.TextField {
                                text: shortcutData["Name"] || ""
                                onEditingFinished: {
                                    var parts = shortcutId.split("/")
                                    if (parts.length > 1 && settingsManager) {
                                        settingsManager.saveNestedConfigValue(parts[0], parts[1], "Name", text)
                                    }
                                }
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
        }
    }
}