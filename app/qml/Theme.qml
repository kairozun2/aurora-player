// Aurora Player - single source of truth for every visual token.
//
// Registered as a QML singleton (see app/CMakeLists.txt), so any component can
// simply write `Theme.accent` or `Theme.spacing4`. Switching `dark` or
// `paletteIndex` retints the whole application without reloading anything.
pragma Singleton
import QtQuick
import QtCore

QtObject {
    id: theme

    // ---------------------------------------------------------------- mode --
    property bool dark: true

    /// Colours pulled from the current cover art by the C++ side. The Aurora
    /// palette uses them for the blurred backdrop, which is what makes the
    /// player feel like it belongs to the album that is playing.
    property color coverDominant: "#2A1D16"
    property color coverAccent: "#F0A45E"
    property color coverMuted: "#7A5B44"

    // ------------------------------------------------------------ palettes --
    /// Every palette ships a dark and a light variant, so the sun/moon button
    /// keeps working whichever colour family is selected.
    readonly property var palettes: [
        { name: "Aurora",
          dark:  { canvas: "#131110", surface: "#1C1A18", raised: "#262320", hover: "#302C29",
                   text: "#FFFFFF", accent: "#F0A45E", accentText: "#1A1207", cover: true },
          light: { canvas: "#FFFFFF", surface: "#F9F8F7", raised: "#F0EFED", hover: "#E9E8E6",
                   text: "#2C2C2B", accent: "#2783DE", accentText: "#FFFFFF", cover: false } },
        { name: "Midnight",
          dark:  { canvas: "#0D0F1A", surface: "#151827", raised: "#1E2233", hover: "#272C40",
                   text: "#EEF1FF", accent: "#7C6CFF", accentText: "#0B0B18", cover: false },
          light: { canvas: "#FFFFFF", surface: "#F6F6FB", raised: "#EDEDF6", hover: "#E4E4F0",
                   text: "#23233A", accent: "#5A4BE0", accentText: "#FFFFFF", cover: false } },
        { name: "Ocean",
          dark:  { canvas: "#08161A", surface: "#0E2027", raised: "#163038", hover: "#1E3D47",
                   text: "#E7F7FA", accent: "#22C7C7", accentText: "#04211F", cover: false },
          light: { canvas: "#FFFFFF", surface: "#F2FAFB", raised: "#E6F4F6", hover: "#D8ECEF",
                   text: "#16323A", accent: "#0E8F9B", accentText: "#FFFFFF", cover: false } },
        { name: "Forest",
          dark:  { canvas: "#0C130E", surface: "#131C16", raised: "#1B271E", hover: "#243426",
                   text: "#EAF6EC", accent: "#4FBF6B", accentText: "#06210E", cover: false },
          light: { canvas: "#FFFFFF", surface: "#F3FAF4", raised: "#E8F4EA", hover: "#DCEDDF",
                   text: "#1D2E22", accent: "#2E9E52", accentText: "#FFFFFF", cover: false } },
        { name: "Sunset",
          dark:  { canvas: "#170D10", surface: "#211318", raised: "#2D1B21", hover: "#3A242B",
                   text: "#FFEFF1", accent: "#FF7A59", accentText: "#24100A", cover: false },
          light: { canvas: "#FFFFFF", surface: "#FFF6F3", raised: "#FDEBE5", hover: "#F8DED6",
                   text: "#33211E", accent: "#E2562F", accentText: "#FFFFFF", cover: false } },
        { name: "Rose",
          dark:  { canvas: "#140E14", surface: "#1D141D", raised: "#291C29", hover: "#362336",
                   text: "#FBEEFA", accent: "#E56BC9", accentText: "#21091C", cover: false },
          light: { canvas: "#FFFFFF", surface: "#FDF5FB", raised: "#F7E9F5", hover: "#EFDCEC",
                   text: "#2E2130", accent: "#C0399F", accentText: "#FFFFFF", cover: false } },
        { name: "Nord",
          dark:  { canvas: "#14181F", surface: "#1C222B", raised: "#262E39", hover: "#313A47",
                   text: "#ECEFF4", accent: "#88C0D0", accentText: "#10151A", cover: false },
          light: { canvas: "#FFFFFF", surface: "#F4F6F9", raised: "#E9EDF2", hover: "#DDE3EA",
                   text: "#2E3440", accent: "#4C7C93", accentText: "#FFFFFF", cover: false } },
        { name: "Mono",
          dark:  { canvas: "#0B0B0B", surface: "#151515", raised: "#1F1F1F", hover: "#2A2A2A",
                   text: "#FFFFFF", accent: "#E6E6E6", accentText: "#101010", cover: false },
          light: { canvas: "#FFFFFF", surface: "#F5F5F5", raised: "#EBEBEB", hover: "#E0E0E0",
                   text: "#171717", accent: "#171717", accentText: "#FFFFFF", cover: false } }
    ]

    property int paletteIndex: 0

    /// Remembers the chosen palette between runs.
    property Settings store: Settings {
        category: "appearance"
        property int palette: 0
    }

    readonly property var pal: {
        const i = Math.max(0, Math.min(palettes.length - 1, paletteIndex))
        return dark ? palettes[i].dark : palettes[i].light
    }

    function setPalette(index) {
        const i = Math.max(0, Math.min(palettes.length - 1, index))
        paletteIndex = i
        store.palette = i
    }

    function paletteName(index) {
        const i = Math.max(0, Math.min(palettes.length - 1, index))
        return palettes[i].name
    }

    // -------------------------------------------------------------- surfaces --
    readonly property color canvas: pal.canvas
    readonly property color surface: pal.surface
    readonly property color surfaceRaised: pal.raised
    readonly property color surfaceHover: pal.hover
    readonly property color border: dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.10)
    readonly property color borderStrong: dark ? Qt.rgba(1, 1, 1, 0.24) : Qt.rgba(0, 0, 0, 0.18)

    // ---------------------------------------------------------------- text --
    readonly property color text: pal.text
    readonly property color textSecondary: dark ? Qt.rgba(1, 1, 1, 0.66) : Qt.rgba(0, 0, 0, 0.55)
    readonly property color textMuted: dark ? Qt.rgba(1, 1, 1, 0.40) : Qt.rgba(0, 0, 0, 0.36)

    // -------------------------------------------------------------- accents --
    readonly property color accent: (pal.cover === true && dark) ? coverAccent : pal.accent
    readonly property color accentText: pal.accentText
    readonly property color danger: "#E5484D"
    readonly property color success: "#46A758"

    // ---------------------------------------------------------------- glass --
    readonly property color glass: Qt.rgba(canvas.r, canvas.g, canvas.b, dark ? 0.97 : 0.98)
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
    /// The first of these families that is actually installed here. Qt 6 made
    /// FontLoader.name read-only, so the pick has to happen in this binding.
    readonly property string fontFamily: {
        const wanted = ["Inter", "SF Pro Display", "Segoe UI Variable Display",
                        "Segoe UI", "Noto Sans", "DejaVu Sans"]
        const available = Qt.fontFamilies()
        for (let i = 0; i < wanted.length; ++i) {
            if (available.indexOf(wanted[i]) !== -1) return wanted[i]
        }
        return ""
    }
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
    readonly property var alpha: function (base, a) {
        return Qt.rgba(base.r, base.g, base.b, a)
    }

    /// Readable text colour for an arbitrary background.
    function onColor(background) {
        const luminance = 0.299 * background.r + 0.587 * background.g + 0.114 * background.b
        return luminance > 0.55 ? "#1A1A19" : "#FFFFFF"
    }

    Component.onCompleted: paletteIndex = store.palette
}
