// Aurora Player - hero screen: cover carousel + vinyl now playing card.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    property var player: null
    property var albums: null
    property var tracks: null

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacing3
        anchors.rightMargin: Theme.spacing5
        anchors.topMargin: Theme.spacing5
        anchors.bottomMargin: Theme.spacing3
        spacing: Theme.spacing4

        // ------------------------------------------------------------ header --
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing3

            ColumnLayout {
                spacing: 2

                Text {
                    text: root.player && root.player.playing ? qsTr("Listening to") : qsTr("Ready to play")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontCaption
                    font.weight: Theme.weightMedium
                }

                Text {
                    text: root.player && root.player.title !== "" ? root.player.title
                                                                  : qsTr("Your music, always with you")
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontDisplay
                    font.weight: Theme.weightBold
                    elide: Text.ElideRight
                    Layout.maximumWidth: root.width - 260
                }

                Text {
                    visible: root.player && root.player.artist !== ""
                    text: root.player ? root.player.artist + (root.player.album !== "" ? "  ·  " + root.player.album : "") : ""
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideRight
                    Layout.maximumWidth: root.width - 260
                }
            }

            Item { Layout.fillWidth: true }

            AuroraButton {
                text: qsTr("Refresh")
                icon: "refresh"
                onClicked: if (root.player) root.player.scan()
            }

            AuroraButton {
                text: qsTr("Shuffle all")
                icon: "shuffle"
                primary: true
                onClicked: if (root.player) root.player.shuffleAll()
            }
        }

        // ---------------------------------------------------------- carousel --
        CoverFlow {
            id: flow
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 240
            model: root.albums
            onActivated: (index) => { if (root.player) root.player.playAlbum(index) }
        }

        // ------------------------------------------------------------- vinyl --
        GlassCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 168
            radius: Theme.radiusXl
            visible: root.player && root.player.title !== ""

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacing4
                spacing: Theme.spacing5

                NowPlayingVinyl {
                    Layout.preferredWidth: 240
                    Layout.fillHeight: true
                    player: root.player
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.spacing2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing2

                        Text {
                            Layout.fillWidth: true
                            text: root.player ? root.player.title : ""
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontTitle
                            font.weight: Theme.weightSemi
                            elide: Text.ElideRight
                        }

                        AuroraButton {
                            text: root.player && root.player.isStream ? qsTr("Open source") : qsTr("Show file")
                            icon: "external"
                            iconTrailing: true
                            onClicked: if (root.player) root.player.revealCurrent()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.player ? root.player.artist : ""
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                    }

                    Item { Layout.fillHeight: true }

                    WaveformSeek {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        peaks: root.player ? root.player.waveform : []
                        position: root.player && root.player.durationSec > 0
                                  ? root.player.positionSec / root.player.durationSec : 0
                        onSeekRequested: (fraction) => { if (root.player) root.player.seekFraction(fraction) }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: root.player ? root.player.positionText : "0:00"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontCaption
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: root.player ? root.player.durationText : "0:00"
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontCaption
                        }
                    }
                }
            }
        }
    }
}
