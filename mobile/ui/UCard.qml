import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: card
    required property var ui
    default property alias items: body.data
    Layout.fillWidth: true
    padding: 18
    background: Rectangle {
        radius: 20
        color: card.ui.surfaceColor
        border.color: card.ui.borderColor
        border.width: 1
    }
    contentItem: ColumnLayout { id: body; spacing: 12 }
}
