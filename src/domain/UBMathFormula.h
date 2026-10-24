// SPDX-License-Identifier: GPL-3.0-only
#ifndef UBMATHFORMULA_H
#define UBMATHFORMULA_H
#include <QByteArray>
#include <QStringList>
#include <QRectF>
namespace UBMathFormula {
struct State { QString kind="quadratic"; QStringList values; int step=0; int mode=0; bool tapToReveal=false; };
QStringList kinds();
QString title(const QString &kind);
QStringList labels(const State &state);
QStringList modes(const State &state);
bool read(const QByteArray &svg, State *state);
QByteArray render(const State &state);
int lastStep(const State &state);
QString error(const State &state);
int stepAt(const QByteArray &svg,qreal x,qreal y);
QRectF inputRect(const State &state);
bool pointInputs(const State &state);
QRectF fieldRect(const State &state,int index);
}
#endif
