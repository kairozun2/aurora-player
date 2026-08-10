// Aurora Player - lyrics panel with karaoke style highlighting.
//
// Synced .lrc files scroll and highlight the active line; plain text files are
// shown as-is. Lyrics are read by the C++ side (embedded tag, sidecar file).
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

GlassCard {
    id: panel

    property var player: null

    radius: Theme.radiusLg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing4
        spacing: Theme.spacing3

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("Lyrics")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontTitle
                font.weight: Theme.weightBold
            }

            Item { Layout.fillWidth: true }

            Text {
                visible: panel.player ? panel.player.lyricsSynced : false
                text: qsTr("synced")
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.weight: Theme.weightMedium
            }
        }

        ListView {
            id: view
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.spacing2
            model: panel.player ? panel.player.lyrics : []
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            readonly property int activeLine: panel.player ? panel.player.currentLyricLine : -1

            onActiveLineChanged: {
                if (activeLine >= 0 && panel.player && panel.player.lyricsSynced)
                    positionViewAtIndex(activeLine, ListView.Center)
            }

            delegate: Text {
                required property int index
                required property string modelData

                width: view.width
                text: modelData
                wrapMode: Text.WordWrap
                horizontalAlignment: panel.player && panel.player.lyricsSynced
                                     ? Text.AlignHCenter : Text.AlignLeft
                font.family: Theme.fontFamily
                font.pixelSize: index === view.activeLine ? Theme.fontBodyLarge : Theme.fontBody
                font.weight: index === view.activeLine ? Theme.weightSemi : Theme.weightRegular
                color: index === view.activeLine ? Theme.text
                                                 : (view.activeLine >= 0 ? Theme.textMuted : Theme.textSecondary)

                Behavior on color { ColorAnimation { duration: Theme.durationBase } }
                Behavior on font.pixelSize { NumberAnimation { duration: Theme.durationBase } }

                TapHandler {
                    enabled: panel.player ? panel.player.lyricsSynced : false
                    onSingleTapped: if (panel.player) panel.player.seekToLyricLine(index)
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: Theme.spacing2
                visible: view.count === 0

                IconGlyph {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 34; height: 34
                    name: "lyrics"
                    color: Theme.textMuted
                }

                Text {
                    text: qsTr("No lyrics for this track")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontCaption
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Drop a .lrc file next to the audio file")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                }
            }
        }
    }
}
