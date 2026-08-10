// Aurora Player - the toggle used everywhere in settings.
import QtQuick
import QtQuick.Controls.Basic

AbstractButton {
    id: control

    property bool checked: false

    signal toggled(bool value)

    implicitWidth: 48
    implicitHeight: 28
    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    background: Rectangle {
        radius: height / 2
        color: control.checked ? Theme.accent
                               : (Theme.dark ? Qt.rgba(1, 1, 1, 0.18) : "#D8D6D3")
        border.width: control.hovered ? 1 : 0
        border.color: Theme.alpha(Theme.text, 0.18)

        Behavior on color { ColorAnimation { duration: Theme.durationFast } }

        Rectangle {
            width: 22
            height: 22
            radius: 11
            y: 3
            x: control.checked ? parent.width - width - 3 : 3
            color: "#FFFFFF"

            Behavior on x {
                NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easeStandard }
            }
        }
    }

    onClicked: {
        checked = !checked
        toggled(checked)
    }
}
