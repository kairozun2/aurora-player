// Aurora Player - frosted glass container.
//
// When `backdrop` is set the card really samples and blurs what is behind it
// (like the floating player bar over the cover art). Without a backdrop it
// degrades gracefully into a tinted, softly shadowed surface.
import QtQuick
import QtQuick.Effects

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

        // Live, blurred snapshot of whatever sits behind the card.
        ShaderEffectSource {
            id: snapshot
            anchors.fill: parent
            visible: root.backdrop !== null
            sourceItem: root.backdrop
            live: true
            recursive: false
            hideSource: false
            sourceRect: {
                if (!root.backdrop)
                    return Qt.rect(0, 0, 0, 0)
                // Referencing the geometry explicitly keeps this binding live
                // while the card moves or resizes.
                var track = root.x + root.y + root.width + root.height
                var p = root.mapToItem(root.backdrop, 0, 0)
                return Qt.rect(p.x, p.y, root.width, root.height)
            }
            layer.enabled: true
            layer.effect: MultiEffect {
                blurEnabled: true
                blur: root.blurAmount
                blurMax: 48
                autoPaddingEnabled: false
            }
        }

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
