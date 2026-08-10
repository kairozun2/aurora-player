// Aurora Player - iOS style segmented switch used across settings.
import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: control

    property var options: []
    property int currentIndex: 0

    signal selected(int index)

    implicitWidth: layout.implicitWidth + 6
    implicitHeight: 34
    radius: Theme.radiusPill
    color: Theme.dark ? Qt.rgba(1, 1, 1, 0.08) : Theme.surfaceRaised
    border.width: Theme.dark ? 0 : 1
    border.color: Theme.border

    // Sliding pill that marks the active option.
    Rectangle {
        id: marker
        height: parent.height - 6
        width: layout.children.length > 0 && control.currentIndex < layout.children.length
               ? layout.children[control.currentIndex].width : 0
        x: layout.children.length > 0 && control.currentIndex < layout.children.length
           ? layout.children[control.currentIndex].x + 3 : 3
        y: 3
        radius: height / 2
        color: Theme.accent

        Behavior on x {
            NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easeStandard }
        }
        Behavior on width {
            NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easeStandard }
        }
    }

    Row {
        id: layout
        anchors.centerIn: parent
        spacing: 0

        Repeater {
            model: control.options

            delegate: AbstractButton {
                required property int index
                required property string modelData

                height: control.height - 6
                width: Math.max(56, segmentLabel.implicitWidth + Theme.spacing4)
                hoverEnabled: true

                background: null

                contentItem: Text {
                    id: segmentLabel
                    text: modelData
                    color: control.currentIndex === index ? Theme.accentText
                                                          : (parent.hovered ? Theme.text : Theme.textSecondary)
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontCaption
                    font.weight: control.currentIndex === index ? Theme.weightSemi : Theme.weightRegular
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                }

                onClicked: {
                    control.currentIndex = index
                    control.selected(index)
                }
            }
        }
    }
}
