// SPDX-License-Identifier: GPL-3.0-only
#include "../../src/domain/UBShapeSvg.h"
#include <QGuiApplication>
#include <QSvgRenderer>
#include <QImage>
#include <QPainter>
#include <QFile>
#include <QDomDocument>
#include <cstdio>
#include <cstdlib>
using namespace UBShapeGeometry;
static void check(bool pass,const char *message){if(!pass){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
int main(int argc,char **argv){
    QGuiApplication app(argc,argv);
    auto keys=Qt::ControlModifier|Qt::ShiftModifier;
    for(int type:{Rectangle,Ellipse,Triangle,Parallelogram})for(int sx:{-1,1})for(int sy:{-1,1}) {
        auto r=drawRect({0,0},{sx*240.,sy*150.},type,5,keys);
        check(qAbs(r.height()/r.width()-ratio(type))<1e-9,"Constrained ratio or drag quadrant");
        auto free=drawRect({0,0},{sx*240.,sy*150.},type,5,Qt::NoModifier);
        check(free.width()==240&&free.height()==150,"Normal shape must resize freely");
        for(auto single:{Qt::ControlModifier,Qt::ShiftModifier})check(drawRect({0,0},{240,150},type,5,single).height()==150,"Both modifier keys are required");
        if(type==Triangle||type==Parallelogram){auto v=polygon(type,r);double side=QLineF(v[0],v[1]).length();for(int i=1;i<v.size();++i)check(qAbs(QLineF(v[i],v[(i+1)%v.size()]).length()-side)<1e-7,"Constrained sides must be equal");}
    }
    for(int type:{Rectangle,Ellipse,Triangle,Parallelogram})for(bool edge:{false,true})for(int mirror:{-1,1}) {
        QSizeF inner(300,160),original(mirror*2.5,.6),requested(1.2,.8);
        auto scales=constrainResize(type,inner,original,requested,edge);
        double w=qAbs(inner.width()*original.width()*scales.width()),h=qAbs(inner.height()*original.height()*scales.height());
        check(qAbs(h/w-ratio(type))<1e-9,"Resize constraint after prior stretching or mirroring");
    }
    for(int n=3;n<=20;++n){auto r=drawRect({0,0},{240,150},RegularPolygon,n,Qt::NoModifier);auto v=polygon(RegularPolygon,r,n);check(v.size()==n,"Polygon side count");double side=QLineF(v[0],v[1]).length();for(int i=1;i<n;++i)check(qAbs(QLineF(v[i],v[(i+1)%n]).length()-side)<1e-7,"Regular polygon equal sides");}
    QImage gallery(960,780,QImage::Format_ARGB32_Premultiplied);gallery.fill(Qt::white);QPainter painter(&gallery);
    QStringList names={"Rectangle","Ellipse","Triangle","Right triangle","Parallelogram","Regular polygon","Cylinder","Rectangular prism","Square-based pyramid","Cone","Sphere","Triangular prism"};
    QList<int> types={0,4,3,5,8,9,10,11,12,13,14,15};
    for(int i=0;i<types.size();++i){int type=types[i];auto r=drawRect({12,12},{192,132},type,6,Qt::NoModifier);auto geometry=paths(type,r,6);check(!geometry.visible.isEmpty(),"Missing visible edges");if(type>=10)check(!geometry.hidden.isEmpty(),"3D hidden edges missing");if(type==5)check(geometry.marker.elementCount()==3,"Right angle marker missing");
        auto svg=QString("<svg xmlns='http://www.w3.org/2000/svg' width='%1' height='%2' viewBox='0 0 %1 %2' data-shape-sides='6'><metadata id='teaching-shape'>%3</metadata><g stroke='#273b3b' stroke-width='2.5' fill='#f5d86e'>%4</g></svg>").arg(r.width()+24).arg(r.height()+24).arg(type).arg(elements(type,r,6)).toUtf8();
        for(auto matrix:{QTransform(),QTransform().scale(2.5,.6),QTransform().rotate(37).scale(.7,2.1)}) {
            auto persisted=uniformSvg(svg,matrix);QDomDocument saved;check(saved.setContent(persisted),"Saved SVG XML");QSvgRenderer reader(persisted);check(reader.isValid(),"Saved SVG render");
            check(persisted.contains("lessonboard-shape-outline"),"Uniform outline missing");
            check(uniformSvg(persisted,matrix)==persisted,"Saved SVG must be stable on repeat reads");
            check(uniformSvg(persisted,QTransform().scale(1.1,.9)).contains("lessonboard-shape-outline"),"Re-resize saved SVG");
            if(type>=10)check(persisted.contains("lessonboard-shape-outline-hidden"),"Persisted hidden edges missing");
            if(type==5)check(persisted.contains("lessonboard-shape-outline-marker"),"Persisted right-angle marker missing");
            if(type==5&&qFuzzyIsNull(matrix.m12())&&qFuzzyIsNull(matrix.m21())) {
                auto markerDoc=saved;auto root=markerDoc.documentElement();
                for(auto node=root.firstChild();!node.isNull();){auto next=node.nextSibling();if(node.toElement().attribute("id")!="lessonboard-shape-outline-marker")root.removeChild(node);node=next;}
                QImage image(700,700,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter ink(&image);ink.translate(220,100);ink.setTransform(matrix,true);QSvgRenderer markerReader(markerDoc.toByteArray());markerReader.render(&ink,QRectF(0,0,r.width()+24,r.height()+24));ink.end();
                int left=700,top=700,right=0,bottom=0;
                for(int y=0;y<700;++y)for(int x=0;x<700;++x)if(image.pixelColor(x,y).lightness()<120){left=qMin(left,x);top=qMin(top,y);right=qMax(right,x);bottom=qMax(bottom,y);}
                check(left<right&&top<bottom&&qAbs((right-left)-(bottom-top))<=2,"Right-angle marker must stay square after non-uniform resize");
            }
        }
        svg=uniformSvg(svg,QTransform());QDomDocument doc;check(doc.setContent(svg),"SVG parse");QSvgRenderer renderer(svg);check(renderer.isValid(),"Qt SVG render");
        int x=(i%4)*240,y=(i/4)*260;painter.setPen(QColor("#273b3b"));painter.drawText(QRect(x,y+10,240,25),Qt::AlignCenter,names[i]);auto size=QSizeF(r.width()+24,r.height()+24).scaled(QSizeF(220,220),Qt::KeepAspectRatio);renderer.render(&painter,QRectF(x+(240-size.width())/2,y+40+(220-size.height())/2,size.width(),size.height()));
        if(argc>1){QFile f(QString::fromLocal8Bit(argv[1])+QString("/shape-%1.svg").arg(type));check(f.open(QIODevice::WriteOnly),"Save SVG");f.write(svg);}
    }
    painter.end();if(argc>1)gallery.save(QString::fromLocal8Bit(argv[1])+"/shape-gallery.png");
    puts("PASS: constraints in four drag quadrants, equal triangle/rhombus sides, 3–20-sided polygons, all 12 Qt-rendered shapes, hidden edges, right-angle marker and stable ordinary SVG outlines under resizing/rotation");
}
