// SPDX-License-Identifier: GPL-3.0-only
#ifndef UBQUADRATICFORMULA_H
#define UBQUADRATICFORMULA_H

#include <QByteArray>

namespace UBQuadraticFormula {
struct State { int a = 1, b = 0, c = 0, step = 0; };
bool read(const QByteArray &svg, State *state);
QByteArray render(const State &state);
int stepAt(const QByteArray &svg, double x, double y); // -1 outside controls, 0 previous, 1 next
}

#endif
