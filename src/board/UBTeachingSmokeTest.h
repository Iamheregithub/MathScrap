// SPDX-License-Identifier: GPL-3.0-only
#ifndef UBTEACHINGSMOKETEST_H
#define UBTEACHINGSMOKETEST_H
class QObject;
void installTeachingCrashDiagnostics();
void scheduleTeachingSmokeTest(QObject *context);
#endif
