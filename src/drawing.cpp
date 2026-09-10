#include "appfont.h"
#include "drawing.h"
#include <QPainterPath>
#include <cmath>
#include <numbers>
namespace MapDrawing {
QPolygonF taskCheckPath(QRectF box) {
    // Oversize the tick while keeping the checkbox and text positions stable.
    const QRectF overlay = box.adjusted(-7, -8, 7, 6);
    QPolygonF points;
    for (const QPointF p : {QPointF(3.74, 7.85), QPointF(7.32, 11.5), QPointF(13.26, 4.5)})
        points << overlay.topLeft() + QPointF(p.x()*overlay.width()/16, p.y()*overlay.height()/16);
    return points;
}
QPolygonF taskProgressArc(QRectF box, qreal progress) {
    QPolygonF points;
    const qreal radius=box.width()*.7;
    const int steps=std::max(1,int(std::ceil(64*progress)));
    for(int i=0;i<=steps;++i) {
        const qreal angle=-std::numbers::pi/2+2*std::numbers::pi*progress*i/steps;
        points.append(box.center()+QPointF(std::cos(angle),std::sin(angle))*radius);
    }
    return points;
}
void paintTask(QPainter &painter, QRectF box, QColor frame, bool checked, qreal progress) {
    if(progress>=0) {
        painter.save();
        frame.setAlphaF(frame.alphaF()*.25);
        painter.setPen(QPen(frame,2.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        painter.drawPolyline(taskProgressArc(box,1));
        painter.setPen(QPen(taskCheckColor,2.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        if(progress>0) painter.drawPolyline(taskProgressArc(box,progress));
        painter.restore(); return;
    }
    painter.save();
    if (checked) frame.setAlphaF(frame.alphaF() * completedTaskFrameOpacity);
    painter.setPen(QPen(frame, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(box, 2, 2);
    if (checked) {
        painter.setPen(QPen(taskCheckColor, taskCheckWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(taskCheckPath(box));
    }
    painter.restore();
}
QVector<QPointF> strokeTriangles(const QPolygonF &path, qreal width) {
    QPolygonF points;
    for (const auto &p : path)
        if (points.isEmpty() || QLineF(points.last(), p).length() > .00001) points << p;
    const bool closed = points.size()>2 && points.first()==points.last();
    if (closed) points.removeLast();
    if (points.size()<2 || width<=0) return {};
    auto normal = [](QPointF d) { return QPointF(-d.y(), d.x()) / std::hypot(d.x(), d.y()); };
    QVector<QPointF> left, right, triangles;
    for (int i=0; i<points.size(); ++i) {
        const auto before = normal(points[i] - points[(i+points.size()-1)%points.size()]);
        const auto after = normal(points[(i+1)%points.size()] - points[i]);
        QPointF offset;
        if (!closed && i==0) offset=after;
        else if (!closed && i==points.size()-1) offset=before;
        else offset=(before+after)/std::max(.25, 1.+QPointF::dotProduct(before,after));
        offset *= width/2;
        left << points[i]+offset; right << points[i]-offset;
    }
    // Adjacent segments share identical edge vertices: no butt-cap wedges.
    const int count=closed ? points.size() : points.size()-1;
    for (int i=0; i<count; ++i) {
        const int j=(i+1)%points.size();
        triangles << left[i] << right[i] << left[j] << left[j] << right[i] << right[j];
    }
    return triangles;
}
QPolygonF edgePath(QPointF a, QPointF b, bool angular, bool vertical, qreal detail) {
    QPolygonF path; path << a;
    QPointF c1=vertical ? QPointF(a.x(),(a.y()+b.y())/2) : QPointF((a.x()+b.x())/2,a.y());
    QPointF c2=vertical ? QPointF(b.x(),c1.y()) : QPointF(c1.x(),b.y());
    if(angular) path << c1 << c2 << b;
    else {
        QPainterPath curve(a); curve.cubicTo(c1,c2,b);
        const qreal scale=std::clamp(detail, .001, 64.);
        const QTransform transform=QTransform::fromScale(scale,scale);
        const auto polygons=curve.toSubpathPolygons(transform);
        if (!polygons.isEmpty()) path=transform.inverted().map(polygons.first());
    }
    return path;
}
QPolygonF shapePolygon(QRectF r, NodeShape shape, qreal radius, qreal detail) {
    QPolygonF polygon;
    if (shape == NodeShape::Hexagon) {
        qreal inset = std::min(10., r.width() / 5);
        polygon << QPointF(r.left()+inset,r.top()) << QPointF(r.right()-inset,r.top())
                << QPointF(r.right(),r.center().y()) << QPointF(r.right()-inset,r.bottom())
                << QPointF(r.left()+inset,r.bottom()) << QPointF(r.left(),r.center().y());
    } else if (shape == NodeShape::Octagon) {
        const qreal d=std::min({10.,r.width()/4,r.height()/4});
        polygon << QPointF(r.left()+d,r.top()) << QPointF(r.right()-d,r.top())
                << QPointF(r.right(),r.top()+d) << QPointF(r.right(),r.bottom()-d)
                << QPointF(r.right()-d,r.bottom()) << QPointF(r.left()+d,r.bottom())
                << QPointF(r.left(),r.bottom()-d) << QPointF(r.left(),r.top()+d);
    } else if (shape == NodeShape::Scalloped) {
        const qreal halfW=r.width()/2, halfH=r.height()/2;
        const int segments=160*std::clamp(int(std::ceil(std::sqrt(std::max(1.,detail)))),1,8);
        for (int i=0;i<segments;++i) {
            const qreal a=i*2*std::numbers::pi/segments;
            const qreal x=std::cos(a), y=std::sin(a);
            const qreal distance=std::min(halfW/std::max(.00001,std::abs(x)),
                                         halfH/std::max(.00001,std::abs(y)));
            const qreal wave=1.8*std::sin(a*20);
            polygon << r.center()+QPointF(x,y)*(distance+wave);
        }
    } else {
        QPainterPath path;
        if (shape == NodeShape::Rectangle) path.addRect(r);
        else path.addRoundedRect(r, shape==NodeShape::Pill ? r.height()/2 : radius,
                                shape==NodeShape::Pill ? r.height()/2 : radius);
        const qreal scale=std::clamp(detail, .001, 64.);
        const QTransform transform=QTransform::fromScale(scale,scale);
        polygon=transform.inverted().map(path.toFillPolygon(transform));
    }
    return polygon;
}
void paintCalendar(QPainter &painter,const CalendarData &data,const NodeAppearance &style, const QString &text) {
    painter.save();
    const qreal scale=Calendar::textScale(text); painter.scale(scale,scale);
    const auto base=Calendar::textFont(text);
    const auto alignment=Calendar::textAlignment(text)|Qt::AlignVCenter;
    const qreal gutter=Calendar::weekGutter(data);
    painter.setRenderHint(QPainter::Antialiasing); painter.setRenderHint(QPainter::TextAntialiasing);
    QFont font=base; font.setPixelSize(11); font.setBold(true); painter.setFont(font); painter.setPen(style.text);
    painter.drawText(QRectF(10+gutter,10,274,28),Qt::AlignCenter,Calendar::title(data));
    font.setPixelSize(10); font.setBold(base.bold()); painter.setFont(font);
    const QStringList weekdays{"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
    for(int i=0;i<7;++i) painter.drawText(QRectF(14+gutter+i*38,44,34,20),Qt::AlignCenter,weekdays[i]);
    const auto days=Calendar::days(data);
    for(int i=0;i<days.size();++i) {
        const auto day=days[i]; if(!day.isValid()) continue;
        const auto cell=Calendar::cell(i,gutter); const bool assigned=data.entries.contains(day.toString(Qt::ISODate));
        const bool today=day==QDate::currentDate();
        painter.setPen(today && !assigned ? QPen(style.branch,1.5) : QPen(Qt::NoPen));
        painter.setBrush(assigned ? QBrush(style.branch) : QBrush(Qt::NoBrush));
        if(assigned) painter.drawRoundedRect(cell,5,5);
        else if(today) painter.drawRoundedRect(cell.adjusted(.75,.75,-.75,-.75),4.25,4.25);
        const double luminance=.2126*style.branch.redF()+.7152*style.branch.greenF()+.0722*style.branch.blueF();
        painter.setPen(assigned ? QColor(luminance>.55 ? "#172129" : "#ffffff") : style.text);
        font.setPixelSize(12); font.setBold(base.bold() || assigned || today); painter.setFont(font);
        painter.drawText(cell,alignment,QString::number(day.day()));
    }
    if(data.view=="month") {
        QColor muted=style.text; muted.setAlphaF(muted.alphaF()*.55);
        painter.setPen(muted); font.setPixelSize(8); font.setBold(base.bold()); painter.setFont(font);
        for(int row=0;row<days.size()/7;++row)
            painter.drawText(Calendar::weekCell(row),Qt::AlignCenter,
                             QString::number(Calendar::weekNumber(data,row)));
    }
    const auto sums=Calendar::totals(data);
    if(sums.enabled) {
        painter.setPen(style.text); font.setPixelSize(11); font.setBold(true); painter.setFont(font);
        painter.drawText(QRectF(294+gutter,44,108,20),Qt::AlignRight|Qt::AlignVCenter,QStringLiteral("Sum"));
        for(int row=0;row<sums.weeks.size();++row)
            painter.drawText(Calendar::sumCell(row,gutter),Qt::AlignRight|Qt::AlignVCenter,Calendar::totalText(sums.weeks[row]));
        if(data.view=="month") {
            const auto total=Calendar::sumCell(sums.weeks.size(),gutter).translated(0,4);
            painter.setPen(QPen(style.branch,1)); painter.drawLine(QPointF(294+gutter,total.top()),QPointF(402+gutter,total.top()));
            painter.setPen(style.text);
            painter.drawText(QRectF(14,total.y(),265+gutter,total.height()),Qt::AlignRight|Qt::AlignVCenter,QStringLiteral("Month total"));
            painter.drawText(total,Qt::AlignRight|Qt::AlignVCenter,Calendar::totalText(sums.month));
        }
    }
    painter.restore();
}
}
