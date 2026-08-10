// Aurora Player - 10 band equaliser with presets and preamp.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

GlassCard {
    id: panel

    property var player: null
    readonly property var frequencies: ["32", "64", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"]

    radius: Theme.radiusLg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing4
        spacing: Theme.spacing4

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing3

            Text {
                text: qsTr("Equalizer")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontTitle
                font.weight: Theme.weightBold
            }

            Item { Layout.fillWidth: true }

            Switch {
                id: enableSwitch
                checked: panel.player ? panel.player.eqEnabled : false
                onToggled: if (panel.player) panel.player.setEqEnabled(checked)

                indicator: Rectangle {
                    implicitWidth: 46
                    implicitHeight: 26
                    x: enableSwitch.width - width
                    y: (enableSwitch.height - height) / 2
                    radius: height / 2
                    color: enableSwitch.checked ? Theme.accent
                                                : (Theme.dark ? Qt.rgba(1, 1, 1, 0.18) : "#D6D4D1")

                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }

                    Rectangle {
                        x: enableSwitch.checked ? parent.width - width - 3 : 3
                        y: 3
                        width: 20
                        height: 20
                        radius: 10
                        color: "#FFFFFF"

                        Behavior on x {
                            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easeStandard }
                        }
                    }
                }

                contentItem: Text {
                    text: qsTr("On")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontCaption
                    verticalAlignment: Text.AlignVCenter
                    rightPadding: enableSwitch.indicator.width + Theme.spacing2
                }
            }
        }

        // ----------------------------------------------------------- presets --
        // Positioned by hand for the same reason as the settings page: a Flow
        // measures its children while the children measure themselves against the
        // Flow, which makes Qt repeat the layout pass without ever settling.
        Item {
            id: presetBox

            readonly property int columnCount: 5
            readonly property int gap: Theme.spacing2
            readonly property int cellHeight: 30
            readonly property int presetCount: panel.player ? panel.player.eqPresets.length : 0
            readonly property int rowCount: Math.ceil(presetCount / columnCount)
            readonly property real cellWidth: (width - gap * (columnCount - 1)) / columnCount

            Layout.fillWidth: true
            Layout.preferredHeight: rowCount > 0 ? rowCount * cellHeight + (rowCount - 1) * gap : 0

            Repeater {
                model: panel.player ? panel.player.eqPresets : []

                delegate: AbstractButton {
                    required property string modelData
                    required property int index

                    width: presetBox.cellWidth
                    height: presetBox.cellHeight
                    x: (index % presetBox.columnCount) * (presetBox.cellWidth + presetBox.gap)
                    y: Math.floor(index / presetBox.columnCount) * (presetBox.cellHeight + presetBox.gap)
                    hoverEnabled: true
                    enabled: panel.player ? panel.player.eqEnabled : false
                    opacity: enabled ? 1 : 0.45

                    readonly property bool selected: panel.player && panel.player.eqPreset === modelData

                    background: Rectangle {
                        radius: Theme.radiusPill
                        color: parent.selected ? Theme.accent
                                               : (parent.hovered ? Theme.surfaceHover : Theme.surfaceRaised)
                        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                    }

                    contentItem: Text {
                        text: modelData
                        color: parent.selected ? Theme.accentText : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        font.weight: Theme.weightMedium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    onClicked: if (panel.player) panel.player.applyPreset(modelData)
                }
            }
        }

        // ------------------------------------------------------------- bands --
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing1
            enabled: panel.player ? panel.player.eqEnabled : false
            opacity: enabled ? 1 : 0.45

            Repeater {
                model: 10

                delegate: ColumnLayout {
                    required property int index
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.spacing1

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: {
                            var g = panel.player ? panel.player.bandGain(index) : 0
                            return (g > 0 ? "+" : "") + g.toFixed(0)
                        }
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                    }

                    Slider {
                        id: band
                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillHeight: true
                        orientation: Qt.Vertical
                        from: -12
                        to: 12
                        value: panel.player ? panel.player.bandGain(index) : 0
                        onMoved: if (panel.player) panel.player.setBand(index, value)

                        background: Rectangle {
                            x: (band.width - width) / 2
                            width: 4
                            height: band.availableHeight
                            y: band.topPadding
                            radius: 2
                            color: Theme.dark ? Qt.rgba(1, 1, 1, 0.16) : "#E6E5E3"

                            // Fill from the centre, like a real mixing desk.
                            Rectangle {
                                width: parent.width
                                radius: parent.radius
                                color: Theme.accent
                                y: band.value >= 0 ? parent.height / 2 - height : parent.height / 2
                                height: Math.abs(band.value) / 12 * (parent.height / 2)
                            }
                        }

                        handle: Rectangle {
                            x: (band.width - width) / 2
                            y: band.topPadding + band.visualPosition * (band.availableHeight - height)
                            width: 16
                            height: 16
                            radius: 8
                            color: "#FFFFFF"
                            border.width: 1
                            border.color: Theme.dark ? Qt.rgba(0, 0, 0, 0.3) : Theme.borderStrong
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: panel.frequencies[index]
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                    }
                }
            }
        }

        // ------------------------------------------------------------ preamp --
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing3
            enabled: panel.player ? panel.player.eqEnabled : false
            opacity: enabled ? 1 : 0.45

            Text {
                text: qsTr("Preamp")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontCaption
            }

            Slider {
                id: preamp
                Layout.fillWidth: true
                from: -12
                to: 12
                value: panel.player ? panel.player.preampDb : 0
                onMoved: if (panel.player) panel.player.setPreamp(value)

                background: Rectangle {
                    x: preamp.leftPadding
                    y: preamp.topPadding + preamp.availableHeight / 2 - 2
                    width: preamp.availableWidth
                    height: 4
                    radius: 2
                    color: Theme.dark ? Qt.rgba(1, 1, 1, 0.16) : "#E6E5E3"

                    Rectangle {
                        x: preamp.value >= 0 ? parent.width / 2 : parent.width / 2 - width
                        width: Math.abs(preamp.value) / 12 * (parent.width / 2)
                        height: parent.height
                        radius: parent.radius
                        color: Theme.accent
                    }
                }

                handle: Rectangle {
                    x: preamp.leftPadding + preamp.visualPosition * (preamp.availableWidth - width)
                    y: preamp.topPadding + preamp.availableHeight / 2 - height / 2
                    width: 14
                    height: 14
                    radius: 7
                    color: "#FFFFFF"
                    border.width: 1
                    border.color: Theme.dark ? Qt.rgba(0, 0, 0, 0.3) : Theme.borderStrong
                }
            }

            Text {
                text: (panel.player && panel.player.preampDb > 0 ? "+" : "")
                      + (panel.player ? panel.player.preampDb.toFixed(1) : "0.0") + " dB"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontCaption
            }
        }
    }
}
