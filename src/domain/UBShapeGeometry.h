// SPDX-License-Identifier: GPL-3.0-only
#ifndef UBSHAPEGEOMETRY_H
#define UBSHAPEGEOMETRY_H
#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QtMath>

// Persisted IDs 0..7 are retained so older lessons keep their geometry.
namespace UBShapeGeometry {
enum Type { Rectangle=0, Square=1, Circle=2, Triangle=3, Ellipse=4,
            RightTriangle=5, Diamond=6, Pentagon=7, Parallelogram=8,
            RegularPolygon=9, Cylinder=10, RectangularPrism=11,
            SquarePyramid=12, Cone=13, Sphere=14, TriangularPrism=15 };
inline bool constrained(Qt::KeyboardModifiers modifiers) {
    return (modifiers & (Qt::ControlModifier|Qt::ShiftModifier)) == (Qt::ControlModifier|Qt::ShiftModifier);
}
inline double ratio(int type) {
    if(type==Triangle)return qSqrt(3.)/2.;
    // The parallelogram's top is offset by a quarter of its total width.
    // Equal sides: (.75w)^2 = (.25w)^2 + h^2.
    if(type==Parallelogram)return qSqrt(.5);
    return 1.;
}
inline QSizeF constrainResize(int type,QSizeF inner,QSizeF originalScale,QSizeF requested,bool vertical) {
    const double w=inner.width()*qAbs(originalScale.width()),h=inner.height()*qAbs(originalScale.height());
    if(w<=0||h<=0||qFuzzyIsNull(requested.width())||qFuzzyIsNull(requested.height()))return requested;
    if(vertical)requested.setWidth((requested.width()<0?-1:1)*h*qAbs(requested.height())/(ratio(type)*w));
    else requested.setHeight((requested.height()<0?-1:1)*w*qAbs(requested.width())*ratio(type)/h);
    return requested;
}
inline QRectF drawRect(QPointF start,QPointF end,int type,int sides,Qt::KeyboardModifiers modifiers) {
    auto delta=end-start;
    bool lock=type==Square||type==Circle||type==Sphere||type==RegularPolygon;
    lock |= constrained(modifiers)&&(type==Rectangle||type==Ellipse||type==Triangle||type==Parallelogram);
    if(lock) {
        double aspect=ratio(type);
        // Regular polygons are fitted into a square canvas; the vertices lie
        // on a circle and are never independently stretched.
        Q_UNUSED(sides);
        double w=qMax(qAbs(delta.x()),qAbs(delta.y())/aspect);
        end=start+QPointF(delta.x()<0?-w:w,delta.y()<0?-w*aspect:w*aspect);
    }
    return QRectF(start,end).normalized();
}
inline QPolygonF polygon(int type,const QRectF &r,int sides=5) {
    auto pt=[&](double x,double y){return QPointF(r.left()+x*r.width(),r.top()+y*r.height());};
    if(type==Triangle)return QPolygonF(QVector<QPointF>{pt(.5,0),pt(1,1),pt(0,1)});
    if(type==RightTriangle)return QPolygonF(QVector<QPointF>{pt(0,0),pt(1,1),pt(0,1)});
    if(type==Parallelogram)return QPolygonF(QVector<QPointF>{pt(.25,0),pt(1,0),pt(.75,1),pt(0,1)});
    if(type==Diamond)return QPolygonF(QVector<QPointF>{pt(.5,0),pt(1,.5),pt(.5,1),pt(0,.5)});
    QPolygonF points;
    int n=type==Pentagon?5:qBound(3,sides,20);
    for(int i=0;i<n;++i){double angle=-M_PI/2+2*M_PI*i/n;points<<pt(.5+.5*qCos(angle),.5+.5*qSin(angle));}
    return points;
}
struct Paths { QPainterPath face,visible,hidden,marker; };
inline Paths paths(int type,const QRectF &r,int sides=5) {
    Paths p;auto pt=[&](double x,double y){return QPointF(r.left()+x*r.width(),r.top()+y*r.height());};
    auto line=[](QPainterPath &path,QPointF a,QPointF b){path.moveTo(a);path.lineTo(b);};
    auto poly=[](QPainterPath &path,const QVector<QPointF> &v){path.addPolygon(QPolygonF(v));path.closeSubpath();};
    auto arc=[&](QPainterPath &path,double y,double start,double sweep){QRectF e(r.left(),r.top()+y*r.height(),r.width(),r.height()*.22);path.arcMoveTo(e,start);path.arcTo(e,start,sweep);};
    if(type<=1)p.face.addRect(r);
    else if(type==Circle||type==Ellipse)p.face.addEllipse(r);
    else if(type<10)poly(p.face,polygon(type,r,sides));
    else if(type==Cylinder) {
        p.face.moveTo(pt(0,.11));p.face.cubicTo(pt(0,-.035),pt(1,-.035),pt(1,.11));p.face.lineTo(pt(1,.89));p.face.cubicTo(pt(1,1.035),pt(0,1.035),pt(0,.89));p.face.closeSubpath();
        p.visible.addEllipse(QRectF(r.left(),r.top(),r.width(),r.height()*.22));
        line(p.visible,pt(0,.11),pt(0,.89));line(p.visible,pt(1,.11),pt(1,.89));arc(p.visible,.78,180,180);arc(p.hidden,.78,0,180);
    } else if(type==Cone) {
        p.face.moveTo(pt(.5,0));p.face.lineTo(pt(1,.89));p.face.cubicTo(pt(1,1.035),pt(0,1.035),pt(0,.89));p.face.closeSubpath();
        line(p.visible,pt(.5,0),pt(0,.89));line(p.visible,pt(.5,0),pt(1,.89));arc(p.visible,.78,180,180);arc(p.hidden,.78,0,180);
    } else if(type==Sphere) {
        p.face.addEllipse(r);p.visible.addEllipse(r);arc(p.visible,.39,180,180);arc(p.hidden,.39,0,180);
    } else if(type==RectangularPrism) {
        auto a=pt(0,.25),b=pt(.72,.25),c=pt(.72,1),d=pt(0,1),e=pt(.28,0),f=pt(1,0),g=pt(1,.75),h=pt(.28,.75);
        poly(p.face,{a,e,f,g,c,d});poly(p.visible,{a,b,c,d});line(p.visible,a,e);line(p.visible,e,f);line(p.visible,f,g);line(p.visible,g,c);line(p.visible,b,f);
        line(p.hidden,e,h);line(p.hidden,h,g);line(p.hidden,h,d);
    } else if(type==SquarePyramid) {
        auto a=pt(.5,0),b=pt(0,.78),c=pt(.63,1),d=pt(1,.72),e=pt(.37,.5);
        poly(p.face,{a,d,c,b});line(p.visible,a,b);line(p.visible,a,c);line(p.visible,a,d);line(p.visible,b,c);line(p.visible,c,d);
        line(p.hidden,b,e);line(p.hidden,e,d);line(p.hidden,e,a);
    } else if(type==TriangularPrism) {
        auto a=pt(0,1),b=pt(.30,.20),c=pt(.60,1),d=pt(.40,.80),e=pt(.70,0),f=pt(1,.80);
        poly(p.face,{a,b,e,f,c});poly(p.visible,{a,b,c});line(p.visible,b,e);line(p.visible,e,f);line(p.visible,f,c);
        line(p.hidden,a,d);line(p.hidden,d,e);line(p.hidden,d,f);
    }
    if(type<10)p.visible=p.face;
    if(type==RightTriangle) {
        double s=qMin(r.width(),r.height())*.14;
        p.marker.moveTo(r.left(),r.bottom()-s);p.marker.lineTo(r.left()+s,r.bottom()-s);p.marker.lineTo(r.left()+s,r.bottom());
    }
    return p;
}
inline QString data(const QPainterPath &path) {
    QString d;auto n=[](double v){return QString::number(v,'g',12);};
    for(int i=0;i<path.elementCount();++i){auto e=path.elementAt(i);
        if(e.isMoveTo())d+="M"+n(e.x)+" "+n(e.y);
        else if(e.isLineTo())d+="L"+n(e.x)+" "+n(e.y);
        else if(e.type==QPainterPath::CurveToElement && i+2<path.elementCount()) {auto c=path.elementAt(++i),end=path.elementAt(++i);d+="C"+n(e.x)+" "+n(e.y)+" "+n(c.x)+" "+n(c.y)+" "+n(end.x)+" "+n(end.y);}
    }return d;
}
inline QString elements(int type,const QRectF &r,int sides=5) {
    auto p=paths(type,r,sides);QString face;
    if(type==Rectangle||type==Square)face=QString("<rect x='%1' y='%2' width='%3' height='%4'/>").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
    else if(type==Circle||type==Ellipse)face=QString("<ellipse cx='%1' cy='%2' rx='%3' ry='%4'/>").arg(r.center().x()).arg(r.center().y()).arg(r.width()/2).arg(r.height()/2);
    else if(type<10){QStringList points;for(auto v:polygon(type,r,sides))points<<QString("%1,%2").arg(v.x()).arg(v.y());face=QString("<polygon points='%1'/>").arg(points.join(' '));}
    else face=QString("<path d='%1' stroke='none'/><path d='%2' fill='none'/><path d='%3' fill='none' stroke-dasharray='6 4' data-hidden-edges='true'/>").arg(data(p.face),data(p.visible),data(p.hidden));
    if(!p.marker.isEmpty())face+=QString("<path id='right-angle-marker' d='%1' fill='none'/>").arg(data(p.marker));
    return face;
}
}
#endif
