#pragma once
#include "calendar.h"
#include "theme.h"
#include <QPainter>
#include <QPolygonF>
namespace MapDrawing {
QPolygonF taskCheckPath(QRectF box, bool compact = false);
void paintTask(QPainter &painter, QRectF box, QColor frame, bool checked, qreal progress = -1, const TaskAppearance &task = Themes::taskAppearance({},Qt::white));
QPolygonF taskProgressArc(QRectF box, qreal progress);
QPolygonF edgePath(QPointF a, QPointF b, bool angular, bool vertical, qreal detail = 1);
QPolygonF shapePolygon(QRectF rect, NodeShape shape, qreal radius, qreal detail = 1);
QVector<QPointF> strokeTriangles(const QPolygonF &path, qreal width);
void paintCalendar(QPainter &painter, const CalendarData &data, const NodeAppearance &style, const QString &text = {});
}
