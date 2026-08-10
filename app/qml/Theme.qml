// Aurora Player - single source of truth for every visual token.
//
// Registered as a QML singleton (see app/CMakeLists.txt), so any component can
// simply write `Theme.accent` or `Theme.spacing4`. Switching `dark` retints the
// whole application without reloading anything.
pragma Singleton
import QtQuick

QtObject {
    id: theme

    // ---------------------------------------------------------------- mode --
    property bool dark: true

    /// Colours pulled from the current cover art by the C++ side. The dark
    /// theme uses them for the blurred backdrop, which is what makes the
    /// player feel like it belongs to the album that is playing.
    property color coverDominant: "#2A1D16"
    property color coverAccent: "#F0A45E"
    property color coverMuted: "#7A5B44"

    // -------------------------------------------------------------- surfaces --
    readonly property color canvas: dark ? "#131110" : "#FFFFFF"
    readonly property color surface: dark ? "#1C1A18" : "#F9F8F7"
    readonly property color surfaceRaised: dark ? "#262320" : "#F0EFED"
    readonly property color surfaceHover: dark ? "#302C29" : "#E9E8E6"
    readonly property color border: dark ? Qt.rgba(1, 1, 1, 0.14) : "#E6E5E3"
    readonly property color borderStrong: dark ? Qt.rgba(1, 1, 1, 0.24) : "#D6D4D1"

    // ---------------------------------------------------------------- text --
    readonly property color text: dark ? "#FFFFFF" : "#2C2C2B"
    readonly property color textSecondary: dark ? Qt.rgba(1, 1, 1, 0.66) : "#7D7A75"
    readonly property color textMuted: dark ? Qt.rgba(1, 1, 1, 0.40) : "#A5A19B"

    // -------------------------------------------------------------- accents --
    readonly property color accent: dark ? coverAccent : "#2783DE"
    readonly property color accentText: dark ? "#1A1207" : "#FFFFFF"
    readonly property color danger: "#E5484D"
    readonly property color success: "#46A758"

    // ---------------------------------------------------------------- glass --
    readonly property color glass: dark ? Qt.rgba(0.11, 0.10, 0.09, 0.62)
                                       : Qt.rgba(1, 1, 1, 0.72)
    readonly property color glassBorder: dark ? Qt.rgba(1, 1, 1, 0.16)
                                              : Qt.rgba(0, 0, 0, 0.08)
    readonly property color shadow: dark ? Qt.rgba(0, 0, 0, 0.55) : Qt.rgba(0, 0, 0, 0.16)
    readonly property color scrim: dark ? Qt.rgba(0, 0, 0, 0.55) : Qt.rgba(0, 0, 0, 0.28)

    // -------------------------------------------------------------- spacing --
    readonly property int spacing1: 4
    readonly property int spacing2: 8
    readonly property int spacing3: 12
    readonly property int spacing4: 16
    readonly property int spacing5: 24
    readonly property int spacing6: 32
    readonly property int spacing7: 48
    readonly property int spacing8: 64

    // --------------------------------------------------------------- radius --
    readonly property int radiusSm: 8
    readonly property int radiusMd: 12
    readonly property int radiusLg: 20
    readonly property int radiusXl: 28
    readonly property int radiusPill: 999

    // ----------------------------------------------------------------- type --
    readonly property string fontFamily: fontProbe.name
    readonly property int fontCaption: 13
    readonly property int fontBody: 15
    readonly property int fontBodyLarge: 17
    readonly property int fontTitle: 20
    readonly property int fontHeading: 26
    readonly property int fontDisplay: 34
    readonly property int weightRegular: Font.Normal
    readonly property int weightMedium: Font.Medium
    readonly property int weightSemi: Font.DemiBold
    readonly property int weightBold: Font.Bold

    // -------------------------------------------------------------- metrics --
    readonly property int controlHeight: 44   // WCAG friendly hit target
    readonly property int iconButton: 40
    readonly property int iconSize: 20
    readonly property int sidebarWidth: 248
    readonly property int playerBarHeight: 92

    // ------------------------------------------------------------- motion --
    readonly property int durationFast: 120
    readonly property int durationBase: 200
    readonly property int durationSlow: 340
    readonly property int easeStandard: Easing.OutCubic
    readonly property int easeEmphasis: Easing.OutBack

    /// Same colour with a different alpha - handy for hover overlays.
    function alpha(base, a) {
        return Qt.rgba(base.r, base.g, base.b, a)
    }

    /// Readable text colour for an arbitrary background.
    function onColor(background) {
        const luminance = 0.299 * background.r + 0.587 * background.g + 0.114 * background.b
        return luminance > 0.55 ? "#1A1A19" : "#FFFFFF"
    }

    /// Picks the first font that is actually installed on this machine.
    property FontLoader fontProbe: FontLoader {
        name: {
            const wanted = ["Inter", "SF Pro Display", "Segoe UI Variable Display",
                            "Segoe UI", "Noto Sans", "DejaVu Sans"]
            const available = Qt.fontFamilies()
            for (let i = 0; i < wanted.length; ++i) {
                if (available.indexOf(wanted[i]) !== -1) return wanted[i]
            }
            return ""
        }
    }
}
