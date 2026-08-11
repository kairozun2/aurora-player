// Aurora Player - application shell.
//
// Layout: cover-art backdrop, slim navigation rail, content stack, optional
// right hand panel (queue / lyrics) and the floating glass player bar.
// `player`, `libraryModel`, `albumModel`, `queueModel` and `downloadsModel` are
// injected from C++ (see app/src/main.cpp).
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects

ApplicationWindow {
    id: window

    width: 1280
    height: 820
    minimumWidth: 940
    minimumHeight: 620
    visible: true
    title: player.title !== "" ? player.title + "  ·  " + player.artist : "Aurora Player"
    color: Theme.canvas

    property int page: 0                   // 0 now playing, 1 library, 2 eq, 3 settings
    property int sidePanel: 0              // 0 none, 1 queue, 2 lyrics

    // ---------------------------------------------------------- backdrop ----
    Item {
        id: backdrop
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            color: Theme.dark ? Theme.canvas : Theme.canvas
        }

        // Blurred cover art: the reason the dark theme feels alive.
        Image {
            id: backdropImage
            anchors.fill: parent
            source: player.coverUrl
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            visible: false
        }

        MultiEffect {
            anchors.fill: parent
            source: backdropImage
            blurEnabled: true
            blur: 1.0
            blurMax: 64
            saturation: Theme.dark ? 0.35 : 0.1
            brightness: Theme.dark ? -0.35 : 0.35
            opacity: player.coverUrl !== "" ? (Theme.dark ? 0.85 : 0.35) : 0

            Behavior on opacity { NumberAnimation { duration: Theme.durationSlow } }
        }

        // Vertical scrim so text stays readable over any artwork.
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: Theme.dark ? Qt.rgba(0.06, 0.05, 0.04, 0.55) : Qt.rgba(1, 1, 1, 0.72)
                }
                GradientStop {
                    position: 0.65
                    color: Theme.dark ? Qt.rgba(0.06, 0.05, 0.04, 0.80) : Qt.rgba(1, 1, 1, 0.88)
                }
                GradientStop {
                    position: 1.0
                    color: Theme.dark ? Qt.rgba(0.04, 0.03, 0.03, 0.96) : Qt.rgba(1, 1, 1, 0.97)
                }
            }
        }
    }

    // ------------------------------------------------------------- layout ----
    RowLayout {
        anchors.fill: parent
        anchors.bottomMargin: Theme.playerBarHeight + Theme.spacing5
        spacing: 0

        // ------------------------------------------------------------ rail --
        Item {
            Layout.preferredWidth: Theme.sidebarWidth
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacing4
                spacing: Theme.spacing4

                // Brand.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing3

                    Text {
                        Layout.fillWidth: true
                        text: "Aurora"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontTitle
                        font.weight: Theme.weightBold
                    }

                    IconButton {
                        icon: Theme.dark ? "sun" : "moon"
                        variant: "ghost"
                        diameter: 34
                        tooltip: Theme.dark ? qsTr("Light theme") : qsTr("Dark theme")
                        onClicked: player.setTheme(Theme.dark ? "light" : "dark")
                    }
                }

                // Navigation.
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing1

                    Repeater {
                        model: [
                            { icon: "disc", label: qsTr("Now playing") },
                            { icon: "library", label: qsTr("Library") },
                            { icon: "equalizer", label: qsTr("Equalizer") },
                            { icon: "settings", label: qsTr("Settings") }
                        ]

                        delegate: AbstractButton {
                            required property int index
                            required property var modelData

                            Layout.fillWidth: true
                            implicitHeight: 44
                            hoverEnabled: true

                            background: Rectangle {
                                radius: Theme.radiusMd
                                color: window.page === index
                                       ? Theme.alpha(Theme.accent, Theme.dark ? 0.20 : 0.12)
                                       : (parent.hovered ? Theme.alpha(Theme.text, 0.07) : "transparent")

                                Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                            }

                            contentItem: RowLayout {
                                spacing: Theme.spacing3

                                Item { implicitWidth: Theme.spacing1 }

                                IconGlyph {
                                    width: 20; height: 20
                                    name: modelData.icon
                                    color: window.page === index ? Theme.accent : Theme.textSecondary
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: window.page === index ? Theme.text : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontBody
                                    font.weight: window.page === index ? Theme.weightSemi : Theme.weightRegular
                                    elide: Text.ElideRight
                                }
                            }

                            onClicked: window.page = index
                        }
                    }
                }

                AuroraButton {
                    Layout.fillWidth: true
                    text: qsTr("Add music")
                    icon: "plus"
                    primary: true
                    onClicked: addDialog.open = true
                }

                Item { Layout.fillHeight: true }

                // Live engine telemetry - proof that things are healthy.
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: player.statusLine
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Repeater {
                            model: 2
                            delegate: Rectangle {
                                required property int index
                                Layout.fillWidth: true
                                height: 3
                                radius: 1.5
                                color: Theme.dark ? Qt.rgba(1, 1, 1, 0.12) : "#E6E5E3"

                                Rectangle {
                                    width: parent.width * (index === 0 ? player.levelLeft : player.levelRight)
                                    height: parent.height
                                    radius: parent.radius
                                    color: Theme.accent

                                    Behavior on width { NumberAnimation { duration: 90 } }
                                }
                            }
                        }
                    }
                }
            }
        }

        // --------------------------------------------------------- content --
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: window.page

            NowPlayingView {
                player: window.playerRef
                albums: albumModel
                tracks: libraryModel
            }

            LibraryView {
                player: window.playerRef
                tracks: libraryModel
                albums: albumModel
                onRequestAddMusic: addDialog.open = true
            }

            Item {
                EqualizerPanel {
                    anchors.fill: parent
                    anchors.margins: Theme.spacing5
                    player: window.playerRef
                }
            }

            SettingsView {
                player: window.playerRef
            }
        }

        // ------------------------------------------------------ side panel --
        Item {
            Layout.preferredWidth: window.sidePanel === 0 ? 0 : 330
            Layout.fillHeight: true
            visible: Layout.preferredWidth > 0
            clip: true

            Behavior on Layout.preferredWidth {
                NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easeStandard }
            }

            QueuePanel {
                anchors.fill: parent
                anchors.margins: Theme.spacing3
                anchors.leftMargin: 0
                visible: window.sidePanel === 1
                player: window.playerRef
                queue: queueModel
                backdrop: backdrop
            }

            LyricsPanel {
                anchors.fill: parent
                anchors.margins: Theme.spacing3
                anchors.leftMargin: 0
                visible: window.sidePanel === 2
                player: window.playerRef
                backdrop: backdrop
            }
        }
    }

    // Keeps QML happy about the injected context property.
    readonly property var playerRef: player

    // The QML theme singleton mirrors the C++ settings and the cover palette,
    // so switching theme or track re-tints the whole window at once.
    Binding { target: Theme; property: "dark"; value: player.darkTheme }
    Binding { target: Theme; property: "coverDominant"; value: player.dominantColor }
    Binding { target: Theme; property: "coverAccent"; value: player.accentColor }

    // ------------------------------------------------------- player bar ----
    PlayerBar {
        id: playerBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacing4
        height: Theme.playerBarHeight
        player: window.playerRef
        backdropSource: backdrop
        lyricsOpen: window.sidePanel === 2
        queueOpen: window.sidePanel === 1
        onToggleLyrics: window.sidePanel = window.sidePanel === 2 ? 0 : 2
        onToggleQueue: window.sidePanel = window.sidePanel === 1 ? 0 : 1
        onOpenNowPlaying: window.page = 0
    }

    // ----------------------------------------------------------- dialogs ----
    AddMusicDialog {
        id: addDialog
        player: window.playerRef
        downloads: downloadsModel
        z: 100
    }

    // Toast for status messages and errors.
    Rectangle {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: playerBar.top
        anchors.bottomMargin: Theme.spacing4
        width: Math.min(toastText.implicitWidth + Theme.spacing5 * 2, window.width - Theme.spacing8)
        height: 44
        radius: Theme.radiusPill
        color: toast.error ? Theme.danger : (Theme.dark ? "#2A2724" : "#2C2C2B")
        opacity: 0
        z: 90

        property bool error: false

        Text {
            id: toastText
            anchors.centerIn: parent
            color: "#FFFFFF"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontCaption
            font.weight: Theme.weightMedium
        }

        Behavior on opacity { NumberAnimation { duration: Theme.durationBase } }

        Timer {
            id: toastTimer
            interval: 3600
            onTriggered: toast.opacity = 0
        }

        function show(message, isError) {
            toastText.text = message
            toast.error = isError === true
            toast.opacity = 1
            toastTimer.restart()
        }
    }

    Connections {
        target: player
        function onNotice(message) { toast.show(message, false) }
        function onErrorOccurred(message) { toast.show(message, true) }
    }

    // ---------------------------------------------------- drag and drop ----
    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            for (var i = 0; i < drop.urls.length; ++i) player.addPath(drop.urls[i])
            if (drop.hasText && drop.urls.length === 0) player.addUrl(drop.text)
            drop.accept()
        }

        Rectangle {
            anchors.fill: parent
            visible: parent.containsDrag
            color: Theme.alpha(Theme.accent, 0.14)
            border.width: 2
            border.color: Theme.accent
            radius: Theme.radiusLg

            Text {
                anchors.centerIn: parent
                text: qsTr("Drop files or folders to add them")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontTitle
                font.weight: Theme.weightSemi
            }
        }
    }

    // -------------------------------------------------------- shortcuts ----
    Shortcut { sequences: ["Space", "Media Play"]; onActivated: player.togglePlayPause() }
    Shortcut { sequence: "Right"; onActivated: player.seekSeconds(player.positionSec + 5) }
    Shortcut { sequence: "Left"; onActivated: player.seekSeconds(player.positionSec - 5) }
    Shortcut { sequences: ["Ctrl+Right", "Media Next"]; onActivated: player.next() }
    Shortcut { sequences: ["Ctrl+Left", "Media Previous"]; onActivated: player.previous() }
    Shortcut { sequence: "Up"; onActivated: player.setVolume(Math.min(1, player.volume + 0.05)) }
    Shortcut { sequence: "Down"; onActivated: player.setVolume(Math.max(0, player.volume - 0.05)) }
    Shortcut { sequence: "M"; onActivated: player.toggleMute() }
    Shortcut { sequence: "S"; onActivated: player.toggleShuffle() }
    Shortcut { sequence: "R"; onActivated: player.cycleRepeat() }
    Shortcut { sequence: "F"; onActivated: player.toggleFavorite() }
    Shortcut { sequence: "L"; onActivated: window.sidePanel = window.sidePanel === 2 ? 0 : 2 }
    Shortcut { sequence: "Q"; onActivated: window.sidePanel = window.sidePanel === 1 ? 0 : 1 }
    Shortcut { sequence: "Ctrl+O"; onActivated: addDialog.open = true }
    Shortcut { sequence: "Ctrl+F"; onActivated: window.page = 1 }
    Shortcut { sequence: "Ctrl+E"; onActivated: window.page = 2 }
    Shortcut { sequence: "Ctrl+,"; onActivated: window.page = 3 }
    Shortcut { sequence: "Ctrl+T"; onActivated: player.setTheme(Theme.dark ? "light" : "dark") }
    Shortcut { sequence: "Escape"; onActivated: { if (addDialog.open) addDialog.open = false; else window.sidePanel = 0 } }

    // Remember the window size between sessions.
    onWidthChanged: player.setWindowSize(width, height)
    onHeightChanged: player.setWindowSize(width, height)
}
