// Aurora Player - play queue side panel with drag-to-reorder.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

GlassCard {
    id: panel

    property var player: null
    property var queue: null               // QueueModel

    radius: Theme.radiusLg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing4
        spacing: Theme.spacing3

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("Queue")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontTitle
                font.weight: Theme.weightBold
            }

            Item { Layout.fillWidth: true }

            IconButton {
                glyph: "shuffle"
                variant: "ghost"
                diameter: 32
                active: panel.player ? panel.player.shuffle : false
                tooltip: qsTr("Shuffle")
                onClicked: if (panel.player) panel.player.toggleShuffle()
            }

            IconButton {
                glyph: "trash"
                variant: "ghost"
                diameter: 32
                tooltip: qsTr("Clear queue")
                onClicked: if (panel.player) panel.player.clearQueue()
            }
        }

        ListView {
            id: view
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            model: panel.queue
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Item {
                id: item

                required property int index
                required property string title
                required property string artist
                required property string durationText
                required property string coverUrl
                required property bool isCurrent

                width: view.width
                height: 52

                Rectangle {
                    anchors.fill: parent
                    anchors.rightMargin: Theme.spacing1
                    radius: Theme.radiusSm
                    color: item.isCurrent ? Theme.alpha(Theme.accent, 0.16)
                                          : (rowHover.hovered ? Theme.alpha(Theme.text, 0.06) : "transparent")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacing2
                        anchors.rightMargin: Theme.spacing2
                        spacing: Theme.spacing3

                        // Position, or an equaliser hint for the current track.
                        Item {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20

                            Text {
                                anchors.centerIn: parent
                                visible: !item.isCurrent
                                text: item.index + 1
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontCaption
                            }

                            Row {
                                anchors.centerIn: parent
                                visible: item.isCurrent
                                spacing: 2

                                Repeater {
                                    model: 3
                                    delegate: Rectangle {
                                        required property int index
                                        width: 2.5
                                        height: 12
                                        radius: 1.25
                                        color: Theme.accent
                                        anchors.verticalCenter: parent.verticalCenter

                                        SequentialAnimation on height {
                                            running: panel.player ? panel.player.playing : false
                                            loops: Animation.Infinite
                                            NumberAnimation { to: 4 + index * 3; duration: 260 + index * 90 }
                                            NumberAnimation { to: 14 - index * 2; duration: 300 + index * 70 }
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            radius: Theme.radiusSm
                            color: Theme.surfaceRaised
                            clip: true

                            Image {
                                anchors.fill: parent
                                source: item.coverUrl
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Text {
                                Layout.fillWidth: true
                                text: item.title
                                color: item.isCurrent ? Theme.accent : Theme.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontCaption
                                font.weight: Theme.weightMedium
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: item.artist
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                        }

                        Text {
                            text: item.durationText
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: 12
                        }

                        IconButton {
                            glyph: "close"
                            variant: "ghost"
                            diameter: 26
                            opacity: rowHover.hovered ? 1 : 0
                            onClicked: if (panel.player) panel.player.removeFromQueue(item.index)

                            Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }
                        }
                    }

                    HoverHandler { id: rowHover }

                    TapHandler {
                        onDoubleTapped: if (panel.player) panel.player.playQueueIndex(item.index)
                    }

                    // Drag to reorder.
                    DragHandler {
                        id: drag
                        yAxis.enabled: true
                        xAxis.enabled: false
                        onActiveChanged: {
                            if (!active && panel.player) {
                                var target = Math.round(item.index + item.y / item.height)
                                panel.player.moveInQueue(item.index, Math.max(0, target))
                                item.y = 0
                            }
                        }
                    }
                    z: drag.active ? 10 : 0
                }
            }

            Text {
                anchors.centerIn: parent
                visible: view.count === 0
                text: qsTr("The queue is empty")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontCaption
            }
        }
    }
}
