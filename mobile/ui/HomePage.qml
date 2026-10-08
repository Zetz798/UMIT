import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    required property var ui
    clip: true
    contentWidth: availableWidth
    contentHeight: body.implicitHeight
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ColumnLayout {
        id: body
        width: page.availableWidth
        spacing: 18
        UCard {
            ui: page.ui
            UText { text: "UMIT"; font.pixelSize: 16; font.letterSpacing: 4; font.bold: true }
            UText { text: page.ui.tr("tagline"); font.pixelSize: 34; font.bold: true }
            UText { text: page.ui.tr("about"); font.pixelSize: 15 }
            UButton {
                Layout.fillWidth: true
                text: page.ui.tr("getStarted")
                highlighted: true
                implicitHeight: 54
                onClicked: page.ui.currentTab = 1
            }
        }
        UText { text: page.ui.tr("howItWorks"); font.pixelSize: 20; font.bold: true }
        UCard {
            ui: page.ui
            UText { text: page.ui.tr("stepOne"); font.bold: true }
            UText { text: page.ui.tr("stepOneDetail") }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: page.ui.borderColor }
            UText { text: page.ui.tr("stepTwo"); font.bold: true }
            UText { text: page.ui.tr("stepTwoDetail") }
        }
        UCard {
            ui: page.ui
            UText { text: page.ui.tr("demoTitle"); font.bold: true }
            UText { text: page.ui.tr("demoDetail"); font.pixelSize: 13 }
        }
        Item { implicitHeight: 6 }
    }
}
