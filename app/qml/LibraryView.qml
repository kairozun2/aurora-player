// Aurora Player - library browser: search, tracks, albums, artists, favourites.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    property var player: null
    property var tracks: null              // LibraryModel
    property var albums: null              // AlbumModel
    property int tab: 0                    // 0 tracks, 1 albums, 2 favourites

    signal requestAddMusic()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing5
        spacing: Theme.spacing4

        // ------------------------------------------------------------ header --
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing3

            Text {
                text: qsTr("Library")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontHeading
                font.weight: Theme.weightBold
            }

            Item { Layout.fillWidth: true }

            // Search box.
            Rectangle {
                Layout.preferredWidth: 260
                Layout.preferredHeight: Theme.controlHeight
                radius: Theme.radiusPill
                color: Theme.surfaceRaised
                border.width: search.activeFocus ? 1 : 0
                border.color: Theme.accent

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacing3
                    anchors.rightMargin: Theme.spacing3
                    spacing: Theme.spacing2

                    IconGlyph {
                        width: 18; height: 18
                        name: "search"
                        color: Theme.textSecondary
                    }

                    TextField {
                        id: search
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search tracks, albums, artists")
                        placeholderTextColor: Theme.textMuted
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        background: null
                        onTextChanged: if (root.tracks) root.tracks.setFilter(text)
                    }

                    IconButton {
                        icon: "close"
                        variant: "ghost"
                        diameter: 26
                        visible: search.text !== ""
                        onClicked: search.text = ""
                    }
                }
            }

            IconButton {
                icon: "plus"
                variant: "accent"
                tooltip: qsTr("Add music")
                onClicked: root.requestAddMusic()
            }

            IconButton {
                icon: "refresh"
                tooltip: qsTr("Rescan folders")
                onClicked: if (root.player) root.player.scan()
            }
        }

        // -------------------------------------------------------------- tabs --
        RowLayout {
            spacing: Theme.spacing2

            Repeater {
                model: [qsTr("Tracks"), qsTr("Albums"), qsTr("Favourites")]
                delegate: AbstractButton {
                    required property int index
                    required property string modelData
                    implicitHeight: 34
                    implicitWidth: label.implicitWidth + Theme.spacing5
                    hoverEnabled: true

                    background: Rectangle {
                        radius: Theme.radiusPill
                        color: root.tab === index ? Theme.accent
                                                  : (parent.hovered ? Theme.surfaceHover : Theme.surfaceRaised)
                        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                    }

                    contentItem: Text {
                        id: label
                        text: modelData
                        color: root.tab === index ? Theme.accentText : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontCaption
                        font.weight: Theme.weightMedium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        root.tab = index
                        if (root.tracks) root.tracks.setFavouritesOnly(index === 2)
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: root.tracks ? qsTr("%1 tracks").arg(root.tracks.count) : ""
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontCaption
            }
        }

        // ----------------------------------------------------------- content --
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.tab === 1 ? 1 : 0

            // Track list.
            ListView {
                id: list
                clip: true
                model: root.tracks
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: TrackRow {
                    width: list.width
                    player: root.player
                }

                // Empty state.
                Column {
                    anchors.centerIn: parent
                    spacing: Theme.spacing3
                    visible: list.count === 0

                    IconGlyph {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 44; height: 44
                        name: "library"
                        color: Theme.textMuted
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Nothing here yet")
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        font.weight: Theme.weightSemi
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Add a folder, a file or a link to get started")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontCaption
                    }
                }
            }

            // Album grid.
            GridView {
                id: grid
                clip: true
                model: root.albums
                cellWidth: 188
                cellHeight: 226
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    required property int index
                    required property string name
                    required property string artist
                    required property string coverUrl
                    required property int trackCount

                    width: grid.cellWidth - Theme.spacing3
                    height: grid.cellHeight - Theme.spacing3

                    Column {
                        anchors.fill: parent
                        spacing: Theme.spacing2

                        Rectangle {
                            width: parent.width
                            height: width
                            radius: Theme.radiusMd
                            color: Theme.surfaceRaised
                            clip: true

                            Image {
                                anchors.fill: parent
                                source: coverUrl
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                            }

                            IconButton {
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: Theme.spacing2
                                icon: "play"
                                variant: "accent"
                                diameter: 38
                                opacity: albumHover.hovered ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }
                                onClicked: if (root.player) root.player.playAlbum(index)
                            }
                        }

                        Text {
                            width: parent.width
                            text: name
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            font.weight: Theme.weightSemi
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: artist + "  ·  " + qsTr("%1 tracks").arg(trackCount)
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideRight
                        }
                    }

                    HoverHandler { id: albumHover }

                    TapHandler {
                        onDoubleTapped: if (root.player) root.player.playAlbum(index)
                    }
                }
            }
        }
    }
}
