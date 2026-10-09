import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import TicketApp
import "ui"
import "ui/Strings.js" as Strings

ApplicationWindow {
    id: window
    width: 420; height: 860; minimumWidth: 320
    visible: true; title: "UMIT"
    color: appSettings.backgroundColor
    property int currentTab: 0
    readonly property color surfaceColor: Qt.tint(appSettings.backgroundColor, appSettings.darkMode ? "#08ffffff" : "#06000000")
    readonly property color borderColor: Qt.tint(appSettings.backgroundColor, appSettings.darkMode ? "#26ffffff" : "#22000000")
    readonly property color accentColor: appSettings.darkMode ? "#7cdac5" : "#12685d"
    function tr(key) { return Strings.text(appSettings.language, key) }
    function message(key) { return key.length ? tr(key) : "" }
    function openAccount() { currentTab = 2 }
    Material.theme: appSettings.darkMode ? Material.Dark : Material.Light
    Material.primary: accentColor
    Material.accent: accentColor
    Material.foreground: appSettings.textColor
    Material.background: appSettings.backgroundColor
    palette.window: appSettings.backgroundColor
    palette.base: surfaceColor
    palette.alternateBase: surfaceColor
    palette.text: appSettings.textColor
    palette.windowText: appSettings.textColor
    palette.buttonText: appSettings.textColor
    palette.button: surfaceColor
    palette.highlight: accentColor
    palette.highlightedText: appSettings.darkMode ? "#122921" : "white"
    font.pixelSize: 14
    TicketController { id: controller; objectName: "ticketController"; Component.onCompleted: setLanguage(appSettings.language) }
    Connections {
        target: appSettings
        function onLanguageChanged() { controller.setLanguage(appSettings.language) }
        function onLockStateChanged() { Qt.inputMethod.hide() }
    }
    Connections {
        target: account
        function onSessionChanged() { controller.resetSession() }
    }
    Timer {
        id: backTimer
        interval: 2000
    }
    Shortcut {
        sequence: "Back"
        autoRepeat: false
        enabled: Qt.platform.os === "android" && !backTimer.running
        onActivated: {
            if (Qt.inputMethod.visible) {
                Qt.inputMethod.hide()
                return
            }
            backTimer.start()
        }
    }
    Label {
        visible: backTimer.running
        text: window.tr("pressBackAgain")
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        width: Math.min(implicitWidth, parent.width - 24)
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        padding: 12
        color: "white"
        z: 100
        background: Rectangle { color: "#333333"; radius: 12 }
    }
    header: ToolBar {
        visible: !appSettings.locked
        background: Rectangle { color: window.color }
        contentItem: RowLayout {
            spacing: 12
            Item { implicitWidth: 6 }
            Rectangle {
                implicitWidth: 40; implicitHeight: 40; radius: 12
                color: window.accentColor
                Image { anchors.fill: parent; anchors.margins: 4; source: appInfo.logoUrl; fillMode: Image.PreserveAspectFit; visible: source.toString().length > 0 }
                Label { anchors.centerIn: parent; text: "U"; font.bold: true; font.pixelSize: 23; color: appSettings.darkMode ? "#122921" : "white"; visible: appInfo.logoUrl.toString().length === 0 }
            }
            ColumnLayout {
                spacing: 0
                Label { text: "UMIT"; font.pixelSize: 21; font.bold: true; font.letterSpacing: 2 }
                Label { text: window.tr(["home", "tickets", "profile", "settings", "support"][window.currentTab]); font.pixelSize: 12; opacity: 0.7 }
            }
            Item { Layout.fillWidth: true }
            ToolButton { text: appSettings.lockEnabled ? "●" : "○"; visible: appSettings.lockEnabled; onClicked: appSettings.lockNow(); Accessible.name: window.tr("lockNow") }
            Item { implicitWidth: 6 }
        }
    }
    footer: TabBar {
        id: navigation
        bottomPadding: SafeArea.margins.bottom
        visible: !appSettings.locked
        currentIndex: window.currentTab
        onCurrentIndexChanged: window.currentTab = currentIndex
        background: Rectangle { color: window.surfaceColor; border.color: window.borderColor }
        Repeater {
            model: ["home", "tickets", "profile", "settings", "support"]
            TabButton {
                required property string modelData
                required property int index
                implicitHeight: 62
                padding: 2
                contentItem: ColumnLayout {
                    spacing: 3
                    Canvas {
                        Layout.alignment: Qt.AlignHCenter; implicitWidth: 24; implicitHeight: 24
                        property color ink: index === window.currentTab ? window.accentColor : appSettings.textColor
                        onInkChanged: requestPaint()
                        onPaint: {
                            var c = getContext("2d"); c.reset(); c.strokeStyle = ink; c.lineWidth = 1.7; c.lineCap = "round"; c.lineJoin = "round";
                            c.beginPath();
                            if (index === 0) { c.moveTo(3,10); c.lineTo(12,3); c.lineTo(21,10); c.moveTo(6,9); c.lineTo(6,21); c.lineTo(18,21); c.lineTo(18,9); c.moveTo(10,21); c.lineTo(10,15); c.lineTo(14,15); c.lineTo(14,21) }
                            else if (index === 1) { c.rect(3,6,18,13); c.moveTo(15,6); c.lineTo(15,9); c.moveTo(15,12); c.lineTo(15,14); c.moveTo(15,17); c.lineTo(15,19) }
                            else if (index === 2) { c.arc(12,7,4,0,Math.PI*2); c.moveTo(4,21); c.bezierCurveTo(4,12,20,12,20,21) }
                            else if (index === 3) { c.moveTo(3,6); c.lineTo(21,6); c.moveTo(3,12); c.lineTo(21,12); c.moveTo(3,18); c.lineTo(21,18); c.moveTo(8,3); c.lineTo(8,9); c.moveTo(16,9); c.lineTo(16,15); c.moveTo(10,15); c.lineTo(10,21) }
                            else { c.arc(12,12,9,0,Math.PI*2); c.moveTo(9,8); c.bezierCurveTo(9,4,18,6,13,11); c.lineTo(12,14); c.moveTo(12,18); c.lineTo(12,18.5) }
                            c.stroke()
                        }
                    }
                    Label { Layout.alignment: Qt.AlignHCenter; text: window.tr(modelData); font.pixelSize: 10; font.bold: index === window.currentTab }
                }
                Accessible.name: window.tr(modelData)
            }
        }
    }
    Loader {
        id: pageLoader
        objectName: "pageLoader"
        anchors.fill: parent; anchors.margins: 16
        active: !appSettings.locked
        sourceComponent: [homePage, ticketsPage, profilePage, settingsPage, supportPage][window.currentTab]
        onLoaded: { opacity = 0; entrance.restart() }
        NumberAnimation { id: entrance; target: pageLoader; property: "opacity"; from: 0; to: 1; duration: 140; easing.type: Easing.OutQuad }
    }
    Component { id: homePage; HomePage { ui: window } }
    Component { id: ticketsPage; TicketsPage { ui: window; tickets: controller } }
    Component { id: profilePage; ProfilePage { ui: window } }
    Component { id: settingsPage; SettingsPage { ui: window } }
    Component { id: supportPage; SupportPage { ui: window } }
    Loader {
        anchors.fill: parent
        active: appSettings.locked
        sourceComponent: Pane {
            padding: 0
            readonly property color lockText: appSettings.darkMode ? "#f3f4f6" : "#172033"
            readonly property color lockBackground: appSettings.darkMode ? "#101b20" : "#f5f7fb"
            Material.foreground: lockText
            Material.background: lockBackground
            palette.text: lockText
            palette.windowText: lockText
            palette.buttonText: lockText
            palette.base: lockBackground
            background: Rectangle { color: parent.lockBackground }
            ScrollView {
                anchors.fill: parent; anchors.margins: 24; contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width; spacing: 18
                    Item { implicitHeight: 40 }
                    Label { text: "UMIT"; font.pixelSize: 36; font.bold: true; color: appSettings.darkMode ? "#f3f4f6" : "#172033" }
                    UText { text: window.tr("lockedTitle"); font.pixelSize: 23; color: appSettings.darkMode ? "white" : "#172033" }
                    UText { text: window.tr("lockedDetail"); color: appSettings.darkMode ? "white" : "#172033" }
                    TextField { id: unlockPassword; objectName: "unlockPassword"; Layout.fillWidth: true; echoMode: TextInput.Password; maximumLength: 128; placeholderText: window.tr("password"); enabled: !appSettings.busy; color: appSettings.darkMode ? "white" : "#172033"; onAccepted: unlockButton.clicked() }
                    UText { text: appSettings.lockRemainingSeconds > 0 ? window.tr("retryAfter").replace("{n}", appSettings.lockRemainingSeconds) : window.message(appSettings.errorCode); visible: text.length > 0; color: appSettings.darkMode ? "#ffb4ab" : "#a02d31" }
                    UButton {
                        id: unlockButton; objectName: "unlockButton"; Layout.fillWidth: true
                        text: appSettings.busy ? window.tr("working") : window.tr("unlock"); highlighted: true
                        enabled: !appSettings.busy && appSettings.lockRemainingSeconds === 0 && unlockPassword.text.length > 0
                        onClicked: { appSettings.unlock(unlockPassword.text); unlockPassword.clear(); Qt.inputMethod.hide() }
                    }
                    BusyIndicator { Layout.alignment: Qt.AlignHCenter; running: appSettings.busy; visible: running }
                }
            }
        }
    }
}
