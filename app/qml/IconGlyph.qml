// Aurora Player - every icon in the app, drawn with code.
//
// Using a Canvas instead of image assets keeps the repository free of binary
// files, makes icons crisp on any DPI and lets them recolour instantly when the
// theme changes. All glyphs are authored on a 24x24 grid and scaled from there.
import QtQuick

Canvas {
    id: glyph

    // play, pause, prev, next, shuffle, repeat, repeatOne, volume, volumeLow,
    // mute, heart, heartFilled, list, lyrics, search, plus, folder, link,
    // video, equalizer, settings, close, back, external, refresh, library,
    // download, sun, moon, chevronLeft, chevronRight, check, trash, disc
    property string name: "play"
    property color color: "#FFFFFF"
    property real strokeWidth: 1.9

    implicitWidth: 24
    implicitHeight: 24
    antialiasing: true

    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        var s = Math.min(width, height) / 24
        ctx.scale(s, s)
        ctx.fillStyle = glyph.color
        ctx.strokeStyle = glyph.color
        ctx.lineWidth = glyph.strokeWidth
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        paintGlyph(ctx, glyph.name)
    }

    function paintGlyph(ctx, name) {
        if (name === "play") {
            triangle(ctx, 8, 5, 19, 12, 8, 19)
        } else if (name === "pause") {
            roundRect(ctx, 7.5, 5, 3.4, 14, 1.2, true)
            roundRect(ctx, 13.1, 5, 3.4, 14, 1.2, true)
        } else if (name === "prev") {
            triangle(ctx, 18, 5.5, 18, 18.5, 8.5, 12)
            roundRect(ctx, 5.4, 5.5, 2.2, 13, 1.1, true)
        } else if (name === "next") {
            triangle(ctx, 6, 5.5, 15.5, 12, 6, 18.5)
            roundRect(ctx, 16.4, 5.5, 2.2, 13, 1.1, true)
        } else if (name === "shuffle") {
            line(ctx, [[3, 7], [7, 7]])
            curve(ctx, 7, 7, 12, 7, 12, 17)
            line(ctx, [[12, 17], [16.5, 17]])
            arrowRight(ctx, 17, 17)
            line(ctx, [[3, 17], [7, 17]])
            curve(ctx, 7, 17, 11, 17, 11.6, 12.6)
            line(ctx, [[13.4, 10.2], [16.5, 7]])
            arrowRight(ctx, 17, 7)
        } else if (name === "repeat" || name === "repeatOne") {
            line(ctx, [[7.5, 7.5], [16, 7.5]])
            curve(ctx, 16, 7.5, 19.5, 7.5, 19.5, 11)
            arrowDown(ctx, 19.5, 12.6)
            line(ctx, [[16.5, 16.5], [8, 16.5]])
            curve(ctx, 8, 16.5, 4.5, 16.5, 4.5, 13)
            arrowUp(ctx, 4.5, 11.4)
            if (name === "repeatOne") {
                ctx.beginPath()
                ctx.arc(12, 12, 3.4, 0, Math.PI * 2)
                ctx.fill()
            }
        } else if (name === "volume" || name === "volumeLow" || name === "mute") {
            ctx.beginPath()
            ctx.moveTo(4, 9.5)
            ctx.lineTo(7.5, 9.5)
            ctx.lineTo(11.5, 5.5)
            ctx.lineTo(11.5, 18.5)
            ctx.lineTo(7.5, 14.5)
            ctx.lineTo(4, 14.5)
            ctx.closePath()
            ctx.fill()
            if (name === "mute") {
                line(ctx, [[15, 9.5], [20, 14.5]])
                line(ctx, [[20, 9.5], [15, 14.5]])
            } else {
                ctx.beginPath()
                ctx.arc(13.4, 12, 3.1, -Math.PI / 3, Math.PI / 3)
                ctx.stroke()
                if (name === "volume") {
                    ctx.beginPath()
                    ctx.arc(13.4, 12, 6.1, -Math.PI / 3, Math.PI / 3)
                    ctx.stroke()
                }
            }
        } else if (name === "heart" || name === "heartFilled") {
            ctx.beginPath()
            ctx.moveTo(12, 19.2)
            ctx.bezierCurveTo(5.2, 14.6, 3.4, 11.2, 5.6, 8.6)
            ctx.bezierCurveTo(7.4, 6.5, 10.3, 7, 12, 9.2)
            ctx.bezierCurveTo(13.7, 7, 16.6, 6.5, 18.4, 8.6)
            ctx.bezierCurveTo(20.6, 11.2, 18.8, 14.6, 12, 19.2)
            ctx.closePath()
            if (name === "heartFilled") ctx.fill()
            else ctx.stroke()
        } else if (name === "list" || name === "library") {
            for (var i = 0; i < 3; ++i) {
                var y = 7 + i * 5
                roundRect(ctx, 4, y - 0.9, 2, 1.9, 0.9, true)
                roundRect(ctx, 8.5, y - 0.9, 11.5, 1.9, 0.9, true)
            }
        } else if (name === "lyrics") {
            roundRect(ctx, 4.5, 6, 11, 1.8, 0.9, true)
            roundRect(ctx, 4.5, 11.1, 15, 1.8, 0.9, true)
            roundRect(ctx, 4.5, 16.2, 8, 1.8, 0.9, true)
        } else if (name === "search") {
            ctx.beginPath()
            ctx.arc(10.8, 10.8, 5.4, 0, Math.PI * 2)
            ctx.stroke()
            line(ctx, [[15, 15], [19.5, 19.5]])
        } else if (name === "plus") {
            line(ctx, [[12, 5.5], [12, 18.5]])
            line(ctx, [[5.5, 12], [18.5, 12]])
        } else if (name === "check") {
            line(ctx, [[5.5, 12.6], [10, 17], [18.5, 7.6]])
        } else if (name === "close") {
            line(ctx, [[6.5, 6.5], [17.5, 17.5]])
            line(ctx, [[17.5, 6.5], [6.5, 17.5]])
        } else if (name === "back" || name === "chevronLeft") {
            line(ctx, [[15, 5.5], [8.2, 12], [15, 18.5]])
        } else if (name === "chevronRight") {
            line(ctx, [[9, 5.5], [15.8, 12], [9, 18.5]])
        } else if (name === "folder") {
            ctx.beginPath()
            ctx.moveTo(3.5, 7.5)
            ctx.lineTo(9.5, 7.5)
            ctx.lineTo(11.5, 9.8)
            ctx.lineTo(20.5, 9.8)
            ctx.lineTo(20.5, 18.5)
            ctx.lineTo(3.5, 18.5)
            ctx.closePath()
            ctx.stroke()
        } else if (name === "link") {
            ctx.beginPath()
            ctx.arc(9.4, 14.6, 4.2, Math.PI * 0.25, Math.PI * 1.55)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(14.6, 9.4, 4.2, Math.PI * 1.25, Math.PI * 0.55)
            ctx.stroke()
            line(ctx, [[9.6, 14.4], [14.4, 9.6]])
        } else if (name === "video") {
            roundRect(ctx, 3.5, 6.5, 17, 11, 3.4, false)
            triangle(ctx, 10.6, 9.6, 15.4, 12, 10.6, 14.4)
        } else if (name === "download") {
            line(ctx, [[12, 4.5], [12, 14.5]])
            line(ctx, [[7.6, 10.6], [12, 15], [16.4, 10.6]])
            line(ctx, [[5, 19], [19, 19]])
        } else if (name === "equalizer") {
            for (var b = 0; b < 3; ++b) {
                var x = 6.5 + b * 5.5
                line(ctx, [[x, 4.5], [x, 19.5]])
                ctx.beginPath()
                ctx.arc(x, b === 1 ? 9 : 15, 2.5, 0, Math.PI * 2)
                ctx.fill()
            }
        } else if (name === "settings") {
            ctx.beginPath()
            ctx.arc(12, 12, 3.2, 0, Math.PI * 2)
            ctx.stroke()
            for (var t = 0; t < 8; ++t) {
                var a = t * Math.PI / 4
                line(ctx, [[12 + Math.cos(a) * 6, 12 + Math.sin(a) * 6],
                           [12 + Math.cos(a) * 8.4, 12 + Math.sin(a) * 8.4]])
            }
        } else if (name === "external") {
            line(ctx, [[13.5, 5], [19, 5], [19, 10.5]])
            line(ctx, [[19, 5], [11, 13]])
            line(ctx, [[16.5, 13.5], [16.5, 19], [5, 19], [5, 7.5], [10.5, 7.5]])
        } else if (name === "refresh") {
            ctx.beginPath()
            ctx.arc(12, 12, 6.6, Math.PI * 0.35, Math.PI * 1.8)
            ctx.stroke()
            triangle(ctx, 15.2, 3.4, 19.6, 7.2, 14.2, 8.4)
        } else if (name === "sun") {
            ctx.beginPath()
            ctx.arc(12, 12, 4.2, 0, Math.PI * 2)
            ctx.fill()
            for (var r = 0; r < 8; ++r) {
                var ra = r * Math.PI / 4
                line(ctx, [[12 + Math.cos(ra) * 6.4, 12 + Math.sin(ra) * 6.4],
                           [12 + Math.cos(ra) * 8.6, 12 + Math.sin(ra) * 8.6]])
            }
        } else if (name === "moon") {
            ctx.beginPath()
            ctx.arc(12.6, 12, 7.4, Math.PI * 0.32, Math.PI * 1.68)
            ctx.arc(9.4, 12, 8.4, Math.PI * 1.72, Math.PI * 0.28, true)
            ctx.closePath()
            ctx.fill()
        } else if (name === "trash") {
            line(ctx, [[4.5, 7.5], [19.5, 7.5]])
            line(ctx, [[9.5, 7.5], [9.5, 4.8], [14.5, 4.8], [14.5, 7.5]])
            line(ctx, [[6.5, 7.5], [7.6, 19.5], [16.4, 19.5], [17.5, 7.5]])
        } else if (name === "disc") {
            ctx.beginPath()
            ctx.arc(12, 12, 8.4, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(12, 12, 2.4, 0, Math.PI * 2)
            ctx.fill()
        } else {
            triangle(ctx, 8, 5, 19, 12, 8, 19)
        }
    }

    // ------------------------------------------------------------- helpers --
    function triangle(ctx, x1, y1, x2, y2, x3, y3) {
        ctx.beginPath()
        ctx.moveTo(x1, y1)
        ctx.lineTo(x2, y2)
        ctx.lineTo(x3, y3)
        ctx.closePath()
        ctx.fill()
    }

    function line(ctx, points) {
        ctx.beginPath()
        ctx.moveTo(points[0][0], points[0][1])
        for (var i = 1; i < points.length; ++i) ctx.lineTo(points[i][0], points[i][1])
        ctx.stroke()
    }

    function curve(ctx, x1, y1, cx, cy, x2, y2) {
        ctx.beginPath()
        ctx.moveTo(x1, y1)
        ctx.quadraticCurveTo(cx, cy, x2, y2)
        ctx.stroke()
    }

    function roundRect(ctx, x, y, w, h, r, filled) {
        ctx.beginPath()
        ctx.moveTo(x + r, y)
        ctx.lineTo(x + w - r, y)
        ctx.quadraticCurveTo(x + w, y, x + w, y + r)
        ctx.lineTo(x + w, y + h - r)
        ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h)
        ctx.lineTo(x + r, y + h)
        ctx.quadraticCurveTo(x, y + h, x, y + h - r)
        ctx.lineTo(x, y + r)
        ctx.quadraticCurveTo(x, y, x + r, y)
        ctx.closePath()
        if (filled) ctx.fill()
        else ctx.stroke()
    }

    function arrowRight(ctx, x, y) {
        triangle(ctx, x - 3.4, y - 2.8, x - 3.4, y + 2.8, x + 0.6, y)
    }

    function arrowDown(ctx, x, y) {
        triangle(ctx, x - 2.8, y - 1.4, x + 2.8, y - 1.4, x, y + 2.6)
    }

    function arrowUp(ctx, x, y) {
        triangle(ctx, x - 2.8, y + 1.4, x + 2.8, y + 1.4, x, y - 2.6)
    }
}
