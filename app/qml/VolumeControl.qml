// Aurora Player - mute toggle + volume slider with scroll support.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: root

    property var player: null

    spacing: Theme.spacing2

    IconButton {
        icon: {
            if (!root.player || root.player.muted) return "mute"
            return root.player.volume > 0.5 ? "volume" : "volumeLow"
        }
        variant: "ghost"
        tooltip: qsTr("Mute")
        onClicked: if (root.player) root.player.toggleMute()
    }

    Slider {
        id: slider
        Layout.fillWidth: true
        from: 0
        to: 1
        value: root.player ? root.player.volume : 0.85
        onMoved: if (root.player) root.player.setVolume(value)

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 4
            radius: 2
            color: Theme.dark ? Qt.rgba(1, 1, 1, 0.20) : Qt.rgba(0, 0, 0, 0.12)

            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: parent.radius
                color: root.player && root.player.muted ? Theme.textMuted : Theme.accent
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.pressed || slider.hovered ? 14 : 11
            height: width
            radius: width / 2
            color: Theme.dark ? "#FFFFFF" : "#FFFFFF"
            border.width: 1
            border.color: Theme.dark ? Qt.rgba(0, 0, 0, 0.25) : Theme.borderStrong

            Behavior on width {
                NumberAnimation { duration: Theme.durationFast }
            }
        }

        WheelHandler {
            target: null
            onWheel: (event) => {
                if (!root.player) return
                var step = event.angleDelta.y > 0 ? 0.05 : -0.05
                root.player.setVolume(Math.max(0, Math.min(1, root.player.volume + step)))
            }
        }
    }
}
