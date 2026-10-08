import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    required property var ui
    clip: true; contentWidth: availableWidth; contentHeight: body.implicitHeight
    ColumnLayout {
        id: body; width: page.availableWidth; spacing: 16
        UCard {
            ui: page.ui
            UText { text: page.ui.tr("helpTitle"); font.pixelSize: 28; font.bold: true }
            UText { text: page.ui.tr("helpDetail") }
        }
        UCard {
            ui: page.ui
            UText { text: page.ui.tr("email"); font.bold: true }
            UText { text: appInfo.supportEmail || page.ui.tr("contactMissing"); wrapMode: Text.WrapAnywhere }
            UButton { Layout.fillWidth: true; text: page.ui.tr("writeEmail"); enabled: appInfo.supportEmail.length > 0; onClicked: Qt.openUrlExternally("mailto:" + encodeURIComponent(appInfo.supportEmail)) }
            UText { text: page.ui.tr("phone"); font.bold: true }
            UText { text: appInfo.supportPhone || page.ui.tr("contactMissing") }
            UButton { Layout.fillWidth: true; text: page.ui.tr("call"); enabled: appInfo.supportPhone.length > 0; onClicked: Qt.openUrlExternally("tel:" + appInfo.supportPhone.replace(/[^+0-9]/g, "")) }
            UText { text: page.ui.tr("contactsMissing"); visible: !appInfo.supportEmail.length && !appInfo.supportPhone.length; font.pixelSize: 12 }
        }
    }
}
