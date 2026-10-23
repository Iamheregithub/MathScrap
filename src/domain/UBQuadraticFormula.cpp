// SPDX-License-Identifier: GPL-3.0-only
#include "UBQuadraticFormula.h"
#include <QDomDocument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtMath>
#include <climits>
#include <numeric>
#include <QBuffer>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPathStroker>
#include <QSvgGenerator>

namespace {
QString num(int n) { return QString::number(n); }
QString formatted(double x) { return QString::number(x, 'f', 3).remove(QRegularExpression("0+$")).remove(QRegularExpression("\\.$")); }
QString exact(int a, int b, int discriminant) {
    if (!discriminant) {
        const int denominator=2*a, divisor=std::gcd(qAbs(b),qAbs(denominator));
        int n=-b/divisor, d=denominator/divisor;
        if (d<0) { n=-n; d=-d; }
        return "x = " + num(n) + (d==1 ? QString() : " / " + num(d));
    }
    const int denominator = 2*qAbs(a), sign = a < 0 ? -1 : 1;
    int scalar = 1, radicand = qAbs(discriminant);
    for (int i=2; i*i<=radicand; ++i) while (radicand%(i*i)==0) { radicand/=i*i; scalar*=i; }
    const int numerator = -b*sign;
    const int divisor = std::gcd(std::gcd(qAbs(numerator), scalar), denominator);
    const int n = numerator/divisor, k = scalar/divisor, d = denominator/divisor;
    const QString prefix=(k==1?QString():num(k)) + (discriminant<0?"i":"");
    const QString radical=prefix + (radicand==1?QString():"√"+num(radicand));
    const QString top = num(n) + " ± " + (radical.isEmpty()?QString("1"):radical);
    return d==1 ? "x = " + top : "x = (" + top + ") / " + num(d);
}

// Lay out mathematical expressions with measured glyphs and vector rules.
// Paths avoid differences in SVG text whitespace, font fallback and superscript support.
struct Math { QPainterPath path; qreal width=0, height=0, baseline=0; };
Math text(const QString &value, int size=30, bool ui=false) {
    QFont font(ui?QStringLiteral("Segoe UI"):QStringLiteral("Cambria Math"));
    font.setPixelSize(size);
    QFontMetricsF metrics(font);
    Math result;result.width=metrics.horizontalAdvance(value);result.height=metrics.height();result.baseline=metrics.ascent();
    result.path.addText(0,result.baseline,font,value);
    return result;
}
void append(Math &target,const Math &part,qreal x,qreal y) {
    target.path.addPath(QTransform::fromTranslate(x,y).map(part.path));
}
Math join(const QList<Math> &parts,qreal gap=0) {
    Math result;
    for(const auto &part:parts)result.baseline=qMax(result.baseline,part.baseline);
    for(const auto &part:parts){append(result,part,result.width,result.baseline-part.baseline);result.height=qMax(result.height,result.baseline-part.baseline+part.height);result.width+=part.width+gap;}
    if(!parts.isEmpty())result.width-=gap;
    return result;
}
Math squared(const QString &value) {
    const auto base=text(value), exponent=text("2",18);
    Math result;result.width=base.width+exponent.width+2;result.baseline=base.baseline+7;result.height=base.height+7;
    append(result,base,0,7);append(result,exponent,base.width+2,0);return result;
}
Math radical(const Math &inside) {
    Math result;result.width=inside.width+34;result.height=inside.height+8;result.baseline=inside.baseline+8;
    append(result,inside,28,8);
    QPainterPath rule;rule.moveTo(1,result.baseline-7);rule.lineTo(7,result.baseline-3);rule.lineTo(13,result.baseline+5);rule.lineTo(22,3);rule.lineTo(result.width,3);
    QPainterPathStroker stroke;stroke.setWidth(1.7);stroke.setJoinStyle(Qt::MiterJoin);result.path.addPath(stroke.createStroke(rule));return result;
}
Math divide(const Math &numerator,const Math &denominator) {
    Math result;result.width=qMax(numerator.width,denominator.width)+22;
    const qreal bar=numerator.height+9;
    result.height=numerator.height+denominator.height+22;result.baseline=bar+10;
    append(result,numerator,(result.width-numerator.width)/2,0);
    append(result,denominator,(result.width-denominator.width)/2,bar+12);
    result.path.addRect(QRectF(0,bar,result.width,1.5));return result;
}
Math exactMath(int a,int b,int discriminant) {
    if(!discriminant){int n=-b,d=2*a,g=std::gcd(qAbs(n),qAbs(d));n/=g;d/=g;if(d<0){n=-n;d=-d;}return d==1?text(num(n)):divide(text(num(n)),text(num(d)));}
    int factor=1,m=qAbs(discriminant);
    for(int i=2;i*i<=m;++i)while(m%(i*i)==0){m/=i*i;factor*=i;}
    int n=a<0?b:-b,d=2*qAbs(a),g=std::gcd(std::gcd(qAbs(n),factor),d);n/=g;factor/=g;d/=g;
    QList<Math> terms;
    if(n)terms<<text(num(n))<<text(" ± ");else terms<<text("± ");
    if(factor!=1||m==1&&discriminant>0)terms<<text(num(factor));
    if(discriminant<0)terms<<text("i");
    if(m!=1)terms<<radical(text(num(m)));
    const auto top=join(terms);return d==1?top:divide(top,text(num(d)));
}
}

namespace UBQuadraticFormula {
bool read(const QByteArray &svg, State *state) {
    if (!state || !svg.contains("lessonboard-quadratic")) return false;
    QDomDocument doc;
    if (!doc.setContent(svg)) return false;
    const auto elements = doc.elementsByTagName("metadata");
    for (int i=0; i<elements.size(); ++i) {
        const auto e=elements.at(i).toElement();
        if (e.attribute("id")!="lessonboard-quadratic") continue;
        const auto json=QJsonDocument::fromJson(QByteArray::fromBase64(e.text().toLatin1()));
        if (!json.isObject()) return false;
        const auto o=json.object();
        if (!o.value("a").isDouble() || !o.value("b").isDouble() || !o.value("c").isDouble() || !o.value("step").isDouble()) return false;
        state->a=o.value("a").toInt(); state->b=o.value("b").toInt(); state->c=o.value("c").toInt();
        state->step=qBound(0,o.value("step").toInt(),5);
        return state->a!=0 && qAbs(state->a)<=9999 && qAbs(state->b)<=9999 && qAbs(state->c)<=9999;
    }
    return false;
}

QByteArray render(const State &s) {
    if (!s.a || qAbs(s.a)>9999 || qAbs(s.b)>9999 || qAbs(s.c)>9999) return {};
    const int stage=qBound(0,s.step,5);
    const qint64 d64=qint64(s.b)*s.b-4*qint64(s.a)*s.c;
    if (d64 < INT_MIN || d64 > INT_MAX) return {};
    const int d=int(d64);
    const QString pa="("+num(s.a)+")", pb="("+num(s.b)+")", pc="("+num(s.c)+")";
    QList<Math> rows;
    rows << divide(join({text("−b ± "),radical(join({squared("b"),text(" − 4ac")}))}),text("2a"));
    if(stage>=1) rows << divide(join({text("−"+(s.b<0?pb:num(s.b))+" ± "),radical(join({squared(pb),text(" − 4"+pa+pc)}))}),text("2"+pa));
    if(stage>=2) rows << divide(join({text(num(-s.b)+" ± "),radical(text(QString("%1 − %2").arg(qint64(s.b)*s.b).arg(4*qint64(s.a)*s.c)))}),text(num(2*s.a)));
    if(stage>=3) rows << divide(join({text(num(-s.b)+" ± "),radical(text(num(d)))}),text(num(2*s.a)));
    if(stage>=4) rows << exactMath(s.a,s.b,d);
    for(auto &row:rows)row=join({text("x = "),row},10);
    if(stage>=5){
        if(d>0) rows << text(QString("x ≈ %1   or   x ≈ %2").arg(formatted((-s.b+qSqrt(d))/(2.0*s.a)),formatted((-s.b-qSqrt(d))/(2.0*s.a))),28);
        else if(!d) rows << text(QString("One repeated root:  x = %1").arg(formatted(-s.b/(2.0*s.a))),26);
        else rows << text("No real roots (complex roots shown above).",24);
    }
    qreal width=560,height=112;
    for(const auto &row:rows){width=qMax(width,row.width+64);height+=row.height+22;}
    height+=66;
    QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);
    QSvgGenerator generator;generator.setOutputDevice(&buffer);generator.setSize(QSize(qCeil(width),qCeil(height)));generator.setViewBox(QRectF(0,0,width,height));
    QPainter painter(&generator);painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#ded8cc"),1.2));painter.setBrush(QColor("#fffdf7"));painter.drawRoundedRect(QRectF(1,1,width-2,height-2),12,12);
    auto draw=[&painter](const Math &part,qreal x,qreal y){painter.fillPath(QTransform::fromTranslate(x,y).map(part.path),QColor("#253e3c"));};
    draw(text("Quadratic formula",20,true),28,20);
    const int values[]={s.a,s.b,s.c};const QStringList names={"a","b","c"};
    for(int i=0;i<3;++i){const qreal x=28+i*150;painter.setPen(Qt::NoPen);painter.setBrush(QColor("#f1eee5"));painter.drawRoundedRect(QRectF(x,56,132,38),7,7);draw(text(names[i]+" = "+num(values[i]),22),x+12,61);}
    qreal y=114;for(const auto &row:rows){draw(row,32,y);y+=row.height+22;}
    const qreal buttonY=height-54;
    for(int i=0;i<2;++i){const bool enabled=i?stage<5:stage>0;const qreal x=width-280+i*134;painter.setPen(Qt::NoPen);painter.setBrush(QColor(enabled?"#dceae4":"#f0eee7"));painter.drawRoundedRect(QRectF(x,buttonY,122,36),7,7);const auto label=text(i?"Next step":"Previous",17,true);painter.fillPath(QTransform::fromTranslate(x+(122-label.width)/2,buttonY+(36-label.height)/2).map(label.path),QColor(enabled?"#253e3c":"#92978f"));}
    painter.end();buffer.close();
    QDomDocument doc;if(!doc.setContent(bytes))return {};
    auto root=doc.documentElement();root.setAttribute("data-quadratic-layout",2);
    const QJsonObject state{{"a",s.a},{"b",s.b},{"c",s.c},{"step",stage}};
    auto metadata=doc.createElement("metadata");metadata.setAttribute("id","lessonboard-quadratic");metadata.appendChild(doc.createTextNode(QString::fromLatin1(QJsonDocument(state).toJson(QJsonDocument::Compact).toBase64())));root.prependChild(metadata);
    QString result=exact(s.a,s.b,d);
    if(d>0)result+=QString("; roots approximately %1 and %2").arg(formatted((-s.b+qSqrt(d))/(2.0*s.a)),formatted((-s.b-qSqrt(d))/(2.0*s.a)));
    else if(!d)result+="; One repeated root";
    else result+="; No real roots";
    auto description=doc.createElement("desc");description.appendChild(doc.createTextNode(QString("a=%1, b=%2, c=%3. Substitution: 4%4%5; denominator 2%6. %7").arg(s.a).arg(s.b).arg(s.c).arg(pa,pc,pa,result)));root.appendChild(description);
    return doc.toByteArray();
}

int stepAt(const QByteArray &svg, double x, double y) {
    State state; if (!read(svg,&state)) return -1;
    QDomDocument doc;if(!doc.setContent(svg))return -1;
    const auto root=doc.documentElement();
    if(root.attribute("data-quadratic-layout")=="2"){
        const auto values=root.attribute("viewBox").split(QRegularExpression("[ ,]+"),Qt::SkipEmptyParts);
        if(values.size()!=4)return -1;
        const qreal width=values[2].toDouble(),height=values[3].toDouble();
        if(y<height-54||y>height-18)return -1;
        if(x>=width-280&&x<=width-158)return 0;
        if(x>=width-146&&x<=width-24)return 1;
    }else{
        const int height=165+(state.step+1)*66;
        if(y<height-52||y>height-14)return -1;
        if(x>=454&&x<=572)return 0;
        if(x>=585&&x<=713)return 1;
    }
    return -1;
}
}
