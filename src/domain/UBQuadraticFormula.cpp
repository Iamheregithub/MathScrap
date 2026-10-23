// SPDX-License-Identifier: GPL-3.0-only
#include "UBQuadraticFormula.h"
#include <QDomDocument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtMath>
#include <climits>
#include <numeric>

namespace {
QString num(int n) { return QString::number(n); }
QString wrapped(int n) { return n < 0 ? QString("(%1)").arg(n) : num(n); }
QString escaped(const QString &s) { return s.toHtmlEscaped(); }
QString formatted(double x) { return QString::number(x, 'f', 3).remove(QRegularExpression("0+$")).remove(QRegularExpression("\\.$")); }
QString line(const QString &s, int y, int size = 27) {
    return QString("<text x='46' y='%1' fill='#253e3c' font-family='DejaVu Sans, Arial' font-size='%2'>%3</text>")
        .arg(y).arg(size).arg(escaped(s));
}
QString fraction(const QString &top, const QString &bottom, int y) {
    return line("x =",y+9,25)
        + QString("<text x='110' y='%1' fill='#253e3c' font-family='DejaVu Sans, Arial' font-size='24'>%2</text>")
            .arg(y-10).arg(escaped(top))
        + QString("<path d='M108 %1 H710' stroke='#253e3c' stroke-width='2'/>").arg(y)
        + QString("<text x='410' y='%1' text-anchor='middle' fill='#253e3c' font-family='DejaVu Sans, Arial' font-size='23'>%2</text>")
            .arg(y+30).arg(escaped(bottom));
}
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
    QStringList rows;
    rows << fraction("−b ± √(b² − 4ac)","2a",123);
    if (stage>=1) rows << fraction(QString("−%1 ± √(%2² − 4%3%4)")
        .arg(wrapped(s.b),wrapped(s.b),wrapped(s.a),wrapped(s.c)),"2"+wrapped(s.a),189);
    if (stage>=2) rows << fraction(QString("%1 ± √(%2 − %3)").arg(-s.b).arg(qint64(s.b)*s.b).arg(4*qint64(s.a)*s.c),num(2*s.a),255);
    if (stage>=3) rows << fraction(QString("%1 ± √%2").arg(-s.b).arg(wrapped(d)),num(2*s.a),321);
    if (stage>=4) rows << line(exact(s.a,s.b,d),387,25);
    if (stage>=5) {
        if (d>0) rows << QString("x₁ ≈ %1     x₂ ≈ %2").arg(formatted((-s.b+qSqrt(d))/(2.0*s.a)), formatted((-s.b-qSqrt(d))/(2.0*s.a)));
        else if (!d) rows << QString("One repeated root:  x = %1").arg(formatted(-s.b/(2.0*s.a)));
        else rows << QString("No real roots; complex roots:  x = (%1 ± i√%2) / %3").arg(-s.b).arg(-d).arg(2*s.a);
    }
    const int height=165+rows.size()*66;
    const QJsonObject state{{"a",s.a},{"b",s.b},{"c",s.c},{"step",stage}};
    const QString metadata=QString::fromLatin1(QJsonDocument(state).toJson(QJsonDocument::Compact).toBase64());
    QString svg=QString("<svg xmlns='http://www.w3.org/2000/svg' width='760' height='%1' viewBox='0 0 760 %1'><metadata id='lessonboard-quadratic'>%2</metadata><rect x='2' y='2' width='756' height='%3' rx='16' fill='#fffdf7' stroke='#c6c0b1' stroke-width='3'/>")
        .arg(height).arg(metadata).arg(height-4);
    svg += line("Quadratic formula",45,25);
    svg += line(QString("a = %1     b = %2     c = %3").arg(s.a).arg(s.b).arg(s.c),83,23);
    for (int i=0; i<rows.size(); ++i) svg += i<4 ? rows[i] : line(rows[i],123+i*66, i==5?23:25);
    const int y=height-52;
    svg += QString("<rect x='454' y='%1' width='118' height='38' rx='8' fill='%2'/><text x='513' y='%3' text-anchor='middle' fill='%4' font-family='DejaVu Sans, Arial' font-size='19'>Previous</text>")
        .arg(y).arg(stage?"#dceae4":"#eeeae1").arg(y+26).arg(stage?"#253e3c":"#9a9a96");
    svg += QString("<rect x='585' y='%1' width='128' height='38' rx='8' fill='%2'/><text x='649' y='%3' text-anchor='middle' fill='%4' font-family='DejaVu Sans, Arial' font-size='19'>Next step</text></svg>")
        .arg(y).arg(stage<5?"#dceae4":"#eeeae1").arg(y+26).arg(stage<5?"#253e3c":"#9a9a96");
    return svg.toUtf8();
}

int stepAt(const QByteArray &svg, double x, double y) {
    State state; if (!read(svg,&state)) return -1;
    const int height=165+(state.step+1)*66;
    if (y<height-52 || y>height-14) return -1;
    if (x>=454 && x<=572) return 0;
    if (x>=585 && x<=713) return 1;
    return -1;
}
}
