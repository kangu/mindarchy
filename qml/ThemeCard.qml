import QtQuick
import QtQuick.Controls

Button {
    id: card
    required property var theme
    objectName: "theme-" + theme.id
    implicitHeight: 152
    checkable: false
    focusPolicy: Qt.StrongFocus
    Accessible.name: theme.name + (checked ? ", current theme" : ", apply theme")
    background: Rectangle {
        radius: 9
        color: card.down ? "#334653" : "#23323c"
        border.width: card.checked || card.activeFocus ? 2 : 1
        border.color: card.checked || card.activeFocus ? "#70d8c4" : "#3a4b57"
    }
    contentItem: Item {
        Canvas {
            id: preview
            anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
            anchors.margins: 5; height: 104
            onWidthChanged: requestPaint()
            onPaint: {
                var c = getContext("2d"), w = width, h = height
                c.reset(); c.fillStyle = card.theme.canvas; c.fillRect(0,0,w,h)
                var id = card.theme.id, colors = card.theme.palette
                var dark = id === "arcade" || id === "lab"
                function node(x,y,ww,hh,fill,stroke,shape) {
                    c.fillStyle=fill; c.strokeStyle=stroke; c.lineWidth=1.4
                    c.beginPath()
                    if (shape === "hex") {
                        c.moveTo(x+5,y); c.lineTo(x+ww-5,y); c.lineTo(x+ww,y+hh/2)
                        c.lineTo(x+ww-5,y+hh); c.lineTo(x+5,y+hh); c.lineTo(x,y+hh/2); c.closePath()
                    } else if(shape === "pill") {
                        c.roundedRect(x,y,ww,hh,hh/2,hh/2)
                    } else c.rect(x,y,ww,hh)
                    c.fill(); c.stroke()
                }
                var root = id === "beach-day" ? "#f3ddb4" : id === "holographic" ? "#f3cdf3" : id === "retro" ? "#32345f" : "#81364e"
                var black = id === "holographic"
                node(12,43,43,22,root,black ? "#17141d" : root,id === "retro" ? "hex" : "rect")
                for(var i=0;i<3;++i) {
                    var col=colors[i % colors.length], y=14+i*32, x=w*.43, leaf=w*.77
                    c.strokeStyle=black ? "#17141d" : col; c.lineWidth=1.5
                    c.beginPath(); c.moveTo(55,54); c.bezierCurveTo(x-15,54,x-15,y+9,x,y+9); c.stroke()
                    var fill=id === "beach-day" ? "#ffffff" : col
                    node(x,y,37,18,fill,black ? "#17141d" : col,id === "arcade" ? "pill" : "rect")
                    c.beginPath(); c.moveTo(x+37,y+9); c.lineTo(leaf+20,y+9); c.stroke()
                    if(id === "holographic" || id === "retro")
                        node(leaf,y+2,22,14,id === "retro" ? card.theme.canvas : col,black ? "#17141d" : col,"pill")
                    c.fillStyle=dark ? "#f3e7d2" : "#35333d"
                    c.fillRect(x+8,y+8,20,1)
                }
                c.fillStyle=dark || id === "retro" ? "#fff0d4" : "#35333d"
                c.font="bold 9px sans-serif"; c.fillText("Ideas",20,57)
            }
        }
        Row {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 7
            spacing: 7
            Label { text: card.checked ? "✓" : ""; color: "#70d8c4"; width: 12 }
            Label { text: card.theme.name; color: "#e0e9ee"; font.pixelSize: 12; font.bold: card.checked }
        }
    }
}
