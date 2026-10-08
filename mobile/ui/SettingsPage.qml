import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    required property var ui
    property string colorError: ""
    property string passwordNotice: ""
    clip: true; contentWidth: availableWidth; contentHeight: body.implicitHeight
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    function clearPasswords() { currentPassword.clear(); newPassword.clear(); confirmation.clear() }
    function luminance(hex) {
        var values = [hex.slice(1,3), hex.slice(3,5), hex.slice(5,7)].map(function(v) {
            var n = parseInt(v,16)/255; return n <= 0.04045 ? n/12.92 : Math.pow((n+0.055)/1.055,2.4)
        })
        return values[0]*0.2126 + values[1]*0.7152 + values[2]*0.0722
    }
    function applyColors() {
        var bg = backgroundInput.text.trim(), fg = textInput.text.trim()
        colorError = ""
        if (!/^#[0-9a-fA-F]{6}$/.test(bg) || !/^#[0-9a-fA-F]{6}$/.test(fg)) { colorError = "invalid_color"; return }
        var a = luminance(bg), b = luminance(fg)
        if ((Math.max(a,b)+0.05)/(Math.min(a,b)+0.05) < 4.5) { colorError = "low_contrast"; return }
        appSettings.clearError(); appSettings.backgroundColor = bg; appSettings.textColor = fg; Qt.inputMethod.hide()
    }
    Component.onDestruction: clearPasswords()
    Connections {
        target: appSettings
        function onColorsChanged() { backgroundInput.text = appSettings.backgroundColor.toString(); textInput.text = appSettings.textColor.toString() }
        function onOperationFinished(success) { if (success) { page.clearPasswords(); page.passwordNotice = "saved" } }
    }
    ColumnLayout {
        id: body; width: page.availableWidth; spacing: 16
        UCard {
            ui: page.ui
            UText { text: page.ui.tr("appearance"); font.pixelSize: 22; font.bold: true }
            UText { text: page.ui.tr("language") }
            ComboBox {
                objectName: "languageSelector"; Layout.fillWidth: true; model: ["Русский", "Қазақша", "English"]
                currentIndex: ["ru","kk","en"].indexOf(appSettings.language)
                onActivated: { appSettings.clearError(); appSettings.language = ["ru","kk","en"][currentIndex] }
            }
            Switch { objectName: "darkSwitch"; Layout.fillWidth: true; text: page.ui.tr("darkMode"); checked: appSettings.darkMode; onToggled: { appSettings.clearError(); appSettings.darkMode = checked } }
        }
        UCard {
            ui: page.ui
            UText { text: page.ui.tr("colors"); font.bold: true; font.pixelSize: 20 }
            UText { text: page.ui.tr("backgroundColor") }
            TextField { id: backgroundInput; Layout.fillWidth: true; maximumLength: 7; text: appSettings.backgroundColor.toString(); placeholderText: "#RRGGBB" }
            UText { text: page.ui.tr("textColor") }
            TextField { id: textInput; Layout.fillWidth: true; maximumLength: 7; text: appSettings.textColor.toString(); placeholderText: "#RRGGBB" }
            Rectangle { Layout.fillWidth: true; implicitHeight: 50; radius: 10; color: /^#[0-9a-fA-F]{6}$/.test(backgroundInput.text) ? backgroundInput.text : appSettings.backgroundColor
                Label { anchors.centerIn: parent; text: "UMIT"; font.bold: true; color: /^#[0-9a-fA-F]{6}$/.test(textInput.text) ? textInput.text : appSettings.textColor }
            }
            UText { text: page.ui.message(page.colorError); visible: text.length > 0 }
            UButton { Layout.fillWidth: true; text: page.ui.tr("applyColors"); onClicked: page.applyColors() }
            UButton { Layout.fillWidth: true; text: page.ui.tr("resetColors"); onClicked: { page.colorError = ""; appSettings.clearError(); appSettings.resetColors() } }
        }
        UCard {
            ui: page.ui
            UText { text: page.ui.tr("security"); font.pixelSize: 22; font.bold: true }
            UText { text: page.ui.tr(appSettings.lockEnabled ? "lockOn" : "lockOff") }
            UText { text: page.ui.tr("lockDetail"); font.pixelSize: 12 }
            TextField { id: currentPassword; Layout.fillWidth: true; visible: appSettings.lockEnabled; maximumLength: 128; echoMode: TextInput.Password; placeholderText: page.ui.tr("currentPassword"); enabled: !appSettings.busy }
            TextField { id: newPassword; Layout.fillWidth: true; maximumLength: 128; echoMode: TextInput.Password; placeholderText: page.ui.tr("newPassword"); enabled: !appSettings.busy }
            TextField { id: confirmation; Layout.fillWidth: true; maximumLength: 128; echoMode: TextInput.Password; placeholderText: page.ui.tr("confirmPassword"); enabled: !appSettings.busy }
            UButton {
                Layout.fillWidth: true; enabled: !appSettings.busy
                text: appSettings.busy ? page.ui.tr("working") : page.ui.tr(appSettings.lockEnabled ? "changePassword" : "setPassword")
                onClicked: { page.passwordNotice = ""; appSettings.setAppPassword(currentPassword.text, newPassword.text, confirmation.text); Qt.inputMethod.hide() }
            }
            UButton { Layout.fillWidth: true; visible: appSettings.lockEnabled; enabled: !appSettings.busy; text: page.ui.tr("disablePassword"); onClicked: { page.passwordNotice = ""; appSettings.disableAppPassword(currentPassword.text); Qt.inputMethod.hide() } }
            UButton { Layout.fillWidth: true; visible: appSettings.lockEnabled; text: page.ui.tr("lockNow"); onClicked: { page.clearPasswords(); appSettings.lockNow() } }
            UText { text: page.ui.message(appSettings.errorCode || page.passwordNotice); visible: text.length > 0 }
            BusyIndicator { Layout.alignment: Qt.AlignHCenter; running: appSettings.busy; visible: running }
        }
        UCard {
            ui: page.ui
            UText { text: page.ui.tr("sessions"); font.pixelSize: 22; font.bold: true }
            UText { text: wallet.connected ? wallet.shortAddress : page.ui.tr("walletNotConnected") }
            UButton { Layout.fillWidth: true; text: page.ui.tr("disconnectWallet"); enabled: wallet.connected || wallet.busy; onClicked: wallet.resetConnection() }
            UText { text: page.ui.tr("disconnectDetail"); font.pixelSize: 12 }
            UButton { Layout.fillWidth: true; text: page.ui.tr("logout"); enabled: account.loggedIn && !account.busy; onClicked: { page.clearPasswords(); account.logout() } }
            UText { text: page.ui.tr("logoutDetail"); font.pixelSize: 12 }
            UText { visible: !account.loggedIn; text: page.ui.tr("notSignedIn") }
        }
    }
}
