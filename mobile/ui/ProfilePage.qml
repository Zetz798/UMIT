import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ScrollView {
    id: page
    required property var ui
    property bool registering: false
    clip: true; contentWidth: availableWidth; contentHeight: body.implicitHeight
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    function clearPasswords() { password.clear(); confirmation.clear() }
    Component.onDestruction: clearPasswords()
    Connections {
        target: account
        function onSessionChanged() { page.clearPasswords(); nicknameEdit.text = account.displayName }
    }
    FileDialog {
        id: avatarPicker; title: page.ui.tr("changeAvatar")
        nameFilters: [page.ui.tr("imageFilter")]
        onAccepted: account.setAvatar(selectedFile)
    }
    ColumnLayout {
        id: body; width: page.availableWidth; spacing: 16
        UCard {
            ui: page.ui; visible: !account.loggedIn
            UText { text: page.ui.tr("signInFirst"); font.bold: true; font.pixelSize: 24 }
            TabBar {
                Layout.fillWidth: true; currentIndex: page.registering ? 1 : 0
                onCurrentIndexChanged: { page.registering = currentIndex === 1; page.clearPasswords(); account.clearFeedback() }
                TabButton { text: page.ui.tr("login") }
                TabButton { text: page.ui.tr("register") }
            }
            TextField { id: email; objectName: "accountEmail"; Layout.fillWidth: true; maximumLength: 254; placeholderText: page.ui.tr("email"); inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoPredictiveText; enabled: !account.busy }
            TextField { id: nickname; objectName: "registerNickname"; Layout.fillWidth: true; maximumLength: 32; visible: page.registering; placeholderText: page.ui.tr("nickname"); enabled: !account.busy }
            TextField { id: password; objectName: "accountPassword"; Layout.fillWidth: true; maximumLength: 128; echoMode: TextInput.Password; placeholderText: page.ui.tr("password"); enabled: !account.busy }
            UText { text: page.ui.tr("passwordHint"); font.pixelSize: 12; visible: page.registering }
            TextField { id: confirmation; objectName: "registerConfirmation"; Layout.fillWidth: true; maximumLength: 128; echoMode: TextInput.Password; visible: page.registering; placeholderText: page.ui.tr("confirmPassword"); enabled: !account.busy }
            UButton {
                objectName: "authenticateButton"; Layout.fillWidth: true; implicitHeight: 52; highlighted: true; enabled: !account.busy
                text: account.busy ? page.ui.tr("working") : page.ui.tr(page.registering ? "registerAction" : "loginAction")
                onClicked: { Qt.inputMethod.hide(); if (page.registering) account.registerAccount(email.text, password.text, confirmation.text, nickname.text); else account.login(email.text, password.text) }
            }
        }
        UCard {
            ui: page.ui; visible: account.loggedIn
            Rectangle {
                Layout.alignment: Qt.AlignHCenter; implicitWidth: 104; implicitHeight: 104; radius: 26; color: page.ui.borderColor
                Image { anchors.fill: parent; anchors.margins: 4; source: account.avatarUrl; fillMode: Image.PreserveAspectFit; asynchronous: true; cache: false; visible: source.toString().length > 0 }
                Label { anchors.centerIn: parent; text: account.displayName.slice(0, 1).toUpperCase(); font.pixelSize: 40; visible: account.avatarUrl.toString().length === 0 }
            }
            UText { text: account.displayName; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 24; font.bold: true; wrapMode: Text.WrapAnywhere }
            UText { text: account.email; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WrapAnywhere; opacity: 0.7 }
            TextField { id: nicknameEdit; Layout.fillWidth: true; maximumLength: 32; text: account.displayName; placeholderText: page.ui.tr("nickname"); enabled: !account.busy }
            UButton { Layout.fillWidth: true; text: page.ui.tr("saveProfile"); enabled: !account.busy; onClicked: { Qt.inputMethod.hide(); account.updateProfile(nicknameEdit.text) } }
            UButton { Layout.fillWidth: true; text: page.ui.tr("changeAvatar"); enabled: !account.busy; onClicked: avatarPicker.open() }
            UButton { Layout.fillWidth: true; text: page.ui.tr("removeAvatar"); enabled: !account.busy && account.avatarUrl.toString().length > 0; onClicked: account.removeAvatar() }
        }
        UText { text: page.ui.message(account.errorCode || account.noticeCode); visible: text.length > 0; font.bold: true }
        BusyIndicator { Layout.alignment: Qt.AlignHCenter; running: account.busy; visible: running }
        UCard {
            ui: page.ui; visible: account.loggedIn
            UText { text: page.ui.tr("walletTitle"); font.bold: true; font.pixelSize: 20 }
            UText { text: page.ui.tr("walletDetail") }
            UButton { Layout.fillWidth: true; text: wallet.buttonText; enabled: !wallet.busy; onClicked: wallet.connectWallet() }
            UText { text: wallet.balanceText; font.bold: true }
            UText { text: wallet.statusText; font.pixelSize: 13 }
            UButton { Layout.fillWidth: true; text: page.ui.tr("refreshBalance"); visible: wallet.connected; enabled: !wallet.refreshing; onClicked: wallet.refreshBalance() }
            UButton { Layout.fillWidth: true; text: page.ui.tr("cancel"); visible: wallet.busy; onClicked: wallet.resetConnection() }
        }
        UCard {
            ui: page.ui; visible: account.isLocal
            UText { text: page.ui.tr("localAccount"); font.bold: true }
            UText { text: page.ui.tr("localAccountDetail"); font.pixelSize: 12 }
        }
    }
}
