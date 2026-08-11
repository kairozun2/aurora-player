// Aurora Player - hero screen: cover carousel.
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
                    text: root.player ? root.player.artist + (root.player.album !== "" ? "  \u00b7  " + root.player.album : "") : ""
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
    }
}
