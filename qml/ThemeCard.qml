import QtQuick
import Mindarchy 1.0
import QtQuick.Controls

Button {
    id: card
    required property var theme
    objectName: "theme-" + theme.id
    readonly property bool hasRecipe: !!theme.recipe.layout
    implicitHeight: hasRecipe || theme.refined ? 183 : 152
    checkable: false
    focusPolicy: Qt.StrongFocus
    Accessible.name: theme.name + (checked ? ", current theme" : ", apply theme")
    background: Rectangle {
        radius: 9
        color: card.down ? (ShellTheme.colors["#334653"] || "#334653") : (ShellTheme.colors["#23323c"] || "#23323c")
        border.width: card.checked || card.activeFocus ? 2 : 1
        border.color: card.checked || card.activeFocus ? (ShellTheme.colors["#70d8c4"] || "#70d8c4") : (ShellTheme.colors["#3a4b57"] || "#3a4b57")
    }
    contentItem: Item {
        Canvas {
            id: preview
            anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
            anchors.margins: 5; height: 104
            onWidthChanged: requestPaint()
            function drawRecipe(c,w,h) {
                var t=card.theme, layout=t.recipe.layout
                function drawNode(x,y,ww,hh,style,label) {
                    c.fillStyle=style.fill; c.strokeStyle=style.border; c.lineWidth=Math.max(.7,style.borderWidth*.65)
                    c.beginPath()
                    if(style.shape===3) { c.strokeStyle=style.branch; c.moveTo(x,y+hh); c.lineTo(x+ww,y+hh); c.stroke() }
                    else if(style.shape!==6) {
                        if(style.shape===1) c.rect(x,y,ww,hh)
                        else c.roundedRect(x,y,ww,hh,style.shape===2?hh/2:4,style.shape===2?hh/2:4)
                        c.fill(); if(style.borderWidth>0) c.stroke()
                    }
                    c.fillStyle=style.text; c.font="8px '" + t.fontFamily + "'"
                    if(label) c.fillText(label,x+6,y+hh/2+3)
                    else if(t.refined && hh>=16) {
                        c.strokeStyle=style.taskAccent; c.lineWidth=style.taskCheckWidth*.55;
                        c.lineCap="round"; c.lineJoin="round"; c.beginPath();
                        c.moveTo(x+5,y+hh/2); c.lineTo(x+8,y+hh/2+3); c.lineTo(x+13,y+hh/2-4); c.stroke();
                        c.fillRect(x+18,y+hh/2,Math.max(4,ww-24),1);
                    } else c.fillRect(x+6,y+hh/2,Math.max(6,ww-12),1)
                }
                function edge(x1,y1,x2,y2,color) {
                    c.strokeStyle=color; c.lineWidth=1.2; c.beginPath(); c.moveTo(x1,y1)
                    if(t.recipe.branchStyle==="Angular") {
                        if(layout==="Vertical") { c.lineTo(x1,(y1+y2)/2); c.lineTo(x2,(y1+y2)/2) }
                        else { c.lineTo((x1+x2)/2,y1); c.lineTo((x1+x2)/2,y2) }
                        c.lineTo(x2,y2)
                    } else c.bezierCurveTo((x1+x2)/2,y1,(x1+x2)/2,y2,x2,y2)
                    c.stroke()
                }
                if(layout==="Vertical") {
                    drawNode(w/2-22,6,44,18,t.previewRoot,"Ideas")
                    for(var i=0;i<3;i++) {
                        var x=10+i*(w-54)/2, b=t.previewBranches[i], leaf=t.previewLeaves[i]
                        edge(w/2,24,x+17,49,b.branch); drawNode(x,49,34,16,b,"")
                        edge(x+17,65,x+17,85,leaf.branch); drawNode(x+2,85,30,12,leaf,"")
                    }
                } else if(layout==="Compact") {
                    drawNode(12,5,47,16,t.previewRoot,"Ideas")
                    for(var j=0;j<3;j++) {
                        var yy=28+j*23, bb=t.previewBranches[j]
                        c.strokeStyle=bb.branch; c.lineWidth=1; c.beginPath()
                        c.moveTo(23,21); c.lineTo(23,yy+6); c.lineTo(45,yy+6); c.stroke()
                        drawNode(45,yy,w*.40,12,bb,"")
                        c.beginPath(); c.moveTo(53,yy+12); c.lineTo(53,yy+20); c.lineTo(65,yy+20); c.stroke()
                        drawNode(65,yy+16,w*.45,8,t.previewLeaves[j],"")
                    }
                } else {
                    drawNode(10,43,45,20,t.previewRoot,"Ideas")
                    for(var k=0;k<3;k++) {
                        var y=10+k*34, xx=w*.43, branch=t.previewBranches[k], ll=t.previewLeaves[k]
                        edge(55,53,xx,y+9,branch.branch); drawNode(xx,y,36,18,branch,"")
                        edge(xx+36,y+9,w*.79,y+14,ll.branch); drawNode(w*.79,y+4,w*.17,12,ll,"")
                    }
                }
            }
            onPaint: {
                var c = getContext("2d"), w = width, h = height
                c.reset(); c.fillStyle = card.theme.canvas; c.fillRect(0,0,w,h)
                if (card.hasRecipe || card.theme.refined) { drawRecipe(c,w,h); return }
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
                c.font="bold 9px '" + card.theme.fontFamily + "'"; c.fillText("Ideas",20,57)
            }
        }
        Row {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.bottomMargin: card.hasRecipe || card.theme.refined ? 35 : 7; anchors.leftMargin: 7; anchors.rightMargin: 7
            spacing: 7
            Label { text: card.checked ? "✓" : ""; color: (ShellTheme.colors["#70d8c4"] || "#70d8c4"); width: 12 }
            Label { text: card.theme.name; color: (ShellTheme.colors["#e0e9ee"] || "#e0e9ee"); font.pixelSize: 12; font.bold: card.checked }
        }
        Label {
            visible: card.hasRecipe || card.theme.refined
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 7
            text: card.hasRecipe ? card.theme.recipe.name + " · " + card.theme.recipe.layout : card.theme.fontFamily + (card.theme.isDefault ? " · Default" : "")
            color: (ShellTheme.colors["#94a9b7"] || "#a7bdcb"); font.pixelSize: 10; elide: Text.ElideRight
        }
    }
}
