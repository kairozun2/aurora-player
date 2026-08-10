// Aurora Player - label + hint on the left, control on the right.
import QtQuick
import QtQuick.Layouts

RowLayout {
    id: row

    property string label: ""
    property string hint: ""
    default property alias controlArea: holder.data

    Layout.fillWidth: true
    spacing: Theme.spacing4

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        Text {
            Layout.fillWidth: true
            text: row.label
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.weight: Theme.weightMedium
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            visible: row.hint !== ""
            text: row.hint
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }

    Item {
        id: holder
        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height
    }
}
