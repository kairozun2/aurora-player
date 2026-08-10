// Aurora Player - floating transport bar.
//
// Sits above the content as a frosted glass pill: cover, metadata, transport,
// waveform scrubber, lyrics/queue toggles and volume.
import QtQuick
import QtQuick.Layouts

GlassCard {
    id: bar

    property var player: null              // PlayerBridge
    property Item backdropSource: null
    property bool lyricsOpen: false
    property bool queueOpen: false

    signal toggleLyrics()
    signal toggleQueue()
    signal openNowPlaying()

    radius: Theme.radiusXl
    backdrop: backdropSource
    implicitHeight: Theme.playerBarHeight

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacing4
        anchors.rightMargin: Theme.spacing4
        spacing: Theme.spacing4

        // ------------------------------------------------------ cover + text --
        Item {
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            Layout.alignment: Qt.AlignVCenter

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusMd
                color: Theme.surfaceRaised
                clip: true

                Image {
                    anchors.fill: parent
                    source: bar.player ? bar.player.coverUrl : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: true
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: bar.openNowPlaying()
            }
        }

        ColumnLayout {
            Layout.minimumWidth: 130
            Layout.maximumWidth: 260
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: bar.player && bar.player.title !== "" ? bar.player.title
                                                            : qsTr("Nothing playing")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
                font.weight: Theme.weightSemi
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: bar.player ? bar.player.artist : ""
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
                visible: text !== ""
            }
        }

        IconButton {
            icon: bar.player && bar.player.favorite ? "heartFilled" : "heart"
            variant: "ghost"
            customColor: bar.player && bar.player.favorite ? Theme.accent : "transparent"
            tooltip: qsTr("Favourite")
            Layout.alignment: Qt.AlignVCenter
            onClicked: if (bar.player) bar.player.toggleFavorite()
        }

        // -------------------------------------------------------- transport --
        RowLayout {
            spacing: Theme.spacing2
            Layout.alignment: Qt.AlignVCenter

            IconButton {
                icon: "shuffle"
                variant: "ghost"
                active: bar.player ? bar.player.shuffle : false
                tooltip: qsTr("Shuffle")
                onClicked: if (bar.player) bar.player.toggleShuffle()
            }

            IconButton {
                icon: "prev"
                tooltip: qsTr("Previous")
                onClicked: if (bar.player) bar.player.previous()
            }

            IconButton {
                icon: bar.player && bar.player.playing ? "pause" : "play"
                variant: "accent"
                diameter: 52
                iconScale: 0.46
                tooltip: bar.player && bar.player.playing ? qsTr("Pause") : qsTr("Play")
                onClicked: if (bar.player) bar.player.togglePlayPause()
            }

            IconButton {
                icon: "next"
                tooltip: qsTr("Next")
                onClicked: if (bar.player) bar.player.next()
            }

            IconButton {
                icon: bar.player && bar.player.repeatMode === 2 ? "repeatOne" : "repeat"
                variant: "ghost"
                active: bar.player ? bar.player.repeatMode !== 0 : false
                tooltip: qsTr("Repeat")
                onClicked: if (bar.player) bar.player.cycleRepeat()
            }
        }

        // ------------------------------------------------------- scrubbing --
        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.spacing3

            Text {
                text: bar.player ? bar.player.positionText : "0:00"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontCaption
                font.features: { "tnum": 1 }
            }

            WaveformSeek {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                peaks: bar.player ? bar.player.waveform : []
                position: bar.player && bar.player.durationSec > 0
                          ? bar.player.positionSec / bar.player.durationSec : 0
                onSeekRequested: (fraction) => { if (bar.player) bar.player.seekFraction(fraction) }
            }

            Text {
                text: bar.player ? bar.player.durationText : "0:00"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontCaption
                font.features: { "tnum": 1 }
            }
        }

        // ----------------------------------------------------------- extras --
        IconButton {
            icon: "lyrics"
            variant: "ghost"
            active: bar.lyricsOpen
            tooltip: qsTr("Lyrics")
            Layout.alignment: Qt.AlignVCenter
            onClicked: bar.toggleLyrics()
        }

        IconButton {
            icon: "list"
            variant: "ghost"
            active: bar.queueOpen
            tooltip: qsTr("Queue")
            Layout.alignment: Qt.AlignVCenter
            onClicked: bar.toggleQueue()
        }

        VolumeControl {
            Layout.preferredWidth: 132
            Layout.alignment: Qt.AlignVCenter
            player: bar.player
        }
    }
}
