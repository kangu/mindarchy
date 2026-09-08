#include "preview.h"
#include "drawing.h"
#include "engine.h"
#include <QAbstractTextDocumentLayout>
#include <QTextDocument>
#include <algorithm>

namespace {
class PreviewText : public QTextDocument {
    QVariant loadResource(int, const QUrl &) override { return {}; }
};
}
QImage renderMapPreview(const Engine &engine, QSize maximum) {
    maximum = maximum.boundedTo(QSize(4096,4096)).expandedTo(QSize(32,32));
    const QRectF bounds = engine.bounds().adjusted(-32,-32,32,32);
    const qreal scale = std::min({2., maximum.width()/bounds.width(), maximum.height()/bounds.height()});
    QImage image(QSize(qMax(1,int(bounds.width()*scale)),qMax(1,int(bounds.height()*scale))), QImage::Format_ARGB32_Premultiplied);
    image.fill(engine.canvasColor());
    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    painter.scale(scale,scale); painter.translate(-bounds.topLeft());
    const auto &nodes=engine.nodes();
    const bool vertical=engine.layout()=="Vertical", compact=engine.layout()=="Compact";
    QSet<int> visible(engine.visibleIds().begin(),engine.visibleIds().end());
    for(int id:engine.visibleIds()) {
        const auto &n=nodes[id]; if(!visible.contains(n.parent)) continue;
        const auto r=n.rect, parent=nodes[n.parent].rect;
        const auto style=engine.appearance(id);
        QPointF a,b;
        if(vertical) { a={parent.center().x(),parent.bottom()}; b={r.center().x(),r.top()}; }
        else if(compact) { a={parent.left()+12,parent.bottom()}; b={r.left(),r.center().y()}; }
        else { const bool left=engine.manual() && r.center().x()<parent.center().x();
            a={left ? parent.left():parent.right(),parent.center().y()};
            b={left ? r.right():r.left(),r.center().y()}; }
        if(!vertical) {
            if(style.shape==NodeShape::Underline) b.setY(r.bottom());
            if(!compact && engine.appearance(n.parent).shape==NodeShape::Underline) a.setY(parent.bottom());
        }
        if(style.branchWidth>0) {
            painter.setPen(QPen(style.branch,style.branchWidth,style.branchStroke));
            painter.drawPolyline(MapDrawing::edgePath(a,b,compact || engine.branchStyle()=="Angular",vertical || compact));
        }
    }
    for(const auto &link:engine.connections()) if(visible.contains(link.first) && visible.contains(link.second)) {
        painter.setPen(QPen(QColor("#efb86f"),2));
        painter.drawPolyline(MapDrawing::edgePath(nodes[link.first].rect.center(),nodes[link.second].rect.center(),false,false));
    }
    for(int id:engine.visibleIds()) {
        const auto &n=nodes[id]; const auto r=n.rect; const auto style=engine.appearance(id);
        painter.setPen(style.borderWidth>0 ? QPen(style.border,style.borderWidth,style.borderStyle) : QPen(Qt::NoPen));
        painter.setBrush(style.fill);
        if(style.shape==NodeShape::Underline) {
            painter.setPen(QPen(style.branch,style.branchWidth,style.branchStroke));
            if(style.branchWidth>0) painter.drawLine(r.bottomLeft(),r.bottomRight());
        } else if(style.shape!=NodeShape::Embedded) painter.drawPolygon(MapDrawing::shapePolygon(r,style.shape,style.radius));
        // Labels smaller than a few output pixels are illegible; avoid spending
        // seconds shaping thousands of texts in overview thumbnails.
        if(scale<.18) continue;
        painter.save(); painter.translate(r.topLeft());
        if(n.kind=="date") MapDrawing::paintCalendar(painter,n.calendar,style);
        else {
            if(n.task) {
                MapDrawing::paintTask(painter, QRectF(8,r.height()/2-5,10,10), style.text, n.checked);
            }
            PreviewText text; QFont font("sans-serif"); font.setPixelSize(15);
            text.setDefaultFont(font); text.setDocumentMargin(0);
            text.setDefaultStyleSheet(QString("body,p {color:%1; margin:0;}").arg(style.text.name()));
            text.setHtml(n.text); text.setTextWidth(std::max(20.,r.width()-30-(n.task?20:0)));
            painter.translate(15+(n.task?20:0),std::max(8.,(r.height()-text.size().height())/2));
            QAbstractTextDocumentLayout::PaintContext context; context.palette.setColor(QPalette::Text,style.text);
            text.documentLayout()->draw(&painter,context);
        }
        painter.restore();
    }
    return image;
}
