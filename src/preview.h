#pragma once
#include <QImage>
class Engine;
// Bounded, display-independent raster of the saved map; no editor UI or selection.
QImage renderMapPreview(const Engine &engine, QSize maximum = QSize(1600, 1000));
