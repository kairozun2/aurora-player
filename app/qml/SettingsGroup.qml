// Aurora Player - titled card that groups related settings.
import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: group

    property string title: ""
    default property alias groupContent: inner.data

    spacing: Theme.spacing3

    Text {
        text: group.title
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontCaption
        font.weight: Theme.weightSemi
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.6
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: inner.implicitHeight + Theme.spacing4 * 2
        radius: Theme.radiusLg
        color: Theme.dark ? Qt.rgba(1, 1, 1, 0.045) : Theme.surface
        border.width: 1
        border.color: Theme.border

        ColumnLayout {
            id: inner
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.spacing4
            spacing: Theme.spacing4
        }
    }
}
