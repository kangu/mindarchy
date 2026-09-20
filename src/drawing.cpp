#include "appfont.h"
#include "drawing.h"
#include <QPainterPath>
#include <cmath>
#include <numbers>
namespace MapDrawing {
QStringList branchStyles() {
    return {"Rounded", "Angular", "Botanical graphite", "Living oak", "Sumi branch",
            "Silver birch", "Elven filigree"};
}
bool artisticBranch(const QString &style) {
    return style != "Rounded" && style != "Angular" && branchStyles().contains(style);
}
namespace {
QColor mix(QColor a, QColor b, qreal t) {
    return QColor::fromRgbF(a.redF()*(1-t)+b.redF()*t,a.greenF()*(1-t)+b.greenF()*t,
                           a.blueF()*(1-t)+b.blueF()*t);
}
QPointF unit(QPointF p) {
    const qreal length=std::hypot(p.x(),p.y());
    return length>.00001 ? p/length : QPointF(1,0);
}
QPointF perpendicular(QPointF p) { const auto u=unit(p); return {-u.y(),u.x()}; }
struct Ribbon { QPolygonF left, right; };
Ribbon ribbon(const BranchStroke &stroke) {
    Ribbon r;
    QPolygonF points;
    for(const auto &p:stroke.path)
        if(points.isEmpty() || QLineF(points.last(),p).length()>.00001) points<<p;
    if(points.size()<2) return r;
    for(int i=0;i<points.size();++i) {
        const auto before=perpendicular(points[i]-points[std::max(0,i-1)]);
        const auto after=perpendicular(points[std::min(i+1,int(points.size())-1)]-points[i]);
        auto n=i==0 ? after : i==points.size()-1 ? before :
            (before+after)/std::max(.4,1+QPointF::dotProduct(before,after));
        const qreal t=qreal(i)/(points.size()-1);
        const qreal width=stroke.startWidth*(1-t)+stroke.endWidth*t;
        r.left<<points[i]+n*width*.5; r.right<<points[i]-n*width*.5;
    }
    return r;
}
}
QVector<QPointF> branchTriangles(const BranchStroke &stroke) {
    const auto r=ribbon(stroke); QVector<QPointF> triangles;
    for(int i=1;i<r.left.size();++i)
        triangles<<r.left[i-1]<<r.right[i-1]<<r.left[i]
                 <<r.left[i]<<r.right[i-1]<<r.right[i];
    return triangles;
}
void paintBranches(QPainter &painter,const QVector<BranchStroke> &strokes) {
    painter.save(); painter.setPen(Qt::NoPen);
    for(const auto &stroke:strokes) {
        const auto r=ribbon(stroke); if(r.left.isEmpty()) continue;
        QPolygonF polygon=r.left;
        for(auto i=r.right.crbegin();i!=r.right.crend();++i) polygon<<*i;
        painter.setBrush(stroke.color); painter.drawPolygon(polygon);
    }
    painter.restore();
}
QVector<BranchStroke> branchGeometry(QPointF a,QPointF b,const QString &style,
    bool vertical,QColor tint,QColor background,qreal width,int depth,quint32 seed,qreal detail) {
    QVector<BranchStroke> strokes;
    const qreal length=QLineF(a,b).length();
    if(!artisticBranch(style) || width<=0 || tint.alpha()==0 || length<.01) return strokes;
    const bool dark=(.2126*background.redF()+.7152*background.greenF()+.0722*background.blueF())<.5;
    const bool oak=style=="Living oak", birch=style=="Silver birch", ink=style=="Sumi branch",
               elven=style=="Elven filigree";
    QColor outline,body,light,grain;
    if(oak) { outline=QColor(dark?"#ab825c":"#594735");body=QColor(dark?"#c49a70":"#947456");light=QColor(dark?"#ecd0a2":"#c7aa7b");grain=QColor(dark?"#795c42":"#65503b"); }
    else if(birch) { outline=QColor(dark?"#9aab9d":"#777a66");body=QColor(dark?"#d6ddc9":"#e2dfc6");light=QColor(dark?"#f0f2df":"#f8f3df");grain=QColor(dark?"#60776b":"#666954"); }
    else if(ink) { outline=QColor(dark?"#c3d4bd":"#384639");body=outline;light=QColor(dark?"#829e83":"#a1aa91");grain=outline; }
    else if(elven) { outline=QColor(dark?"#b3d4bd":"#46634f");body=outline;light=QColor(dark?"#e0ebd3":"#81977a");grain=outline; }
    else { outline=QColor(dark?"#c4c8b4":"#5a5a48");body=QColor(dark?"#929b85":"#a4a18a");light=QColor(dark?"#e0dfc8":"#dfdbc1");grain=outline; }
    outline=mix(outline,tint,.12);body=mix(body,tint,.08);
    const qreal thickness=std::min(length*.12,std::clamp(width*(elven?1.05:ink?3.2:birch?4.4:oak?4.8:3.3)/(1+.16*std::max(0,depth-1)),elven?.8:2.,elven?2.5:13.));
    const qreal tip=std::max(elven?.55:.7,thickness*(elven?.42:.23));
    const qreal phase=(seed%997)*.017;
    const QPointF delta=b-a;
    const QPointF c1=vertical ? a+QPointF(0,delta.y()*.44):a+QPointF(delta.x()*.44,0);
    const QPointF c2=vertical ? b-QPointF(0,delta.y()*.44):b-QPointF(delta.x()*.44,0);
    QPolygonF path; constexpr int count=64;
    for(int i=0;i<=count;++i) {
        const qreal t=qreal(i)/count,u=1-t;
        const QPointF p=a*(u*u*u)+c1*(3*u*u*t)+c2*(3*u*t*t)+b*(t*t*t);
        const QPointF tangent=(c1-a)*(3*u*u)+(c2-c1)*(6*u*t)+(b-c2)*(3*t*t);
        // Endpoint envelope leaves node attachment points exact; seed is stable as nodes move.
        const qreal wave=std::sin(t*std::numbers::pi)*std::sin(t*std::numbers::pi*3+phase);
        path<<p+perpendicular(tangent)*wave*std::min(length*.02,elven?2.:4.);
    }
    auto add=[&](QPolygonF p,QColor color,qreal first,qreal last) {
        color.setAlphaF(color.alphaF()*tint.alphaF());
        strokes.append({p,color,first,last});
    };
    auto sample=[&](qreal t,qreal offset=0.) {
        t=std::clamp(t,0.,1.); const qreal index=t*count; const int i=std::min(count-1,int(index));
        return path[i]+(path[i+1]-path[i])*(index-i)+perpendicular(path[i+1]-path[i])*offset;
    };
    add(path,outline,thickness,tip);
    if(!elven && !ink) {
        add(path,body,thickness*.77,tip*.5);
        QPolygonF highlight;
        for(int i=0;i<=count;++i) {const qreal t=qreal(i)/count; highlight<<sample(t,-(thickness*(1-t)+tip*t)*.16);}
        add(highlight,light,thickness*.21,tip*.18);
    }
    // Ornament never appears on short connectors; texture does not change the silhouette at LOD boundaries.
    if(detail<.48 || length<32) return strokes;
    if(elven) {
        const qreal t=.68, side=(seed%2)?1.:-1.;
        const auto origin=sample(t), tangent=unit(sample(t+.02)-sample(t-.02)), normal=perpendicular(tangent)*side;
        const qreal size=std::min(12.,length*.10);
        QPolygonF leaf,vein;
        for(int i=0;i<=32;++i) {
            const qreal u=qreal(i)/32,angle=u*2*std::numbers::pi;
            const qreal along=(1-std::cos(angle))*.5;
            leaf<<origin+tangent*(along*size*.65)+normal*(along*size+std::sin(angle)*size*.22);
        }
        add(leaf,outline,.65,.65);
        add({origin,origin+tangent*size*.65+normal*size},light,.6,.25);
        if(length>100) {
            const auto base=sample(.30); QPolygonF curl;
            for(int i=0;i<=40;++i) {
                const qreal u=qreal(i)/40,angle=u*std::numbers::pi*2.2,radius=size*.45*(1-u);
                curl<<base+tangent*(std::sin(angle)*radius)+normal*(size*.45-std::cos(angle)*radius);
            }
            add(curl,outline,.75,.3);
        }
    } else {
        const int fibers=ink?4:oak?5:3;
        for(int f=0;f<fibers;++f) {
            QPolygonF fiber;
            const qreal begin=.04+f*.035, end=.96-f*.055;
            for(int i=0;i<=40;++i) {
                const qreal t=begin+(end-begin)*i/40.;
                const qreal envelope=(thickness*(1-t)+tip*t);
                const qreal offset=envelope*((qreal(f)/(fibers-1)-.5)*.6+.055*std::sin(t*24+phase+f));
                fiber<<sample(t,offset);
            }
            add(fiber,ink?light:grain,ink?.42:.33,.18);
        }
        if(oak || birch) {
            const int marks=std::clamp(int(length/18),2,16);
            for(int i=0;i<marks;++i) {
                const qreal t=.12+.72*(i+.3*std::sin(phase+i))/(marks);
                const qreal local=thickness*(1-t)+tip*t;
                const auto center=sample(t), normal=perpendicular(sample(t+.01)-sample(t-.01));
                const auto tangent=unit(sample(t+.01)-sample(t-.01));
                const qreal side=i%2 ? 1.:-1.;
                add({center+normal*local*.34*side,center+normal*local*.02*side+tangent*(birch?1.5:3.),center-normal*local*.16*side+tangent*(birch?1.:5.)},grain,birch?.85:.5,.3);
            }
        }
        if(oak && length>90) {
            const qreal t=.73; const auto origin=sample(t);
            const auto tangent=unit(sample(t+.02)-sample(t-.02));const auto normal=perpendicular(tangent);
            QPolygonF bud;for(int i=0;i<=20;++i){const qreal u=qreal(i)/20;bud<<origin+tangent*(u*7)+normal*(std::sin(u*std::numbers::pi)*3+u*7);}
            add(bud,QColor(dark?"#bfd09a":"#748252"),2.7,.2);
        }
    }
    return strokes;
}
QPolygonF taskCheckPath(QRectF box,bool compact) {
    // A long rising finish and short rounded foot give the mark a clear silhouette.
    const QRectF overlay=compact ? box.adjusted(1,1,-1,-1) : box.adjusted(-4,-5,7,4);
    QPolygonF points;
    for(const QPointF p : {QPointF(.12,.49),QPointF(.37,.77),QPointF(.88,.15)})
        points << overlay.topLeft()+QPointF(p.x()*overlay.width(),p.y()*overlay.height());
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
void paintTask(QPainter &painter, QRectF box, QColor frame, bool checked, qreal progress, const TaskAppearance &task) {
    const auto completionColor=task.accent;
    if(progress>=0) {
        painter.save();
        frame.setAlphaF(frame.alphaF()*task.trackOpacity);
        painter.setPen(QPen(frame,task.progressWidth,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        painter.drawPolyline(taskProgressArc(box,1));
        painter.setPen(QPen(completionColor,task.progressWidth,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        if(progress>0) painter.drawPolyline(taskProgressArc(box,progress));
        if(progress>=1) {
            painter.setPen(QPen(completionColor,1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            painter.drawPolyline(taskCheckPath(box,true));
        }
        painter.restore(); return;
    }
    painter.save();
    if (checked) frame.setAlphaF(frame.alphaF() * task.completedFrameOpacity);
    painter.setPen(QPen(frame, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(box, task.cornerRadius, task.cornerRadius);
    if (checked) {
        painter.setPen(QPen(completionColor, task.checkWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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
    const qreal scale=Calendar::textScale(text,style.fontFamily,style.fontSize); painter.scale(scale,scale);
    const auto base=Calendar::textFont(text,style.fontFamily,style.fontSize);
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
        painter.setPen(assigned ? Themes::contrastInk(style.branch) : style.text);
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
