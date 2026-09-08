#include "drawing.h"
#include <QPainterPath>
#include <cmath>
#include <numbers>
namespace MapDrawing {
QPolygonF edgePath(QPointF a, QPointF b, bool angular, bool vertical) {
    QPolygonF path; path << a;
    QPointF c1=vertical ? QPointF(a.x(),(a.y()+b.y())/2) : QPointF((a.x()+b.x())/2,a.y());
    QPointF c2=vertical ? QPointF(b.x(),c1.y()) : QPointF(c1.x(),b.y());
    if(angular) path << c1 << c2 << b;
    else for(int j=1;j<=24;++j) { qreal t=j/24.,u=1-t; path << u*u*u*a+3*u*u*t*c1+3*u*t*t*c2+t*t*t*b; }
    return path;
}
QPolygonF shapePolygon(QRectF r, NodeShape shape, qreal radius) {
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
        for (int i=0;i<160;++i) {
            const qreal a=i*2*std::numbers::pi/160.;
            const qreal x=std::cos(a), y=std::sin(a);
            const qreal distance=std::min(halfW/std::max(.00001,std::abs(x)),
                                         halfH/std::max(.00001,std::abs(y)));
            const qreal wave=1.8*std::sin(i*2*std::numbers::pi/8.);
            polygon << r.center()+QPointF(x,y)*(distance+wave);
        }
    } else {
        QPainterPath path;
        if (shape == NodeShape::Rectangle) path.addRect(r);
        else path.addRoundedRect(r, shape==NodeShape::Pill ? r.height()/2 : radius,
                                shape==NodeShape::Pill ? r.height()/2 : radius);
        polygon=path.toFillPolygon();
    }
    return polygon;
}
void paintCalendar(QPainter &painter,const CalendarData &data,const NodeAppearance &style) {
    painter.setRenderHint(QPainter::Antialiasing); painter.setRenderHint(QPainter::TextAntialiasing);
    QFont font("sans-serif"); font.setPixelSize(11); font.setBold(true); painter.setFont(font); painter.setPen(style.text);
    painter.drawText(QRectF(39,10,216,28),Qt::AlignCenter,Calendar::title(data));
    font.setPixelSize(18); painter.setFont(font);
    painter.drawText(Calendar::previous(),Qt::AlignCenter,QStringLiteral("‹"));
    painter.drawText(Calendar::next(),Qt::AlignCenter,QStringLiteral("›"));
    font.setPixelSize(10); font.setBold(false); painter.setFont(font);
    const QStringList weekdays{"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
    for(int i=0;i<7;++i) painter.drawText(QRectF(14+i*38,44,34,20),Qt::AlignCenter,weekdays[i]);
    const auto days=Calendar::days(data);
    for(int i=0;i<days.size();++i) {
        const auto day=days[i]; if(!day.isValid()) continue;
        const auto cell=Calendar::cell(i); const bool assigned=data.entries.contains(day.toString(Qt::ISODate));
        const bool today=day==QDate::currentDate();
        painter.setPen(today ? QPen(style.branch,1.5) : QPen(Qt::NoPen));
        painter.setBrush(assigned ? QBrush(style.branch) : QBrush(Qt::NoBrush));
        if(assigned || today) painter.drawRoundedRect(cell,5,5);
        const double luminance=.2126*style.branch.redF()+.7152*style.branch.greenF()+.0722*style.branch.blueF();
        painter.setPen(assigned ? QColor(luminance>.55 ? "#172129" : "#ffffff") : style.text);
        font.setPixelSize(12); font.setBold(assigned || today); painter.setFont(font);
        painter.drawText(cell,Qt::AlignCenter,QString::number(day.day()));
    }
    const auto sums=Calendar::totals(data);
    if(sums.enabled) {
        painter.setPen(style.text); font.setPixelSize(11); font.setBold(true); painter.setFont(font);
        painter.drawText(QRectF(294,44,108,20),Qt::AlignRight|Qt::AlignVCenter,QStringLiteral("Sum"));
        for(int row=0;row<sums.weeks.size();++row)
            painter.drawText(Calendar::sumCell(row),Qt::AlignRight|Qt::AlignVCenter,Calendar::totalText(sums.weeks[row]));
        if(data.view=="month") {
            const auto total=Calendar::sumCell(sums.weeks.size()).translated(0,4);
            painter.setPen(QPen(style.branch,1)); painter.drawLine(QPointF(294,total.top()),QPointF(402,total.top()));
            painter.setPen(style.text);
            painter.drawText(QRectF(14,total.y(),265,total.height()),Qt::AlignRight|Qt::AlignVCenter,QStringLiteral("Month total"));
            painter.drawText(total,Qt::AlignRight|Qt::AlignVCenter,Calendar::totalText(sums.month));
        }
    }
}
}
