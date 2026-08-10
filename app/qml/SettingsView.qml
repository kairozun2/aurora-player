// Aurora Player - the settings page.
//
// Every control writes straight through to the C++ Controller via PlayerBridge,
// so nothing here keeps its own state: flipping a switch persists the setting
// and the whole UI reacts through the bridge's change signals.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    property var player: null

    readonly property var speedValues: [0.75, 1.0, 1.25, 1.5, 2.0]
    readonly property var crossfadeValues: [0, 2, 4, 8]

    function closestIndex(values, value) {
        var best = 0
        var bestDelta = Number.MAX_VALUE
        for (var i = 0; i < values.length; ++i) {
            var delta = Math.abs(values[i] - value)
            if (delta < bestDelta) { bestDelta = delta; best = i }
        }
        return best
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            id: column
            width: Math.min(760, scroll.availableWidth - Theme.spacing5 * 2)
            x: Math.max(Theme.spacing5, (scroll.availableWidth - width) / 2)
            spacing: Theme.spacing6

            Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacing5 }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing1

                Text {
                    text: qsTr("Settings")
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontHeading
                    font.weight: Theme.weightBold
                }

                Text {
                    Layout.fillWidth: true
                    text: root.player ? root.player.statusLine : ""
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideRight
                }
            }

            SettingsGroup {
                Layout.fillWidth: true
                title: qsTr("Appearance")

                SettingsRow {
                    label: qsTr("Language")
                    hint: qsTr("Interface language, applied instantly")

                    SegmentedControl {
                        options: ["Русский", "English"]
                        currentIndex: root.player && root.player.language === "en" ? 1 : 0
                        onSelected: (index) => {
                            if (root.player) root.player.setLanguage(index === 1 ? "en" : "ru")
                        }
                    }
                }

                SettingsRow {
                    label: qsTr("Theme")
                    hint: qsTr("Dark uses colours sampled from the cover art")

                    SegmentedControl {
                        options: [qsTr("Dark"), qsTr("Light")]
                        currentIndex: root.player && !root.player.darkTheme ? 1 : 0
                        onSelected: (index) => {
                            if (root.player) root.player.setTheme(index === 1 ? "light" : "dark")
                        }
                    }
                }
            }

            SettingsGroup {
                Layout.fillWidth: true
                title: qsTr("Playback")

                SettingsRow {
                    label: qsTr("Gapless playback")
                    hint: qsTr("No silence between tracks of an album")

                    AuroraSwitch {
                        checked: root.player ? root.player.gapless : true
                        onToggled: (value) => { if (root.player) root.player.setGapless(value) }
                    }
                }

                SettingsRow {
                    label: qsTr("Crossfade")
                    hint: qsTr("Overlap the end of a track with the next one")

                    SegmentedControl {
                        options: [qsTr("Off"), "2 s", "4 s", "8 s"]
                        currentIndex: root.player
                                      ? root.closestIndex(root.crossfadeValues, root.player.crossfade) : 0
                        onSelected: (index) => {
                            if (root.player) root.player.setCrossfade(root.crossfadeValues[index])
                        }
                    }
                }

                SettingsRow {
                    label: qsTr("Playback speed")
                    hint: qsTr("Pitch is preserved by the resampler")

                    SegmentedControl {
                        options: ["0.75x", "1x", "1.25x", "1.5x", "2x"]
                        currentIndex: root.player
                                      ? root.closestIndex(root.speedValues, root.player.speed) : 1
                        onSelected: (index) => {
                            if (root.player) root.player.setSpeed(root.speedValues[index])
                        }
                    }
                }

                SettingsRow {
                    label: qsTr("Remember position")
                    hint: qsTr("Resume the last track where you stopped")

                    AuroraSwitch {
                        checked: root.player ? root.player.rememberPosition : true
                        onToggled: (value) => { if (root.player) root.player.setRememberPosition(value) }
                    }
                }
            }

            SettingsGroup {
                Layout.fillWidth: true
                title: qsTr("Equalizer")

                SettingsRow {
                    label: qsTr("10-band equalizer")
                    hint: qsTr("Biquad filters applied on the audio thread")

                    AuroraSwitch {
                        checked: root.player ? root.player.eqEnabled : false
                        onToggled: (value) => { if (root.player) root.player.setEqEnabled(value) }
                    }
                }

                // The preset buttons are positioned by hand instead of with a Flow.
                // A positioner measures its children while those children measure
                // themselves against the positioner, so Qt keeps repeating the layout
                // pass and the window never finishes opening. Fixed columns plus
                // arithmetic x/y removes that feedback completely.
                Item {
                    id: presetBox

                    readonly property int columnCount: 4
                    readonly property int gap: Theme.spacing2
                    readonly property int cellHeight: 34
                    readonly property int presetCount: root.player ? root.player.eqPresets.length : 0
                    readonly property int rowCount: Math.ceil(presetCount / columnCount)
                    readonly property real cellWidth: (width - gap * (columnCount - 1)) / columnCount

                    Layout.fillWidth: true
                    Layout.preferredHeight: rowCount > 0 ? rowCount * cellHeight + (rowCount - 1) * gap : 0
                    opacity: root.player && root.player.eqEnabled ? 1.0 : 0.45

                    Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }

                    Repeater {
                        model: root.player ? root.player.eqPresets : []

                        delegate: AuroraButton {
                            required property string modelData
                            required property int index

                            text: modelData
                            primary: root.player && root.player.eqPreset === modelData
                            enabled: root.player && root.player.eqEnabled
                            width: presetBox.cellWidth
                            height: presetBox.cellHeight
                            x: (index % presetBox.columnCount) * (presetBox.cellWidth + presetBox.gap)
                            y: Math.floor(index / presetBox.columnCount) * (presetBox.cellHeight + presetBox.gap)
                            onClicked: if (root.player) root.player.applyPreset(modelData)
                        }
                    }
                }
            }

            SettingsGroup {
                Layout.fillWidth: true
                title: qsTr("Library")

                Text {
                    Layout.fillWidth: true
                    text: root.player ? root.player.libraryStats : ""
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: root.player ? root.player.musicFolders : []

                    delegate: RowLayout {
                        required property string modelData

                        Layout.fillWidth: true
                        spacing: Theme.spacing3

                        IconGlyph {
                            name: "grid"
                            color: Theme.textMuted
                            implicitWidth: 18
                            implicitHeight: 18
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideMiddle
                        }

                        AuroraButton {
                            text: qsTr("Remove")
                            implicitHeight: 32
                            onClicked: if (root.player) root.player.removeFolder(modelData)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing2

                    AuroraButton {
                        text: qsTr("Rescan library")
                        icon: "refresh"
                        onClicked: if (root.player) root.player.scan()
                    }

                    Item { Layout.fillWidth: true }
                }
            }

            SettingsGroup {
                Layout.fillWidth: true
                title: qsTr("External tools")

                Repeater {
                    model: root.player ? root.player.toolStatus : []

                    delegate: RowLayout {
                        required property var modelData

                        Layout.fillWidth: true
                        spacing: Theme.spacing3

                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: modelData.available ? Theme.success : Theme.danger
                        }

                        Text {
                            text: modelData.name
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            font.weight: Theme.weightMedium
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.detail
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontCaption
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            SettingsGroup {
                Layout.fillWidth: true
                title: qsTr("About")

                Text {
                    Layout.fillWidth: true
                    text: root.player ? root.player.aboutText : ""
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontCaption
                    lineHeight: 1.35
                    wrapMode: Text.WordWrap
                }
            }

            Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacing7 }
        }
    }
}
