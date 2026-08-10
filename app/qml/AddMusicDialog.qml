// Aurora Player - "add music" hub: files, folders, direct links, YouTube.
//
// Every import path the app supports lives here, including the live download
// queue driven by yt-dlp / ffmpeg on the C++ side.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: root

    property var player: null
    property var downloads: null            // DownloadsModel
    property bool open: false
    property int mode: 0                    // 0 files, 1 folder, 2 link

    anchors.fill: parent
    visible: opacity > 0
    opacity: open ? 1 : 0

    Behavior on opacity {
        NumberAnimation { duration: Theme.durationBase }
    }

    // Scrim.
    Rectangle {
        anchors.fill: parent
        color: Theme.scrim

        TapHandler { onSingleTapped: root.open = false }
    }

    GlassCard {
        id: card
        width: Math.min(560, root.width - Theme.spacing6)
        height: implicitContentHeight
        anchors.centerIn: parent
        radius: Theme.radiusXl
        tint: Theme.dark ? Qt.rgba(0.10, 0.09, 0.08, 0.94) : Qt.rgba(1, 1, 1, 0.97)

        readonly property int implicitContentHeight: layout.implicitHeight + Theme.spacing5 * 2

        scale: root.open ? 1 : 0.96
        Behavior on scale {
            NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easeStandard }
        }

        ColumnLayout {
            id: layout
            anchors.fill: parent
            anchors.margins: Theme.spacing5
            spacing: Theme.spacing4

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: qsTr("Add music")
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontTitle
                    font.weight: Theme.weightBold
                }

                Item { Layout.fillWidth: true }

                IconButton {
                    glyph: "close"
                    variant: "ghost"
                    diameter: 32
                    onClicked: root.open = false
                }
            }

            // ------------------------------------------------------- sources --
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing2

                Repeater {
                    model: [
                        { icon: "plus", label: qsTr("Files") },
                        { icon: "folder", label: qsTr("Folder") },
                        { icon: "link", label: qsTr("Link / YouTube") }
                    ]

                    delegate: AbstractButton {
                        required property int index
                        required property var modelData

                        Layout.fillWidth: true
                        implicitHeight: 76
                        hoverEnabled: true

                        background: Rectangle {
                            radius: Theme.radiusMd
                            color: root.mode === index ? Theme.alpha(Theme.accent, Theme.dark ? 0.20 : 0.12)
                                                       : (parent.hovered ? Theme.surfaceHover : Theme.surfaceRaised)
                            border.width: root.mode === index ? 1 : 0
                            border.color: Theme.accent

                            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                        }

                        contentItem: ColumnLayout {
                            spacing: Theme.spacing1

                            IconGlyph {
                                Layout.alignment: Qt.AlignHCenter
                                width: 22; height: 22
                                name: modelData.icon
                                color: root.mode === index ? Theme.accent : Theme.textSecondary
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: modelData.label
                                color: root.mode === index ? Theme.text : Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontCaption
                                font.weight: Theme.weightMedium
                            }
                        }

                        onClicked: {
                            root.mode = index
                            if (index === 0) fileDialog.open()
                            else if (index === 1) folderDialog.open()
                            else urlField.forceActiveFocus()
                        }
                    }
                }
            }

            // ----------------------------------------------------- link input --
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing2

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.controlHeight
                    radius: Theme.radiusSm
                    color: Theme.surfaceRaised
                    border.width: urlField.activeFocus ? 1 : 0
                    border.color: Theme.accent

                    TextField {
                        id: urlField
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacing3
                        anchors.rightMargin: Theme.spacing3
                        placeholderText: qsTr("Paste a YouTube link, a stream or a direct file URL")
                        placeholderTextColor: Theme.textMuted
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        background: null
                        verticalAlignment: Text.AlignVCenter
                        onAccepted: root.submitUrl()
                    }
                }

                AuroraButton {
                    text: qsTr("Add")
                    primary: true
                    enabled: urlField.text.trim() !== ""
                    onClicked: root.submitUrl()
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("YouTube and other sites need yt-dlp; streams and direct files play instantly without downloading.")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.border
                visible: downloadList.count > 0
            }

            // ----------------------------------------------------- downloads --
            ListView {
                id: downloadList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(count * 58, 174)
                visible: count > 0
                clip: true
                spacing: Theme.spacing1
                model: root.downloads

                delegate: Item {
                    required property int index
                    required property string jobId
                    required property string title
                    required property string stateText
                    required property real progress
                    required property string detail
                    required property bool finished
                    required property bool failed

                    width: downloadList.width
                    height: 54

                    RowLayout {
                        anchors.fill: parent
                        spacing: Theme.spacing3

                        IconGlyph {
                            width: 20; height: 20
                            name: failed ? "close" : (finished ? "check" : "download")
                            color: failed ? Theme.danger : (finished ? Theme.success : Theme.accent)
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Text {
                                Layout.fillWidth: true
                                text: title
                                color: Theme.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontCaption
                                font.weight: Theme.weightMedium
                                elide: Text.ElideRight
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 4
                                radius: 2
                                color: Theme.dark ? Qt.rgba(1, 1, 1, 0.14) : "#E6E5E3"
                                visible: !finished && !failed

                                Rectangle {
                                    width: parent.width * Math.max(0, Math.min(1, progress))
                                    height: parent.height
                                    radius: parent.radius
                                    color: Theme.accent

                                    Behavior on width { NumberAnimation { duration: Theme.durationFast } }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: stateText + (detail !== "" ? "  ·  " + detail : "")
                                color: failed ? Theme.danger : Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }

                        IconButton {
                            glyph: "close"
                            variant: "ghost"
                            diameter: 28
                            visible: !finished && !failed
                            onClicked: if (root.player) root.player.cancelDownload(jobId)
                        }
                    }
                }
            }
        }
    }

    function submitUrl() {
        var text = urlField.text.trim()
        if (text === "" || !player) return
        player.addUrl(text)
        urlField.text = ""
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Choose audio files")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Audio files") + " (*.mp3 *.flac *.wav *.ogg *.opus *.m4a *.aac *.wma *.alac *.aiff)",
                      qsTr("All files") + " (*)"]
        onAccepted: {
            for (var i = 0; i < selectedFiles.length; ++i)
                if (root.player) root.player.addPath(selectedFiles[i])
            root.open = false
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Choose a music folder")
        onAccepted: {
            if (root.player) root.player.addFolder(selectedFolder)
            root.open = false
        }
    }

    Keys.onEscapePressed: root.open = false
}
