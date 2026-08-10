// Aurora Player - the only button the UI needs.
//
// Three shapes: "ghost" (transparent until hovered), "soft" (light grey pill,
// like the transport buttons in the reference design) and "accent" (filled).
import QtQuick
import QtQuick.Controls.Basic

AbstractButton {
    id: control

    property string icon: "play"
    property string variant: "soft"        // ghost | soft | accent
    property int diameter: Theme.iconButton
    property real iconScale: 0.5
    property bool active: false            // sticky "on" state for toggles
    property string tooltip: ""
    property color customColor: "transparent"

    implicitWidth: diameter
    implicitHeight: diameter
    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    readonly property color baseColor: {
        if (variant === "accent" || active) return Theme.accent
        if (variant === "ghost") return "transparent"
        return Theme.dark ? Qt.rgba(1, 1, 1, 0.10) : Theme.surfaceRaised
    }

    readonly property color contentColor: {
        if (customColor.a > 0) return customColor
        if (variant === "accent" || active) return Theme.accentText
        return control.hovered ? Theme.text : Theme.textSecondary
    }

    background: Rectangle {
        radius: control.diameter / 2
        color: {
            var base = control.baseColor
            if (control.pressed) return Qt.darker(base.a > 0 ? base : Theme.surfaceRaised, 1.18)
            if (control.hovered) {
                if (control.variant === "ghost")
                    return Theme.dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.05)
                return Qt.lighter(base, Theme.dark ? 1.35 : 1.05)
            }
            return base
        }
        border.width: control.variant === "soft" && !Theme.dark ? 1 : 0
        border.color: Theme.border

        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }
    }

    contentItem: Item {
        IconGlyph {
            anchors.centerIn: parent
            width: Math.round(control.diameter * control.iconScale)
            height: width
            name: control.icon
            color: control.contentColor
            strokeWidth: 1.9 * (24 / Math.max(width, 1)) * (width / 24) * 1.0
        }
    }

    scale: pressed ? 0.94 : 1.0
    Behavior on scale {
        NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easeStandard }
    }

    ToolTip.visible: control.tooltip !== "" && control.hovered
    ToolTip.delay: 450
    ToolTip.text: control.tooltip
}
