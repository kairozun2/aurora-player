// Aurora Player - one row of the track list.
import QtQuick
import QtQuick.Layouts

Item {
    id: row

    property var player: null

    required property int index
    required property string title
    required property string artist
    required property string album
    required property string durationText
    required property string coverUrl
    required property bool favorite
    required property string path

    readonly property bool isCurrent: player && player.currentPath === path

    implicitHeight: 58

    Rectangle {
        anchors.fill: parent
        anchors.rightMargin: Theme.spacing2
        radius: Theme.radiusMd
        color: row.isCurrent ? Theme.alpha(Theme.accent, Theme.dark ? 0.18 : 0.12)
                             : (hover.hovered ? Theme.surfaceHover : "transparent")

        Behavior on color { ColorAnimation { duration: Theme.durationFast } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.spacing3
            anchors.rightMargin: Theme.spacing3
            spacing: Theme.spacing3

            // Cover with a play overlay on hover.
            Rectangle {
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                radius: Theme.radiusSm
                color: Theme.surfaceRaised
                clip: true

                Image {
                    anchors.fill: parent
                    source: row.coverUrl
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                }

                Rectangle {
                    anchors.fill: parent
                    color: Qt.rgba(0, 0, 0, 0.45)
                    visible: hover.hovered || row.isCurrent

                    IconGlyph {
                        anchors.centerIn: parent
                        width: 18; height: 18
                        name: row.isCurrent && row.player && row.player.playing ? "pause" : "play"
                        color: "#FFFFFF"
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Text {
                    Layout.fillWidth: true
                    text: row.title
                    color: row.isCurrent ? Theme.accent : Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    font.weight: row.isCurrent ? Theme.weightSemi : Theme.weightMedium
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: row.artist
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideRight
                }
            }

            Text {
                Layout.preferredWidth: 180
                visible: row.width > 620
                text: row.album
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
            }

            IconButton {
                icon: row.favorite ? "heartFilled" : "heart"
                variant: "ghost"
                diameter: 32
                opacity: row.favorite || hover.hovered ? 1 : 0
                customColor: row.favorite ? Theme.accent : "transparent"
                onClicked: if (row.player) row.player.toggleFavoriteAt(row.index)

                Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }
            }

            Text {
                text: row.durationText
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontCaption
            }

            IconButton {
                icon: "plus"
                variant: "ghost"
                diameter: 32
                opacity: hover.hovered ? 1 : 0
                tooltip: qsTr("Play next")
                onClicked: if (row.player) row.player.queueNext(row.index)

                Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }
            }
        }

        HoverHandler { id: hover }

        TapHandler {
            onDoubleTapped: if (row.player) row.player.playTrackAt(row.index)
            onSingleTapped: if (row.isCurrent && row.player) row.player.togglePlayPause()
        }
    }
}
