#include "canvas.h"
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QImageReader>

namespace {
bool acceptsImage(const QMimeData *mime) {
    if(mime->hasImage()) return true;
    const auto urls=mime->urls();
    if(urls.size()!=1 || !urls[0].isLocalFile()) return false;
    return QImageReader::supportedImageFormats().contains(QFileInfo(urls[0].toLocalFile()).suffix().toLower().toLatin1());
}
}
double MindCanvas::imageWidth(int id) const {
    return id==m_imageSelected && m_imageResizeHandle>=0 ? m_imageResizeWidth : (m_engine?m_engine->nodes().value(id).image.width:0);
}
double MindCanvas::imageInset(int id) const {
    return m_engine?m_engine->nodes().value(id).image.inset(imageWidth(id)):0;
}
QRectF MindCanvas::contentRect(int id,QRectF rect) const {
    if(!m_engine) return rect;
    const auto node=m_engine->nodes().value(id);
    if(!node.image.empty()) return node.image.contentRect(rect,id==m_editingId?m_editContentSize:m_engine->contentSize(id),imageWidth(id));
    if(node.kind=="date") { const auto size=Calendar::size(node.calendar); rect.setTop(rect.center().y()-size.height()/2); rect.setSize(size); }
    return rect;
}
QRectF MindCanvas::imageWorldRect(int id) const {
    return m_engine?m_engine->nodes().value(id).image.rect(displayRect(id),imageWidth(id)):QRectF();
}
QRectF MindCanvas::imageSelectionRect() const {
    if(m_imageSelected<0 || !m_engine || !m_engine->selectedIds().contains(m_imageSelected) || editing() || m_dragging) return {};
    const auto r=imageWorldRect(m_imageSelected);
    return r.isEmpty()?QRectF():QRectF(mapFromWorld(r.topLeft()),r.size()*m_zoom);
}
QRectF MindCanvas::imageDropRect() const {
    if(m_imageDrop<0) return {};
    const auto r=displayRect(m_imageDrop);
    return {mapFromWorld(r.topLeft()),r.size()*m_zoom};
}
int MindCanvas::imageHandleHit(QPointF p) const {
    const auto r=imageSelectionRect(); if(r.isEmpty()) return -1;
    const QPointF points[]={r.topLeft(),{r.center().x(),r.top()},r.topRight(),{r.right(),r.center().y()},r.bottomRight(),{r.center().x(),r.bottom()},r.bottomLeft(),{r.left(),r.center().y()}};
    for(int i=0;i<8;++i) if(QRectF(points[i]-QPointF(6,6),QSizeF(12,12)).contains(p)) return i;
    return -1;
}
void MindCanvas::cancelImageResize() {
    if(m_imageResizeHandle<0) return;
    m_imageResizeHandle=-1; m_imageResizeWidth=0; refresh(); emit viewChanged();
}
void MindCanvas::dragEnterEvent(QDragEnterEvent *event) {
    if(acceptsImage(event->mimeData())) { event->setDropAction(Qt::CopyAction); event->accept(); }
    else event->ignore();
}
void MindCanvas::dragMoveEvent(QDragMoveEvent *event) {
    m_imageDrop=acceptsImage(event->mimeData()) && boundingRect().contains(event->position()) ? hit(event->position()):-1;
    emit viewChanged();
    if(m_imageDrop>=0) { event->setDropAction(Qt::CopyAction); event->accept(); } else event->ignore();
}
void MindCanvas::dragLeaveEvent(QDragLeaveEvent *event) { m_imageDrop=-1; emit viewChanged(); event->accept(); }
void MindCanvas::dropEvent(QDropEvent *event) {
    const int id=boundingRect().contains(event->position())?hit(event->position()):-1;
    m_imageDrop=-1; emit viewChanged();
    if(id<0 || !acceptsImage(event->mimeData())) { event->ignore(); return; }
    if(editing()) emit commitRequested();
    if(editing()) { event->ignore(); return; }
    bool success=false;
    if(event->mimeData()->urls().size()==1 && event->mimeData()->urls().first().isLocalFile()) success=m_engine->importImage(id,event->mimeData()->urls().first().toLocalFile());
    else { NodeImage image; success=NodeImage::importPixels(qvariant_cast<QImage>(event->mimeData()->imageData()),image) && m_engine->setImage(id,image); }
    if(success) {
        m_engine->select(id); m_imageSelected=id; forceActiveFocus();
        event->setDropAction(Qt::CopyAction); event->accept(); refresh();
    } else event->ignore();
    emit viewChanged();
}
