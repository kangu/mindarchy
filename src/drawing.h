#pragma once
#include "calendar.h"
#include "theme.h"
#include <QPainter>
#include <QPolygonF>
namespace MapDrawing {
// Shared vector strokes for GPU, software canvas and document previews.
struct BranchStroke {
    QPolygonF path;
    QColor color;
    qreal startWidth = 1, endWidth = 1;
};
QStringList branchStyles();
bool artisticBranch(const QString &style);
QVector<BranchStroke> branchGeometry(QPointF a, QPointF b, const QString &style,
    bool vertical, QColor tint, QColor background, qreal width, int depth, quint32 seed,
    qreal detail = 1);
QVector<QPointF> branchTriangles(const BranchStroke &stroke);
void paintBranches(QPainter &painter, const QVector<BranchStroke> &strokes);

QPolygonF taskCheckPath(QRectF box, bool compact = false);
void paintTask(QPainter &painter, QRectF box, QColor frame, bool checked, qreal progress = -1, const TaskAppearance &task = Themes::taskAppearance({},Qt::white));
QPolygonF taskProgressArc(QRectF box, qreal progress);
QPolygonF edgePath(QPointF a, QPointF b, bool angular, bool vertical, qreal detail = 1);
QPolygonF shapePolygon(QRectF rect, NodeShape shape, qreal radius, qreal detail = 1);
QVector<QPointF> strokeTriangles(const QPolygonF &path, qreal width);
void paintCalendar(QPainter &painter, const CalendarData &data, const NodeAppearance &style, const QString &text = {});
}
