// SPDX-License-Identifier: GPL-3.0-only
#ifndef UBSHAPESVG_H
#define UBSHAPESVG_H
#include "UBShapeGeometry.h"
#include <QDomDocument>
#include <QCryptographicHash>
#include <QPainterPathStroker>
#include <QRegularExpression>
namespace UBShapeGeometry {
// Bake page-width outlines into ordinary SVG so OpenBoard and PDF renderers
// preserve thickness after free resizing, without vector-effect extensions.
inline QByteArray uniformSvg(const QByteArray &data,const QTransform &matrix)
{
    QDomDocument doc;
    if (!doc.setContent(data)) return data;
    auto root = doc.documentElement();
    auto groups = doc.elementsByTagName("g");
    if (groups.isEmpty()) return data;
    auto group = groups.at(0).toElement();
    auto shape = group.firstChildElement();
    if (shape.isNull()) return data;
    // Work in page coordinates, excluding translation. Zoom remains a view
    // transform, so an outline still zooms naturally with the rest of the page.
    QTransform linear(matrix.m11(), matrix.m12(), matrix.m21(), matrix.m22(), 0, 0);
    bool invertible = false;
    const auto inverse = linear.inverted(&invertible);
    if (!invertible) return data;
    QStringList key {shape.tagName(), group.attribute("stroke"), group.attribute("stroke-width", "3"), group.attribute("stroke-dasharray", "none")};
    for (const auto &name : {"x", "y", "width", "height", "cx", "cy", "r", "rx", "ry", "points"}) key << shape.attribute(name);
    key << QString::number(root.attribute("data-shape-sides").toInt());
    for (auto v : {matrix.m11(), matrix.m12(), matrix.m21(), matrix.m22()}) key << QString::number(v, 'g', 6);
    const QString fingerprint = QString::fromLatin1(QCryptographicHash::hash(key.join('|').toUtf8(), QCryptographicHash::Sha256).toHex());
    if (root.attribute("data-lessonboard-outline-key") == fingerprint) return data;
    QPainterPath path,hidden,marker;
    const auto metadata=doc.elementsByTagName("metadata");
    const int shapeType=metadata.isEmpty()?-1:metadata.at(0).toElement().text().toInt();
    if(shapeType>=10) {
        auto bounds=root.attribute("viewBox").split(QRegularExpression("[ ,]+"),Qt::SkipEmptyParts);
        if(bounds.size()!=4)return data;
        auto geometry=UBShapeGeometry::paths(shapeType,QRectF(12,12,bounds[2].toDouble()-24,bounds[3].toDouble()-24));
        path=geometry.visible;hidden=geometry.hidden;
    } else if (shape.tagName() == "rect") {
        path.addRect(QRectF(shape.attribute("x").toDouble(), shape.attribute("y").toDouble(),
                           shape.attribute("width").toDouble(), shape.attribute("height").toDouble()));
    } else if (shape.tagName() == "circle") {
        const double radius = shape.attribute("r").toDouble();
        path.addEllipse(QPointF(shape.attribute("cx").toDouble(), shape.attribute("cy").toDouble()), radius, radius);
    } else if (shape.tagName() == "ellipse") {
        path.addEllipse(QPointF(shape.attribute("cx").toDouble(), shape.attribute("cy").toDouble()), shape.attribute("rx").toDouble(), shape.attribute("ry").toDouble());
    } else if (shape.tagName() == "polygon") {
        const auto coords = shape.attribute("points").split(QRegularExpression("[ ,]+"), Qt::SkipEmptyParts);
        if (coords.size() < 6 || coords.size() % 2) return data;
        path.moveTo(coords[0].toDouble(), coords[1].toDouble());
        for (int i = 2; i < coords.size(); i += 2) path.lineTo(coords[i].toDouble(), coords[i+1].toDouble());
        path.closeSubpath();
    } else return data;
    if(shapeType==5) {
        QPolygonF points;auto coords=shape.attribute("points").split(QRegularExpression("[ ,]+"),Qt::SkipEmptyParts);
        for(int i=0;i+1<coords.size();i+=2)points<<QPointF(coords[i].toDouble(),coords[i+1].toDouble());
        if(points.size()==3) {
            // Marker follows the actual perpendicular legs, including legacy
            // triangles whose vertices have been moved.
            for(int i=0;i<3;++i){auto v=points[i],a=points[(i+1)%3]-v,b=points[(i+2)%3]-v;
                double la=QLineF(QPointF(),linear.map(a)).length(),lb=QLineF(QPointF(),linear.map(b)).length();
                if(la>0&&lb>0&&qAbs(QPointF::dotProduct(a,b)/(QLineF(QPointF(),a).length()*QLineF(QPointF(),b).length()))<.001){
                    double size=qMin(la,lb)*.14;a*=size/la;b*=size/lb;
                    marker.moveTo(v+a);marker.lineTo(v+a+b);marker.lineTo(v+b);break;
                }
            }
        }
    }
    // A filled SVG path represents the outline. Ordinary OpenBoard and PDF
    // exporters can therefore retain its uniform width without custom painting
    // or relying on SVG vector-effect support.
    QPainterPathStroker stroker;
    stroker.setWidth(group.attribute("stroke-width", "3").toDouble());
    if (group.attribute("stroke-dasharray", "none") != "none") {
        QVector<qreal> dashes;
        const double width = group.attribute("stroke-width", "3").toDouble();
        for (const auto &v : group.attribute("stroke-dasharray").split(QRegularExpression("[ ,]+"), Qt::SkipEmptyParts))
            if (width > 0) dashes << v.toDouble()/width;
        if (!dashes.isEmpty()) stroker.setDashPattern(dashes);
    }
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setCapStyle(Qt::RoundCap);
    auto old=root.firstChildElement("path");
    while(!old.isNull()){auto next=old.nextSiblingElement("path");if(old.attribute("id").startsWith("lessonboard-shape-outline"))root.removeChild(old);old=next;}
    // Fill geometry stays editable; every visible/hidden outline is baked at
    // page width so non-uniform resize does not thicken individual edges.
    for(auto child=group.firstChildElement();!child.isNull();child=child.nextSiblingElement())child.setAttribute("stroke","none");
    auto bake=[&](const QPainterPath &source,bool dashed,const QString &suffix){
        if(source.isEmpty())return;
        stroker.setDashPattern(dashed?QVector<qreal>{4,3}:QVector<qreal>{});
        auto outline=inverse.map(stroker.createStroke(linear.map(source)));
        auto baked=doc.createElement("path");baked.setAttribute("id","lessonboard-shape-outline"+suffix);
        baked.setAttribute("d",UBShapeGeometry::data(outline));baked.setAttribute("fill",group.attribute("stroke","#000000"));
        baked.setAttribute("stroke","none");baked.setAttribute("fill-rule","nonzero");root.appendChild(baked);
    };
    bake(path,group.attribute("stroke-dasharray","none")!="none","");
    bake(hidden,true,"-hidden");bake(marker,false,"-marker");
    root.setAttribute("data-lessonboard-outline-key", fingerprint);
    return doc.toByteArray();
}
}
#endif
