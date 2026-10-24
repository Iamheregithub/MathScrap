// SPDX-License-Identifier: GPL-3.0-only
#include "UBMathFormula.h"
#include "UBQuadraticFormula.h"
#include <QtWidgets>
#include <QDomDocument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSvgGenerator>
#include <QtMath>
#include <numeric>
#include <cmath>
namespace {
QString num(int n){return QString::number(n);}
QString nfmt(double n){return QString::number(n==0?0:n,'g',15);}
QString dec(double n){if(qAbs(n)<0.0005)n=0;QString s=QString::number(n,'f',3);s.remove(QRegularExpression("0+$"));s.remove(QRegularExpression("\\.$"));return s;}
QString par(double n){return "("+nfmt(n)+")";}
struct Math { QString spoken; QPainterPath path; qreal width=0, height=0, baseline=0; };
Math text(const QString &value, int size=30, bool ui=false) {
    QFont font(ui?QStringLiteral("Segoe UI"):QStringLiteral("Cambria Math"));
    font.setPixelSize(size);
    QFontMetricsF metrics(font);
    Math result;result.spoken=value;result.width=metrics.horizontalAdvance(value);result.height=metrics.height();result.baseline=metrics.ascent();
    result.path.addText(0,result.baseline,font,value);
    return result;
}
void append(Math &target,const Math &part,qreal x,qreal y) {
    target.path.addPath(QTransform::fromTranslate(x,y).map(part.path));
}
Math join(const QList<Math> &parts,qreal gap=0) {
    Math result;
    for(const auto &part:parts)result.baseline=qMax(result.baseline,part.baseline);
    for(const auto &part:parts){result.spoken+=part.spoken;append(result,part,result.width,result.baseline-part.baseline);result.height=qMax(result.height,result.baseline-part.baseline+part.height);result.width+=part.width+gap;}
    if(!parts.isEmpty())result.width-=gap;
    return result;
}
Math squared(const QString &value) {
    const auto base=text(value), exponent=text("2",18);
    Math result;result.spoken=value+"²";result.width=base.width+exponent.width+2;result.baseline=base.baseline+7;result.height=base.height+7;
    append(result,base,0,7);append(result,exponent,base.width+2,0);return result;
}
Math radical(const Math &inside) {
    Math result;result.spoken="sqrt("+inside.spoken+")";result.width=inside.width+34;result.height=inside.height+8;result.baseline=inside.baseline+8;
    append(result,inside,28,8);
    QPainterPath rule;rule.moveTo(1,result.baseline-7);rule.lineTo(7,result.baseline-3);rule.lineTo(13,result.baseline+5);rule.lineTo(22,3);rule.lineTo(result.width,3);
    QPainterPathStroker stroke;stroke.setWidth(1.7);stroke.setJoinStyle(Qt::MiterJoin);result.path.addPath(stroke.createStroke(rule));return result;
}
Math divide(const Math &numerator,const Math &denominator) {
    Math result;result.spoken="("+numerator.spoken+")/("+denominator.spoken+")";result.width=qMax(numerator.width,denominator.width)+22;
    const qreal bar=numerator.height+9;
    result.height=numerator.height+denominator.height+22;result.baseline=bar+10;
    append(result,numerator,(result.width-numerator.width)/2,0);
    append(result,denominator,(result.width-denominator.width)/2,bar+12);
    result.path.addRect(QRectF(0,bar,result.width,1.5));return result;
}
Math relation(const Math &lhs,const Math &rhs){return join({lhs,text(" = "),rhs},6);}
Math exactRoot(double value){
    const double root=qSqrt(value);
    if(root==qFloor(root))return text(nfmt(root));
    if(value>0&&value<=1000000000&&value==qFloor(value)){
        int remainder=int(value),factor=1;
        for(int i=2;i*i<=remainder;++i)while(remainder%(i*i)==0){remainder/=i*i;factor*=i;}
        if(factor>1)return join({text(QString::number(factor)),radical(text(QString::number(remainder)))});
    }
    return radical(text(nfmt(value)));
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
Math power(const QString &base,int exponent){if(exponent==1)return text(base);const auto b=text(base),e=text(QString::number(exponent),18);Math r;r.spoken=base+"^"+QString::number(exponent);r.width=b.width+e.width+2;r.baseline=b.baseline+7;r.height=b.height+7;append(r,b,0,7);append(r,e,b.width+2,0);return r;}
Math eq(const QString &name,const Math &rhs){return join({text(name+" = "),rhs},6);}
Math stack(const QList<Math> &lines){Math r;for(const auto &line:lines){r.spoken+=line.spoken+" ";append(r,line,0,r.height);r.width=qMax(r.width,line.width);r.height+=line.height+8;}r.baseline=lines.isEmpty()?0:lines.first().baseline;return r;}
struct Work { QList<Math> rows; QString message; QStringList semantic; };
using Polynomial=QMap<int,double>;
using Terms=QMap<QPair<int,int>,double>;
bool parsePolynomial(QString source,Polynomial &p){
    source.remove(QRegularExpression("\\s+"));source.replace(QChar(0x2212),'-');
    if(source.startsWith("f(x)="))source.remove(0,5);if(source.startsWith("y="))source.remove(0,2);
    if(source.isEmpty()||source.size()>180)return false;
    source.replace("-","+-");if(source.startsWith('+'))source.remove(0,1);
    const QRegularExpression term("^([+-]?)(?:(\\d*\\.?\\d+)(?:/(\\d*\\.?\\d+))?\\*?)?(x(?:\\^([0-4]))?)?$");
    const auto parts=source.split('+',Qt::KeepEmptyParts);
    for(const auto &part:parts){const auto m=term.match(part);if(!m.hasMatch()||(m.captured(2).isEmpty()&&m.captured(4).isEmpty()))return false;
        if(part.endsWith('*'))return false;
        bool ok=true;double c=m.captured(2).isEmpty()?1:m.captured(2).toDouble(&ok);if(!ok||!m.captured(3).isEmpty())return false;if(m.captured(1)=="-")c=-c;
        int degree=m.captured(4).isEmpty()?0:m.captured(5).isEmpty()?1:m.captured(5).toInt();p[degree]+=c;if(!std::isfinite(c)||qAbs(c)>1000000)return false;}
    return true;
}
int choose(int n,int k){int r=1;for(int i=1;i<=k;++i)r=r*(n-i+1)/i;return r;}
Math polynomial(const Terms &terms){
    QList<Math> lines,parts;bool first=true;qreal width=0;
    for(auto it=terms.cend();it!=terms.cbegin();){--it;const double c=it.value();if(c==0)continue;
        QList<Math> t;QString sign=c<0?(first?"−":" − "):(first?"":" + ");if(!sign.isEmpty())t<<text(sign);
        const int xp=it.key().first,hp=it.key().second;
        if(qAbs(c)!=1||(!xp&&!hp))t<<text(nfmt(qAbs(c)));
        if(xp)t<<power("x",xp);if(hp)t<<power("h",hp);const Math next=join(t);
        if(!parts.isEmpty()&&width+next.width>700){lines<<join(parts);parts.clear();width=0;}
        parts<<next;width+=next.width;first=false;
    }
    if(parts.isEmpty()&&lines.isEmpty())return text("0");if(!parts.isEmpty())lines<<join(parts);return lines.size()==1?lines.first():stack(lines);
}
Terms asTerms(const Polynomial &p){Terms t;for(auto it=p.cbegin();it!=p.cend();++it)t[{it.key(),0}]=it.value();return t;}
Math polynomialAt(const Polynomial &p,const QString &variable){QList<Math> parts;bool first=true;for(auto it=p.cend();it!=p.cbegin();){--it;double c=it.value();if(c==0)continue;const QString sign=c<0?(first?"−":" − "):(first?"":" + ");parts<<text(sign);if(qAbs(c)!=1||it.key()==0)parts<<text(nfmt(qAbs(c)));if(it.key())parts<<power(variable,it.key());first=false;}return first?text("0"):join(parts);}
Math limit(const Math &inside){return join({stack({text("lim",24),text("h → 0",16)}),inside},12);}
Work build(const UBMathFormula::State &s){
    Work w;const QString k=s.kind;
    const auto T=[](const QString &v){return text(v);};
    if(k=="quadratic")w.rows<<eq("x",divide(join({T("−b ± "),radical(join({squared("b"),T(" − 4ac")}))}),T("2a")));
    else if(k=="discriminant")w.rows<<eq("D",join({squared("b"),T(" − 4ac")}));
    else if(k=="pythagoras")w.rows<<join({squared("a"),T(" + "),squared("b"),T(" = "),squared("c")});
    else if(k=="distance")w.rows<<eq("d",radical(join({squared("(x₂ − x₁)"),T(" + "),squared("(y₂ − y₁)")})));
    else if(k=="slope")w.rows<<eq("m",divide(T("y₂ − y₁"),T("x₂ − x₁")));
    else if(k=="sine")w.rows<<join({divide(T("a"),T("sin A")),T(" = "),divide(T("b"),T("sin B"))},8);
    else if(k=="cosine-side"||k=="cosine-angle")w.rows<<relation(squared("a"),join({squared("b"),T(" + "),squared("c"),T(" − 2bc cos A")}));
    else if(k=="quotient"||k=="derivative"){
        const auto definition=divide(T("f(x + h) − f(x)"),T("h"));w.rows<<eq(k=="derivative"?"f′(x)":"Q",k=="derivative"?limit(definition):definition);
        if(s.values.value(0).isEmpty()){w.message="Enter f(x). Polynomials through degree 4 are supported.";return w;}
        Polynomial p;if(!parsePolynomial(s.values.value(0),p)){w.message="Unsupported function. Use a polynomial in x (degree 0–4; integer or decimal coefficients).";return w;}
        auto quotient=[&](const Math &v){return eq(k=="derivative"?"f′(x)":"Q",k=="derivative"?limit(v):v);};
        w.rows<<quotient(divide(join({polynomialAt(p,"(x + h)"),T(" − ("),polynomialAt(p,"x"),T(")")}),T("h")));
        Terms expansion;for(auto it=p.cbegin();it!=p.cend();++it)for(int h=0;h<=it.key();++h)expansion[{it.key()-h,h}]+=it.value()*choose(it.key(),h);
        w.rows<<quotient(divide(join({polynomial(expansion),T(" − ("),polynomialAt(p,"x"),T(")")}),T("h")));
        Terms difference=expansion;for(auto it=p.cbegin();it!=p.cend();++it)difference[{it.key(),0}]-=it.value();
        w.rows<<quotient(divide(polynomial(difference),T("h")));
        Terms q;for(auto it=difference.cbegin();it!=difference.cend();++it)if(it.key().second>0)q[{it.key().first,it.key().second-1}]+=it.value();
        w.rows<<quotient(divide(join({T("h("),polynomial(q),T(")")}),T("h")));
        w.rows<<quotient(polynomial(q));
        if(k=="quotient")w.rows<<text("For h ≠ 0.",22);
        if(k=="derivative"){Terms derivative;for(auto it=p.cbegin();it!=p.cend();++it)if(it.key())derivative[{it.key()-1,0}]=it.value()*it.key();w.rows<<eq("f′(x)",polynomial(derivative));}
        w.semantic<<"Polynomial expansion, cancellation, h ≠ 0";return w;
    }else{w.message="Unknown formula.";return w;}
    const auto labels=UBMathFormula::labels(s);QList<double> v;
    for(int i=0;i<labels.size();++i){const QString raw=s.values.value(i).trimmed();if(raw.isEmpty()||raw=="-"||raw=="+"||raw=="."){w.message="Enter all values to show the substitution.";return w;}bool ok=false;double n=raw.toDouble(&ok);if(!ok||!std::isfinite(n)||qAbs(n)>1000000){w.message="Enter a finite number with magnitude at most 1,000,000.";return w;}v<<n;}
    auto fail=[&](const QString &m){w.message=m;return w;};
    if(k=="quadratic"||k=="discriminant"){
        for(double n:v)if(n!=qRound(n)||qAbs(n)>9999)return fail("Use integer coefficients from −9999 to 9999.");
        const int a=int(v[0]),b=int(v[1]),c=int(v[2]),d=b*b-4*a*c;
        if(!a)return fail("a must be nonzero for a quadratic.");
        if(k=="quadratic"){
            w.rows<<eq("x",divide(join({T("−"+(b<0?par(b):num(b))+" ± "),radical(join({squared(par(b)),T(" − 4"+par(a)+par(c))}))}),T("2"+par(a))));
            w.rows<<eq("x",divide(join({T(num(-b)+" ± "),radical(T(QString::number(b*b)+" − "+(4*a*c<0?par(4*a*c):num(4*a*c))))}),T(num(2*a))));
            w.rows<<eq("x",divide(join({T(num(-b)+" ± "),radical(T(num(d)))}),T(num(2*a))));w.rows<<eq("x",exactMath(a,b,d));
            if(d>0)w.rows<<T("x ≈ "+dec((-b+qSqrt(d))/(2.0*a))+"   or   x ≈ "+dec((-b-qSqrt(d))/(2.0*a)));
            else if(!d)w.rows<<T("One repeated root: x = "+nfmt(-b/(2.0*a)));
            else w.rows<<text("No real roots. Complex roots shown above.",24);
            w.semantic<<"quadratic roots"<<nfmt(d);
        }else{
            w.rows<<eq("D",join({squared(par(b)),T(" − 4"+par(a)+par(c))}));w.rows<<eq("D",T(num(b*b)+" − "+(4*a*c<0?par(4*a*c):num(4*a*c))));w.rows<<eq("D",T(num(d)));
            w.rows<<text(d>0?"D > 0: two distinct real roots.":d==0?"D = 0: one repeated real root.":"D < 0: no real roots.",24);
        }
    }else if(k=="pythagoras"){
        if(v[0]<=0||v[1]<=0)return fail("Side lengths must be positive.");
        const bool hyp=s.mode==0;const double rad=hyp?v[0]*v[0]+v[1]*v[1]:v[0]*v[0]-v[1]*v[1];
        if(rad<=0)return fail("The hypotenuse must be longer than the known leg.");
        const QString unknown=QStringList{"c","a","b"}.value(qBound(0,s.mode,2));
        const double first=v[0]*v[0],second=v[1]*v[1];
        if(hyp){
            w.rows<<relation(join({squared(nfmt(v[0])),T(" + "),squared(nfmt(v[1]))}),squared("c"));
            w.rows<<relation(T(nfmt(first)+" + "+nfmt(second)),squared("c"));
            w.rows<<relation(T(nfmt(rad)),squared("c"));
            w.rows<<relation(radical(T(nfmt(rad))),T("c"));
            const auto exact=exactRoot(rad);
            if(exact.spoken!=radical(T(nfmt(rad))).spoken)w.rows<<relation(exact,T("c"));
        }else{
            const bool a=s.mode==1;
            w.rows<<relation(a?join({squared("a"),T(" + "),squared(nfmt(v[1]))}):join({squared(nfmt(v[1])),T(" + "),squared("b")}),squared(nfmt(v[0])));
            w.rows<<relation(a?join({squared("a"),T(" + "+nfmt(second))}):join({T(nfmt(second)+" + "),squared("b")}),T(nfmt(first)));
            w.rows<<relation(squared(unknown),T(nfmt(first)+" − "+nfmt(second)));
            w.rows<<relation(squared(unknown),T(nfmt(rad)));
            w.rows<<eq(unknown,radical(T(nfmt(rad))));
            const auto exact=exactRoot(rad);
            if(exact.spoken!=radical(T(nfmt(rad))).spoken)w.rows<<eq(unknown,exact);
        }
        if(qSqrt(rad)!=qFloor(qSqrt(rad)))w.rows<<text(unknown+" ≈ "+dec(qSqrt(rad)));
    }else if(k=="distance"||k=="slope"){
        const double dx=v[2]-v[0],dy=v[3]-v[1];
        const QString xs=par(v[2])+" − "+par(v[0]),ys=par(v[3])+" − "+par(v[1]);
        if(k=="slope"){
            w.rows<<eq("m",divide(T(ys),T(xs)));
            if(dx==0){w.rows<<text(dy==0?"Slope is undefined: the two points coincide.":"Slope is undefined: the line is vertical.",24);return w;}
            if(!std::isfinite(dy/dx))return fail("The slope is too large to represent. Rescale the coordinates.");
            w.rows<<eq("m",divide(T(nfmt(dy)),T(nfmt(dx))));
            if(dy==qRound(dy)&&dx==qRound(dx)){int n=qRound(dy),den=qRound(dx),g=std::gcd(qAbs(n),qAbs(den));n/=g;den/=g;if(den<0){n=-n;den=-den;}w.rows<<eq("m",den==1?T(num(n)):divide(T(num(n)),T(num(den))));}
            w.rows<<text("m ≈ "+dec(dy/dx));
        }else{
            const double r=dx*dx+dy*dy;
            w.rows<<eq("d",radical(join({squared("("+xs+")"),T(" + "),squared("("+ys+")")})));
            w.rows<<eq("d",radical(join({squared(par(dx)),T(" + "),squared(par(dy))})));
            w.rows<<eq("d",radical(T(nfmt(dx*dx)+" + "+nfmt(dy*dy))));w.rows<<eq("d",radical(T(nfmt(r))));
            const auto exact=exactRoot(r);if(exact.spoken!=radical(T(nfmt(r))).spoken)w.rows<<eq("d",exact);
            if(qSqrt(r)!=qFloor(qSqrt(r)))w.rows<<text("d ≈ "+dec(qSqrt(r)));
        }
    }else if(k=="sine"){
        const double a=v[0],A=v[1],third=v[2];if(a<=0||A<=0||A>=180)return fail("Use a positive side and an angle strictly between 0° and 180°.");
        const double sinA=qSin(qDegreesToRadians(A));
        if(sinA==0)return fail("Angle A is too small to calculate reliably.");
        if(s.mode==0){if(third<=0||A+third>=180)return fail("The known angles must be positive and sum to less than 180°.");
            w.rows<<relation(divide(T(nfmt(a)),T("sin("+nfmt(A)+"°)")),divide(T("b"),T("sin("+nfmt(third)+"°)")));
            w.rows<<relation(T("b sin("+nfmt(A)+"°)"),T(nfmt(a)+" sin("+nfmt(third)+"°)"));
            w.rows<<eq("b",divide(T(nfmt(a)+" sin("+nfmt(third)+"°)"),T("sin("+nfmt(A)+"°)")));double b=a*qSin(qDegreesToRadians(third))/sinA;
            if(!std::isfinite(b))return fail("The side is too large to represent. Rescale the known side.");
            w.rows<<text("b ≈ "+dec(b));
        }else{if(third<=0)return fail("Side b must be positive.");const double ratio=third*qSin(qDegreesToRadians(A))/a;
            w.rows<<relation(divide(T(nfmt(a)),T("sin("+nfmt(A)+"°)")),divide(T(nfmt(third)),T("sin B")));
            w.rows<<relation(T(nfmt(a)+" sin B"),T(nfmt(third)+" sin("+nfmt(A)+"°)"));
            w.rows<<eq("sin B",divide(T(nfmt(third)+" sin("+nfmt(A)+"°)"),T(nfmt(a))));w.rows<<eq("sin B",T(nfmt(ratio)));
            if(ratio>1+1e-12){w.rows<<text("No triangle: sin B would exceed 1.",24);return w;}
            const double B=qRadiansToDegrees(qAsin(qBound(-1.0,ratio,1.0)));QList<double> angles;
            for(double candidate:{B,180-B})if(candidate>0&&A+candidate<180-1e-9&&(angles.isEmpty()||qAbs(candidate-angles.first())>1e-8))angles<<candidate;
            w.rows<<eq("B",join({power("sin",-1),T("("+nfmt(ratio)+")")}));
            if(angles.isEmpty())w.rows<<text("No valid triangle: the angle sum reaches 180°.",24);
            else{for(int i=0;i<angles.size();++i)w.rows<<text(QString("%1B ≈ %2°; C ≈ %3°").arg(angles.size()>1?QString("Triangle %1: ").arg(i+1):QString(),dec(angles[i]),dec(180-A-angles[i])),24);if(angles.size()==2)w.rows<<text("SSA ambiguous case: both triangles are valid.",22);}
        }
    }else if(k=="cosine-side"){
        const double b=v[0],c=v[1],A=v[2];if(b<=0||c<=0||A<=0||A>=180)return fail("Use positive sides and an included angle between 0° and 180°.");
        // The expanded cosine law loses the small difference when b and c
        // are similar and A is small. This equivalent form avoids cancellation.
        const double side=A==90?std::hypot(b,c):std::hypot(b-c,2*qSqrt(b)*qSqrt(c)*qSin(qDegreesToRadians(A/2)));
        const double rad=A==90?b*b+c*c:side*side;
        w.rows<<relation(squared("a"),join({squared(nfmt(b)),T(" + "),squared(nfmt(c)),T(" − 2"+par(b)+par(c)+" cos("+nfmt(A)+"°)")}));
        w.rows<<relation(squared("a"),T(nfmt(b*b)+" + "+nfmt(c*c)+" − "+nfmt(2*b*c)+" cos("+nfmt(A)+"°)"));
        w.rows<<relation(squared("a"),T(nfmt(rad)));
        w.rows<<eq("a",radical(T(nfmt(rad))));
        const auto exact=exactRoot(rad);if(exact.spoken!=radical(T(nfmt(rad))).spoken)w.rows<<eq("a",exact);
        if(side!=qFloor(side))w.rows<<text("a ≈ "+dec(side));
    }else if(k=="cosine-angle"){
        const double a=v[0],b=v[1],c=v[2];if(a<=0||b<=0||c<=0||a+b<=c||a+c<=b||b+c<=a)return fail("These side lengths do not form a triangle.");
        const double scale=qMax(a,qMax(b,c)),an=a/scale,bn=b/scale,cn=c/scale;
        const double denominator=2*bn*cn;
        if(denominator==0)return fail("These side lengths differ too much to calculate the angle reliably.");
        const double ratio=(bn*bn+cn*cn-an*an)/denominator;
        w.rows<<relation(squared(nfmt(a)),join({squared(nfmt(b)),T(" + "),squared(nfmt(c)),T(" − 2"+par(b)+par(c)+" cos A")}));
        if(a*a>0&&b*b>0&&c*c>0&&2*b*c>0){
            w.rows<<relation(T(nfmt(a*a)),T(nfmt(b*b)+" + "+nfmt(c*c)+" − "+nfmt(2*b*c)+" cos A"));
            w.rows<<relation(T(nfmt(2*b*c)+" cos A"),T(nfmt(b*b)+" + "+nfmt(c*c)+" − "+nfmt(a*a)));
            w.rows<<eq("cos A",divide(T(nfmt(b*b+c*c-a*a)),T(nfmt(2*b*c))));
        }else w.rows<<eq("cos A",divide(join({squared(par(b)),T(" + "),squared(par(c)),T(" − "),squared(par(a))}),T("2"+par(b)+par(c))));
        w.rows<<eq("cos A",T(nfmt(ratio)));w.rows<<eq("A",join({power("cos",-1),T("("+nfmt(ratio)+")")}));w.rows<<text("A ≈ "+dec(qRadiansToDegrees(qAcos(qBound(-1.0,ratio,1.0))))+"°");
    }
    return w;
}
} // namespace
namespace UBMathFormula {
QStringList kinds(){return {"quadratic","discriminant","pythagoras","distance","slope","sine","cosine-side","cosine-angle","quotient","derivative"};}
QString title(const QString &k){static const QMap<QString,QString> t={{"quadratic","Quadratic formula"},{"discriminant","Discriminant"},{"pythagoras","Pythagorean theorem"},{"distance","Distance formula"},{"slope","Slope formula"},{"sine","Sine law"},{"cosine-side","Cosine law · side"},{"cosine-angle","Cosine law · angle"},{"quotient","Difference quotient"},{"derivative","Derivative definition"}};return t.value(k,"Formula");}
QStringList modes(const State &s){if(s.kind=="pythagoras")return {"Find hypotenuse c","Find leg a","Find leg b"};if(s.kind=="sine")return {"Find side b","Find angle B (SSA)"};return {};}
QStringList labels(const State &s){
    if(s.kind=="quadratic"||s.kind=="discriminant")return {"a","b","c"};
    if(s.kind=="pythagoras")return s.mode==0?QStringList{"a","b"}:s.mode==1?QStringList{"c","b"}:QStringList{"c","a"};
    if(s.kind=="distance"||s.kind=="slope")return {"x₁","y₁","x₂","y₂"};
    if(s.kind=="sine")return s.mode==0?QStringList{"a","A (°)","B (°)"}:QStringList{"a","A (°)","b"};
    if(s.kind=="cosine-side")return {"b","c","A (°)"};
    if(s.kind=="cosine-angle")return {"a","b","c"};
    return {"f(x)"};
}
bool pointInputs(const State &s){return s.kind=="distance"||s.kind=="slope";}
QRectF fieldRect(const State &s,int index){return pointInputs(s)?QRectF(190+(index%2)*128,(index/2)*52,112,40):QRectF(index*144,0,labels(s).size()==1?480:132,40);}
QRectF inputRect(const State &s){const bool function=s.kind=="quotient"||s.kind=="derivative";return QRectF(28,56,pointInputs(s)||function?480:labels(s).size()*144-12,pointInputs(s)?92:modes(s).isEmpty()?40:84);}
bool read(const QByteArray &svg,State *state){
    if(!state)return false;
    if(svg.contains("mathscrap-formula")){
        QDomDocument doc;if(!doc.setContent(svg))return false;const auto list=doc.elementsByTagName("metadata");
        for(int i=0;i<list.size();++i){const auto e=list.at(i).toElement();if(e.attribute("id")!="mathscrap-formula")continue;
            const auto json=QJsonDocument::fromJson(QByteArray::fromBase64(e.text().toLatin1()));if(!json.isObject())return false;const auto o=json.object();state->kind=o["kind"].toString();if(!kinds().contains(state->kind))return false;
            state->values.clear();for(const auto &v:o["values"].toArray())state->values<<v.toString().left(180);state->step=qBound(0,o["step"].toInt(),12);state->mode=qBound(0,o["mode"].toInt(),2);return true;
        }return false;
    }
    UBQuadraticFormula::State old;if(UBQuadraticFormula::read(svg,&old)){state->kind="quadratic";state->values={num(old.a),num(old.b),num(old.c)};state->step=old.step;state->mode=0;return true;}return false;
}
int lastStep(const State &state){const auto w=build(state);return w.message.isEmpty()?qMax(0,int(w.rows.size())-1):0;}
QString error(const State &state){return build(state).message;}
QByteArray render(const State &s){
    const auto w=build(s);const int stage=qBound(0,s.step,w.message.isEmpty()?qMax(0,int(w.rows.size())-1):0);QList<Math> rows;
    for(int i=0;i<=stage&&i<w.rows.size();++i)rows<<w.rows[i];
    if(!w.message.isEmpty())rows<<text(w.message,18,true);
    const auto inputs=inputRect(s);qreal width=qMax(580.0,inputs.right()+28),height=inputs.bottom()+22;
    for(const auto &row:rows){width=qMax(width,row.width+64);height+=row.height+22;}height+=66;
    QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);QSvgGenerator generator;generator.setOutputDevice(&buffer);generator.setSize(QSize(qCeil(width),qCeil(height)));generator.setViewBox(QRectF(0,0,width,height));QPainter painter(&generator);painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#ded8cc"),1.2));painter.setBrush(QColor("#fffdf7"));painter.drawRoundedRect(QRectF(1,1,width-2,height-2),12,12);
    auto draw=[&](const Math &m,qreal x,qreal y){painter.fillPath(QTransform::fromTranslate(x,y).map(m.path),QColor("#253e3c"));};draw(text(title(s.kind),20,true),28,20);
    const auto names=labels(s);const bool function=names.size()==1;
    if(pointInputs(s))for(int row=0;row<2;++row){
        const qreal y=inputs.top()+row*52;
        draw(text(row?"B(x₂, y₂) = (":"A(x₁, y₁) = (",22),inputs.left(),y+6);
        draw(text(",",22),inputs.left()+305,y+6);draw(text(")",22),inputs.left()+438,y+6);
    }
    for(int i=0;i<names.size();++i){
        const auto box=fieldRect(s,i).translated(inputs.topLeft());
        painter.setPen(Qt::NoPen);painter.setBrush(QColor("#f1eee5"));painter.drawRoundedRect(box,7,7);
        draw(text((pointInputs(s)?QString():names[i]+" = ")+s.values.value(i),function?18:22),box.left()+10,box.top()+6);
    }
    if(!modes(s).isEmpty())draw(text(modes(s).value(s.mode),18,true),28,108);
    qreal y=inputs.bottom()+22;for(const auto &row:rows){draw(row,32,y);y+=row.height+22;}
    const qreal by=height-54;for(int i=0;i<2;++i){bool enabled=i?(w.message.isEmpty()&&stage<w.rows.size()-1):stage>0;qreal x=width-280+i*134;painter.setPen(Qt::NoPen);painter.setBrush(QColor(enabled?"#dceae4":"#f0eee7"));painter.drawRoundedRect(QRectF(x,by,122,36),7,7);const auto label=text(i?"Next step":"Previous",17,true);painter.fillPath(QTransform::fromTranslate(x+(122-label.width)/2,by+(36-label.height)/2).map(label.path),QColor(enabled?"#253e3c":"#92978f"));}painter.end();buffer.close();
    QDomDocument doc;if(!doc.setContent(bytes))return {};auto root=doc.documentElement();root.setAttribute("width",QString::number(width,'g',12));root.setAttribute("height",QString::number(height,'g',12));
    QJsonArray values;for(const auto &v:s.values)values.append(v);const QJsonObject state{{"kind",s.kind},{"values",values},{"step",stage},{"mode",s.mode}};auto metadata=doc.createElement("metadata");metadata.setAttribute("id","mathscrap-formula");metadata.appendChild(doc.createTextNode(QString::fromLatin1(QJsonDocument(state).toJson(QJsonDocument::Compact).toBase64())));root.insertBefore(metadata,root.firstChild());for(auto node=root.firstChild();!node.isNull();){auto next=node.nextSibling();if(node.toElement().tagName()=="desc")root.removeChild(node);node=next;}auto desc=doc.createElement("desc");desc.appendChild(doc.createTextNode(title(s.kind)+"; "+s.values.join(", ")+"; "+w.message+"; "+[&rows]{QStringList spoken;for(const auto &row:rows)spoken<<row.spoken;return spoken.join("; ");}()+"; "+w.semantic.join("; ")));root.appendChild(desc);return doc.toByteArray();
}
int stepAt(const QByteArray &svg,qreal x,qreal y){
    State s;if(!read(svg,&s))return -1;if(!svg.contains("mathscrap-formula"))return UBQuadraticFormula::stepAt(svg,x,y);
    QDomDocument doc;if(!doc.setContent(svg))return -1;const auto v=doc.documentElement().attribute("viewBox").split(QRegularExpression("[ ,]+"),Qt::SkipEmptyParts);if(v.size()!=4)return -1;const qreal width=v[2].toDouble(),height=v[3].toDouble();if(y<height-54||y>height-18)return -1;if(x>=width-280&&x<=width-158)return 0;if(x>=width-146&&x<=width-24)return 1;return -1;
}
}
