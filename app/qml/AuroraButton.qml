// Aurora Player - the pill button used for primary and secondary actions.
//
// Deliberately built on Rectangle instead of AbstractButton: `text` and `icon`
// would shadow AbstractButton's own properties, and a plain Rectangle gives
// full control over the pill shape, the hover tint and the icon placement.
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: button

    property string text: ""
    property string icon: ""
    property bool iconTrailing: false
    property bool primary: false
    property bool busy: false

    signal clicked()

    readonly property bool interactive: enabled && !busy
    readonly property color foreground: primary ? Theme.accentText : Theme.text

    implicitWidth: content.implicitWidth + Theme.spacing5 * 2
    implicitHeight: Theme.controlHeight
    radius: Theme.radiusPill
    opacity: enabled ? 1.0 : 0.45

    color: primary
           ? (mouse.pressed ? Qt.darker(Theme.accent, 1.12)
                            : (mouse.containsMouse ? Qt.lighter(Theme.accent, 1.06)
                                                   : Theme.accent))
           : (mouse.pressed ? Theme.surfaceHover
                            : (mouse.containsMouse ? Theme.surfaceRaised
                                                   : Theme.alpha(Theme.text, Theme.dark ? 0.07 : 0.04)))
    border.width: primary ? 0 : 1
    border.color: primary ? "transparent" : Theme.border

    scale: mouse.pressed && interactive ? 0.97 : 1.0

    Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }
    Behavior on scale {
        NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easeStandard }
    }

    RowLayout {
        id: content
        anchors.centerIn: parent
        spacing: Theme.spacing2
        layoutDirection: button.iconTrailing ? Qt.RightToLeft : Qt.LeftToRight

        IconGlyph {
            visible: button.icon !== ""
            name: button.icon === "" ? "play" : button.icon
            color: button.foreground
            implicitWidth: Theme.iconSize
            implicitHeight: Theme.iconSize
        }

        Text {
            visible: button.text !== ""
            text: button.text
            color: button.foreground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.weight: Theme.weightMedium
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: button.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: if (button.interactive) button.clicked()
    }
}
