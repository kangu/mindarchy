#pragma once
#include "calendar.h"
#include "theme.h"
#include <QPainter>
#include <QPolygonF>
namespace MapDrawing {
inline const QColor taskCheckColor{"#36b879"};
inline constexpr qreal taskCheckWidth = 2.6;
inline constexpr qreal completedTaskFrameOpacity = .5;
QPolygonF taskCheckPath(QRectF box);
void paintTask(QPainter &painter, QRectF box, QColor frame, bool checked);
QPolygonF edgePath(QPointF a, QPointF b, bool angular, bool vertical);
QPolygonF shapePolygon(QRectF rect, NodeShape shape, qreal radius);
void paintCalendar(QPainter &painter, const CalendarData &data, const NodeAppearance &style);
}
