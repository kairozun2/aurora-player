// Aurora Player - frosted glass container.
//
// Drawn with plain Qt Quick primitives only: a rounded, clipped surface with a
// hairline border and a soft top highlight. No GPU effect module is involved,
// so the card renders on machines with weak or missing 3D drivers, and a
// missing effect type can never stop the whole interface from loading.
import QtQuick

Item {
    id: root

    property real radius: Theme.radiusLg
    property Item backdrop: null
    property real blurAmount: 0.9
    property color tint: Theme.glass
    property color borderColor: Theme.glassBorder
    property bool shadow: true
    property real shadowOpacity: Theme.dark ? 0.55 : 0.18

    default property alias content: contentHost.data

    // Soft drop shadow, faked with a slightly larger rounded outline: costs
    // nothing on the GPU and behaves identically on every driver.
    Rectangle {
        visible: root.shadow
        anchors.fill: parent
        anchors.margins: -2
        radius: root.radius + 2
        color: "transparent"
        border.width: 2
        border.color: Qt.rgba(0, 0, 0, root.shadowOpacity * 0.22)
    }

    // The card surface. "clip" gives the rounded silhouette that the mask
    // effect used to provide.
    Rectangle {
        id: surface
        anchors.fill: parent
        radius: root.radius
        color: root.tint
        clip: true

        // Top highlight: the detail that sells the glass look.
        Rectangle {
            width: parent.width
            height: 1
            color: Theme.dark ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(1, 1, 1, 0.8)
        }

        Item {
            id: contentHost
            anchors.fill: parent
        }
    }

    // Hairline border, drawn on top of the content.
    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: 1
        border.color: root.borderColor
    }
}
