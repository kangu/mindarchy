#pragma once
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QJsonObject>
#include <QRectF>
#include <QUrl>
#include <algorithm>
#include <cmath>

// Immutable, implicitly shared pixels/bytes keep undo snapshots inexpensive.
struct NodeImage {
    QByteArray data;
    QImage pixels;
    double width = 0;
    QString placement = "left";
    static bool validPlacement(const QString &p) { return p=="left" || p=="right" || p=="top" || p=="bottom"; }
    bool empty() const { return pixels.isNull(); }
    QSizeF size(double overrideWidth = 0) const {
        if(empty()) return {};
        const double w=overrideWidth>0 ? overrideWidth : width;
        return {w,w*pixels.height()/pixels.width()};
    }
    double inset(double overrideWidth = 0) const { return empty()?0:size(overrideWidth).width()+12; }
    QSizeF expanded(QSizeF content, double overrideWidth = 0) const {
        if(empty()) return content;
        const auto s=size(overrideWidth);
        if(placement=="top" || placement=="bottom") return {std::max(content.width(),s.width()+20),content.height()+s.height()+12};
        return {content.width()+s.width()+12,std::max(content.height(),s.height()+20)};
    }
    QRectF rect(QRectF node, double overrideWidth = 0) const {
        if(empty()) return {};
        const auto s=size(overrideWidth);
        if(placement=="top") return {{node.center().x()-s.width()/2,node.top()+10},s};
        if(placement=="bottom") return {{node.center().x()-s.width()/2,node.bottom()-10-s.height()},s};
        return {{placement=="right"?node.right()-10-s.width():node.left()+10,node.center().y()-s.height()/2},s};
    }
    QRectF contentRect(QRectF node, QSizeF base, double overrideWidth = 0) const {
        if(empty()) return node;
        const auto s=size(overrideWidth);
        if(placement=="top" || placement=="bottom")
            return {{node.center().x()-base.width()/2,placement=="top"?node.top()+s.height()+12:node.top()},base};
        return {{placement=="left"?node.left()+s.width()+12:node.left(),node.center().y()-base.height()/2},base};
    }
    static constexpr qint64 SourceLimit=64*1024*1024, PixelLimit=64*1024*1024;
    static constexpr qint64 StoredLimit=8*1024*1024, DocumentImageLimit=12*1024*1024, DecodedLimit=128*1024*1024;
    static double defaultWidth(QSize pixels) { if(!pixels.isValid()) return 0; return pixels.width()*std::min(1.,120./std::max(pixels.width(),pixels.height())); }
    double boundedWidth(double proposed) const {
        if(empty() || !std::isfinite(proposed)) return width;
        const double ratio=double(pixels.width())/std::max(pixels.width(),pixels.height());
        return std::clamp(proposed,24*ratio,1024*ratio);
    }
    QJsonObject json() const { return {{"data",QString::fromLatin1(data.toBase64())},{"width",width},{"placement",placement}}; }
    QString source() const { return empty()?QString():QString("data:image/%1;base64,%2").arg(data.startsWith("\xff\xd8")?"jpeg":"png",QString::fromLatin1(data.toBase64())); }
    static QImage read(const QByteArray &bytes, bool importing) {
        if(bytes.isEmpty() || bytes.size()>(importing?SourceLimit:StoredLimit)) return {};
        QBuffer buffer; buffer.setData(bytes); buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer); reader.setAutoTransform(true);
        const auto s=reader.size();
        if(!s.isValid() || qint64(s.width())*s.height()>PixelLimit) return {};
        if(!importing && (s.width()>2048 || s.height()>2048 || (reader.format()!="png" && reader.format()!="jpeg"))) return {};
        if(importing && (s.width()>2048 || s.height()>2048)) reader.setScaledSize(s.scaled(2048,2048,Qt::KeepAspectRatio));
        auto image=reader.read();
        if(image.width()>2048 || image.height()>2048) image=image.scaled(2048,2048,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        return image;
    }
    static bool fromJson(const QJsonValue &value, NodeImage &out) {
        if(!value.isObject()) return false;
        const auto obj=value.toObject();
        if(!obj["data"].isString() || obj["data"].toString().size()>StoredLimit*4/3+4 || !obj["width"].isDouble()) return false;
        auto decoded=QByteArray::fromBase64Encoding(obj["data"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
        if(!decoded) return false;
        NodeImage next; next.placement=obj.value("placement").toString("left");
        if(!validPlacement(next.placement) || (obj.contains("placement") && !obj["placement"].isString())) return false;
        next.data=decoded.decoded; next.pixels=read(next.data,false); next.width=obj["width"].toDouble();
        if(next.empty() || !std::isfinite(next.width) || next.width<=0 || next.width>next.boundedWidth(1e9)+.001) return false;
        out=next; return true;
    }
    static bool importPixels(QImage image, NodeImage &out) {
        if(image.isNull() || qint64(image.width())*image.height()>PixelLimit) return false;
        if(image.width()>2048 || image.height()>2048) image=image.scaled(2048,2048,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        QByteArray png; QBuffer p(&png); p.open(QIODevice::WriteOnly); if(!image.save(&p,"PNG")) return false;
        QByteArray best=png;
        // PNG retains sharp line art; JPEG is chosen only for substantial savings.
        bool transparent=false;
        if(image.hasAlphaChannel()) {
            auto rgba=image.convertToFormat(QImage::Format_ARGB32);
            for(int y=0;y<rgba.height() && !transparent;++y) {
                const auto row=reinterpret_cast<const QRgb*>(rgba.constScanLine(y));
                for(int x=0;x<rgba.width();++x) if(qAlpha(row[x])<255) { transparent=true; break; }
            }
        }
        if(!transparent) {
            QByteArray jpg; QBuffer j(&jpg); j.open(QIODevice::WriteOnly);
            if(image.save(&j,"JPEG",88) && jpg.size()<png.size()*.8) best=jpg;
        }
        if(best.size()>StoredLimit) return false;
        NodeImage next; next.data=best; next.pixels=read(best,false); next.width=defaultWidth(next.pixels.size());
        if(next.empty()) return false;
        out=next; return true;
    }
    static bool importFile(QString path, NodeImage &out) {
        if(path.startsWith("file:")) path=QUrl(path).toLocalFile();
        QFile file(path); if(!file.open(QIODevice::ReadOnly) || file.size()>SourceLimit) return false;
        return importPixels(read(file.readAll(),true),out);
    }
};
