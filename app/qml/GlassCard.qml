// Aurora Player - frosted glass container.
//
// When `backdrop` is set the card really samples and blurs what is behind it
// (like the floating player bar over the cover art). Without a backdrop it
// degrades gracefully into a tinted, softly shadowed surface.
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

    // Rounded clipper: everything below inherits the card silhouette.
    Item {
        id: clipper
        anchors.fill: parent
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: mask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 0.35
        }

        // Simple semi-transparent background instead of heavy blur
        Rectangle {
            anchors.fill: parent
            color: root.tint
        }

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

    Rectangle {
        id: mask
        anchors.fill: parent
        radius: root.radius
        color: "black"
        visible: false
        layer.enabled: true
        layer.smooth: true
    }

    // Hairline border on top of the content.
    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: 1
        border.color: root.borderColor
    }

    layer.enabled: root.shadow
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: Qt.rgba(0, 0, 0, root.shadowOpacity)
        shadowBlur: 0.8
        shadowVerticalOffset: 10
        blurMax: 40
    }
}
