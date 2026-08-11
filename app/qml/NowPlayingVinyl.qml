// Aurora Player - "vinyl" now playing card.
//
// The record slides out from behind the sleeve and keeps spinning while the
// track plays, exactly like the reference design. Everything is drawn with
// gradients, so no artwork is needed for the disc itself.
import QtQuick
import QtQuick.Effects

Item {
    id: root

    property var player: null
    property string coverUrl: player ? player.coverUrl : ""
    property bool playing: player ? player.playing : false
    property int sleeveSize: Math.round(Math.min(width * 0.62, height))

    implicitWidth: 420
    implicitHeight: 260

    // ------------------------------------------------------------- record ---
    Item {
        id: disc
        width: root.sleeveSize
        height: root.sleeveSize
        y: (root.height - height) / 2
        x: sleeve.x + (root.playing ? root.sleeveSize * 0.52 : root.sleeveSize * 0.08)
        z: 0

        Behavior on x {
            NumberAnimation { duration: Theme.durationSlow; easing.type: Theme.easeStandard }
        }

        Rectangle {
            id: vinyl
            anchors.fill: parent
            radius: width / 2
            color: "#121212"
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#0C0C0C" }
                GradientStop { position: 0.45; color: "#1E1E1E" }
                GradientStop { position: 0.55; color: "#111111" }
                GradientStop { position: 1.0; color: "#050505" }
            }

            // Grooves.
            Repeater {
                model: 9
                delegate: Rectangle {
                    required property int index
                    anchors.centerIn: parent
                    width: vinyl.width * (0.42 + index * 0.062)
                    height: width
                    radius: width / 2
                    color: "transparent"
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, index % 2 === 0 ? 0.055 : 0.028)
                }
            }

            // Label in the middle, tinted with the cover accent.
            Rectangle {
                anchors.centerIn: parent
                width: vinyl.width * 0.34
                height: width
                radius: width / 2
                color: Theme.coverAccent

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * 0.16
                    height: width
                    radius: width / 2
                    color: Theme.dark ? "#141210" : "#FFFFFF"
                }
            }

            RotationAnimator {
                target: vinyl
                from: 0
                to: 360
                duration: 3400
                loops: Animation.Infinite
                running: root.playing
            }
        }
    }

    // ------------------------------------------------------------- sleeve ---
    Rectangle {
        id: sleeve
        width: root.sleeveSize
        height: root.sleeveSize
        x: 0
        y: (root.height - height) / 2
        z: 1
        radius: Theme.radiusMd
        color: Theme.surfaceRaised
        clip: true

        Image {
            id: sleeveSource
            anchors.fill: parent
            source: root.coverUrl
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            visible: false
        }

        Rectangle {
            id: sleeveMask
            anchors.fill: parent
            radius: parent.radius
            visible: false
            layer.enabled: true
        }

        MultiEffect {
            anchors.fill: parent
            source: sleeveSource
            maskEnabled: true
            maskSource: sleeveMask
        }

        // Sleeve spine highlight.
        Rectangle {
            width: 3
            height: parent.height
            color: Qt.rgba(0, 0, 0, 0.18)
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: if (root.player) root.player.togglePlayPause()
    }
}
