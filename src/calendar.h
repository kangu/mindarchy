#pragma once
#include <QDate>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlockFormat>
#include "appfont.h"
#include <QMap>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>
#include <QRegularExpression>
#include <cmath>

struct CalendarData {
    QString view = "week";
    QDate anchor;
    QMap<QString,QString> entries;
};
namespace Calendar {
inline QFont textFont(const QString &text, const QString &family = {}) {
    QFont base(family.isEmpty() ? mindarchyTextFamily() : family); base.setPixelSize(15);
    QTextDocument doc; doc.setDefaultFont(base); doc.setHtml(text);
    QTextCursor cursor(&doc); cursor.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor);
    return cursor.charFormat().font().resolve(base);
}
inline qreal textScale(const QString &text, const QString &family = {}) {
    const auto font=textFont(text,family);
    return (font.pixelSize()>0 ? font.pixelSize() : font.pointSizeF()*96./72.)/15.;
}
inline Qt::Alignment textAlignment(const QString &text) {
    QTextDocument doc; doc.setHtml(text); QTextCursor cursor(&doc);
    const auto format=cursor.blockFormat();
    return format.hasProperty(QTextFormat::BlockAlignment) ? format.alignment() : Qt::AlignHCenter;
}

inline QDate start(const CalendarData &data) {
    return data.view=="month" ? QDate(data.anchor.year(),data.anchor.month(),1)
                              : data.anchor.addDays(1-data.anchor.dayOfWeek());
}
inline QVector<QDate> days(const CalendarData &data) {
    const auto first=start(data);
    if(!first.isValid()) return {};
    if(data.view=="week") {
        QVector<QDate> result; for(int i=0;i<7;++i) result.append(first.addDays(i)); return result;
    }
    const int offset=first.dayOfWeek()-1, count=((offset+first.daysInMonth()+6)/7)*7;
    QVector<QDate> result(count);
    for(int i=0;i<first.daysInMonth();++i) result[offset+i]=first.addDays(i);
    return result;
}
struct Totals {
    bool enabled=false;
    QVector<double> weeks;
    double month=0;
};
inline bool numericValue(QString text, double &value) {
    static const QRegularExpression number(QStringLiteral("^[+-]?(?:[0-9]+(?:[.,][0-9]+)?|[.,][0-9]+)$"));
    text=text.trimmed();
    if(!number.match(text).hasMatch()) return false;
    text.replace(',', '.'); bool ok=false; value=text.toDouble(&ok);
    return ok && std::isfinite(value);
}
inline Totals totals(const CalendarData &data) {
    const auto visible=days(data); Totals result; result.weeks.resize(visible.size()/7);
    for(int i=0;i<visible.size();++i) {
        double value=0;
        if(visible[i].isValid() && numericValue(data.entries.value(visible[i].toString(Qt::ISODate)),value)) {
            result.enabled=true; result.weeks[i/7]+=value; result.month+=value;
        }
    }
    return result;
}
inline QString totalText(double value) {
    if(!std::isfinite(value)) return QStringLiteral("Overflow");
    return QString::number(value==0 ? 0 : value,'g',12);
}
inline qreal weekGutter(const CalendarData &data) { return data.view=="month" ? 32. : 0.; }
inline int weekNumber(const CalendarData &data, int row) {
    const auto first=start(data);
    return first.addDays(1-first.dayOfWeek()+row*7).weekNumber();
}
inline QRectF weekCell(int row) { return {8,70.+row*32.,30,28}; }
inline QRectF sumCell(int row, qreal offset=0) { return {294+offset,70.+row*32.,108,28}; }
inline QSizeF size(const CalendarData &data) {
    const auto sums=totals(data);
    return {(sums.enabled ? 414. : 294.)+weekGutter(data),82.+(days(data).size()/7)*32.+(sums.enabled && data.view=="month" ? 36. : 0.)};
}
inline QRectF cell(int index, qreal offset=0) { return {14.+offset+(index%7)*38.,70.+(index/7)*32.,34.,28.}; }
inline QString title(const CalendarData &data) {
    const auto first=start(data);
    if(data.view=="month") return first.toString("MMMM yyyy");
    const auto last=first.addDays(6);
    return first.toString("d MMM")+QStringLiteral(" – ")+last.toString("d MMM yyyy");
}
inline QString key(const CalendarData &data) {
    QString result=data.view+data.anchor.toString(Qt::ISODate)+QDate::currentDate().toString(Qt::ISODate);
    for(const auto &date:days(data)) result+=data.entries.contains(date.toString(Qt::ISODate)) ? '1' : '0';
    const auto sums=totals(data);
    if(sums.enabled) {
        for(double sum:sums.weeks) result+='|'+totalText(sum);
        result+='|'+totalText(sums.month);
    }
    return result;
}
}
