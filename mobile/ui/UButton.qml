import QtQuick
import QtQuick.Controls

Button {
    id: control
    padding: 14
    implicitHeight: Math.max(50, label.implicitHeight + 24)
    contentItem: Text {
        id: label
        text: control.text
        textFormat: Text.PlainText
        font: control.font
        color: control.highlighted ? control.palette.highlightedText : control.palette.buttonText
        opacity: control.enabled ? 1 : 0.5
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
    }
    background: Rectangle {
        radius: 12
        color: control.highlighted ? control.palette.highlight : control.palette.button
        opacity: control.enabled ? (control.down ? 0.75 : 1) : 0.35
        border.width: control.highlighted ? 0 : 1
        border.color: Qt.rgba(control.palette.buttonText.r, control.palette.buttonText.g, control.palette.buttonText.b, 0.18)
        Behavior on opacity { NumberAnimation { duration: 90 } }
    }
}
