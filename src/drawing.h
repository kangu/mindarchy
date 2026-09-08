#pragma once
#include "calendar.h"
#include "theme.h"
#include <QPainter>
#include <QPolygonF>
namespace MapDrawing {
QPolygonF edgePath(QPointF a, QPointF b, bool angular, bool vertical);
QPolygonF shapePolygon(QRectF rect, NodeShape shape, qreal radius);
void paintCalendar(QPainter &painter, const CalendarData &data, const NodeAppearance &style);
}
