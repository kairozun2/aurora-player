// Aurora Player - 3D cover carousel.
//
// A PathView whose delegates are rotated around the Y axis, so the covers next
// to the centre one lean back into the scene. Drag, scroll, arrow keys or a
// click all move the selection; the centre cover is what plays.
import QtQuick

Item {
    id: root

    property alias model: view.model
    property alias currentIndex: view.currentIndex
    property int coverSize: Math.round(Math.min(width * 0.34, height * 0.78, 340))
    property bool showTitleOverlay: true

    signal activated(int index)

    implicitHeight: coverSize * 1.35

    PathView {
        id: view
        anchors.fill: parent
        pathItemCount: 7
        preferredHighlightBegin: 0.5
        preferredHighlightEnd: 0.5
        highlightRangeMode: PathView.StrictlyEnforceRange
        snapMode: PathView.SnapToItem
        dragMargin: root.width / 2
        flickDeceleration: 2400
        maximumFlickVelocity: 2600
        clip: false
        focus: true

        Keys.onLeftPressed: decrementCurrentIndex()
        Keys.onRightPressed: incrementCurrentIndex()

        path: Path {
            startX: 0
            startY: root.height / 2

            PathAttribute { name: "itemScale"; value: 0.62 }
            PathAttribute { name: "itemAngle"; value: 62 }
            PathAttribute { name: "itemZ"; value: 0 }
            PathAttribute { name: "itemOpacity"; value: 0.35 }

            PathLine { x: root.width / 2; y: root.height / 2 }
            PathAttribute { name: "itemScale"; value: 1.0 }
            PathAttribute { name: "itemAngle"; value: 0 }
            PathAttribute { name: "itemZ"; value: 40 }
            PathAttribute { name: "itemOpacity"; value: 1.0 }

            PathLine { x: root.width; y: root.height / 2 }
            PathAttribute { name: "itemScale"; value: 0.62 }
            PathAttribute { name: "itemAngle"; value: -62 }
            PathAttribute { name: "itemZ"; value: 0 }
            PathAttribute { name: "itemOpacity"; value: 0.35 }
        }

        delegate: Item {
            id: card

            required property int index
            required property string title
            required property string artist
            required property string coverUrl

            readonly property real itemScale: PathView.itemScale === undefined ? 1 : PathView.itemScale
            readonly property real itemAngle: PathView.itemAngle === undefined ? 0 : PathView.itemAngle
            readonly property real itemOpacity: PathView.itemOpacity === undefined ? 1 : PathView.itemOpacity
            readonly property bool isCurrent: PathView.isCurrentItem

            width: root.coverSize
            height: root.coverSize
            z: PathView.itemZ === undefined ? 0 : PathView.itemZ
            scale: itemScale
            opacity: itemOpacity

            transform: Rotation {
                origin.x: card.width / 2
                origin.y: card.height / 2
                axis { x: 0; y: 1; z: 0 }
                angle: card.itemAngle
            }

            Rectangle {
                id: art
                anchors.fill: parent
                radius: Theme.radiusLg
                color: Theme.surfaceRaised
                clip: true

                Image {
                    anchors.fill: parent
                    source: card.coverUrl
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: true
                }

                // Legibility gradient for the overlay text.
                Rectangle {
                    anchors.fill: parent
                    visible: card.isCurrent && root.showTitleOverlay
                    gradient: Gradient {
                        GradientStop { position: 0.55; color: "transparent" }
                        GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.72) }
                    }
                }

                Column {
                    visible: card.isCurrent && root.showTitleOverlay
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: Theme.spacing4
                    spacing: 2

                    Text {
                        width: parent.width
                        text: card.title
                        color: "#FFFFFF"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBodyLarge
                        font.weight: Theme.weightSemi
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: card.artist
                        color: Qt.rgba(1, 1, 1, 0.75)
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideRight
                    }
                }
            }

            // Simple border instead of heavy shadow
            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusLg
                color: "transparent"
                border.width: card.isCurrent ? 2 : 0
                border.color: Theme.accent
                visible: card.isCurrent
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (card.isCurrent) root.activated(card.index)
                    else view.currentIndex = card.index
                }
                onDoubleClicked: root.activated(card.index)
            }

            Behavior on scale {
                NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easeStandard }
            }
        }

        WheelHandler {
            property real acc: 0
            onWheel: (event) => {
                acc += event.angleDelta.y
                if (acc > 90) { view.decrementCurrentIndex(); acc = 0 }
                else if (acc < -90) { view.incrementCurrentIndex(); acc = 0 }
            }
        }
    }
}
