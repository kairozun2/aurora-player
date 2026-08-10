// Aurora Player - waveform scrubber.
//
// The peaks come from the C++ analyser (cached per track), so scrubbing shows
// the real shape of the song instead of a plain line. Falls back to a slim
// progress bar while the analysis is still running.
import QtQuick

Item {
    id: root

    property var peaks: []                 // 0..1 floats
    property real position: 0              // 0..1
    property color playedColor: Theme.accent
    property color remainingColor: Theme.dark ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(0, 0, 0, 0.14)
    property real barWidth: 3
    property real barSpacing: 2
    property bool interactive: true

    signal seekRequested(real fraction)

    implicitHeight: 48

    readonly property int barCount: Math.max(1, Math.floor(width / (barWidth + barSpacing)))
    readonly property bool hasPeaks: peaks && peaks.length > 0

    // Resample the analyser output to however many bars fit right now.
    function peakAt(index) {
        if (!hasPeaks) return 0.35
        var i = Math.floor(index * peaks.length / barCount)
        return Math.max(0.06, Math.min(1, peaks[Math.min(i, peaks.length - 1)]))
    }

    Row {
        id: bars
        anchors.centerIn: parent
        spacing: root.barSpacing
        visible: root.hasPeaks

        Repeater {
            model: root.barCount
            delegate: Rectangle {
                required property int index
                width: root.barWidth
                radius: width / 2
                height: Math.max(root.barWidth, root.peakAt(index) * root.height)
                anchors.verticalCenter: parent.verticalCenter
                color: (index / root.barCount) <= root.position ? root.playedColor : root.remainingColor
                opacity: (index / root.barCount) <= root.position ? 1 : 0.85

                Behavior on height {
                    NumberAnimation { duration: Theme.durationSlow; easing.type: Theme.easeStandard }
                }
                Behavior on color {
                    ColorAnimation { duration: Theme.durationFast }
                }
            }
        }
    }

    // Slim fallback bar while peaks are still being computed.
    Rectangle {
        visible: !root.hasPeaks
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: 4
        radius: 2
        color: root.remainingColor

        Rectangle {
            width: parent.width * Math.max(0, Math.min(1, root.position))
            height: parent.height
            radius: parent.radius
            color: root.playedColor
        }
    }

    // Hover playhead + drag-to-seek.
    Rectangle {
        visible: hover.hovered && root.interactive
        width: 2
        height: parent.height
        radius: 1
        color: Theme.alpha(Theme.text, 0.5)
        x: Math.max(0, Math.min(root.width - width, hover.mouseX))
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        enabled: root.interactive
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        property bool hovered: containsMouse

        function emitSeek(x) {
            root.seekRequested(Math.max(0, Math.min(1, x / Math.max(1, root.width))))
        }

        onClicked: (mouse) => emitSeek(mouse.x)
        onPositionChanged: (mouse) => { if (pressed) emitSeek(mouse.x) }
    }
}
