import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    required property var ui
    required property var tickets
    property int mode: 0
    clip: true; contentWidth: availableWidth; contentHeight: body.implicitHeight
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ColumnLayout {
        id: body; width: page.availableWidth; spacing: 16
        UCard {
            ui: page.ui; visible: !account.loggedIn
            UText { text: page.ui.tr("signInFirst"); font.pixelSize: 22; font.bold: true }
            UText { text: page.ui.tr("signInFirstDetail") }
            UButton { Layout.fillWidth: true; text: page.ui.tr("openAccount"); onClicked: page.ui.openAccount() }
        }
        ColumnLayout {
            visible: account.loggedIn; Layout.fillWidth: true; spacing: 16
            UText { text: page.ui.tr("event"); font.pixelSize: 20; font.bold: true }
            TabBar {
                Layout.fillWidth: true; currentIndex: page.mode
                onCurrentIndexChanged: { page.mode = currentIndex; page.tickets.clearResult() }
                TabButton { text: page.ui.tr("issue") }
                TabButton { text: page.ui.tr("verify") }
            }
            UCard {
                ui: page.ui; visible: page.mode === 0
                UText { text: page.ui.tr("issueTitle"); font.bold: true; font.pixelSize: 22 }
                UText { text: page.ui.tr("issueDetail") }
                UButton { objectName: "issueTicketButton"; Layout.fillWidth: true; implicitHeight: 52; text: page.ui.tr("createTicket"); highlighted: true; onClicked: page.tickets.issueTicket() }
                UText { text: page.ui.tr("lastTicket") }
                UText { text: page.tickets.lastTicketId || page.ui.tr("noTicket"); font.pixelSize: 26; font.bold: true; wrapMode: Text.WrapAnywhere }
                UButton { Layout.fillWidth: true; text: page.ui.tr("goCheck"); enabled: page.tickets.lastTicketId.length > 0; onClicked: { ticketInput.text = page.tickets.lastTicketId; page.mode = 1 } }
            }
            UCard {
                ui: page.ui; visible: page.mode === 1
                UText { text: page.ui.tr("checkTitle"); font.bold: true; font.pixelSize: 22 }
                UText { text: page.ui.tr("checkDetail") }
                TextField { id: ticketInput; Layout.fillWidth: true; maximumLength: 20; inputMethodHints: Qt.ImhDigitsOnly; placeholderText: page.ui.tr("ticketPlaceholder"); onTextChanged: page.tickets.clearResult() }
                UButton { Layout.fillWidth: true; implicitHeight: 52; text: page.ui.tr("redeem"); highlighted: true; enabled: ticketInput.text.trim().length > 0; onClicked: { Qt.inputMethod.hide(); page.tickets.redeemTicket(ticketInput.text) } }
            }
            UCard {
                ui: page.ui; visible: page.tickets.resultKind !== "idle"
                UText { text: page.tickets.statusText; font.bold: true; font.pixelSize: 22 }
                UText { text: page.tickets.detailText }
            }
            UText { text: page.ui.tr("demoDetail"); font.pixelSize: 12; opacity: 0.7 }
        }
    }
}
