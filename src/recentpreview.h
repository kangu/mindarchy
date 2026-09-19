#pragma once
#include "engine.h"
#include "preview.h"
#include <QQuickImageProvider>
#include <QPainter>

// Qt loads these on its image worker; six file-backed snapshots never block QML.
class RecentPreviewProvider : public QQuickImageProvider {
public:
    RecentPreviewProvider() : QQuickImageProvider(Image,ForceAsynchronousImageLoading) {}
    QImage requestImage(const QString &id,QSize *size,const QSize &requested) override {
        const auto path=QString::fromUtf8(QByteArray::fromBase64(id.section('/',0,0).toLatin1(),QByteArray::Base64UrlEncoding));
        Engine map(nullptr,Engine::InitialContent::Blank);
        QImage result;
        if(map.open(path)) {
            const auto bounds=requested.isValid()?requested.boundedTo(QSize(1000,700)).expandedTo(QSize(32,32)):QSize(900,550);
            const auto snapshot=renderMapPreview(map,bounds);
            result=QImage(bounds,QImage::Format_ARGB32_Premultiplied); result.fill(map.canvasColor());
            QPainter painter(&result); painter.drawImage(QPoint((bounds.width()-snapshot.width())/2,(bounds.height()-snapshot.height())/2),snapshot);
        }
        if(size) *size=result.size();
        return result;
    }
};
