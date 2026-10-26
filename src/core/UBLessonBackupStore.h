// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>

// Value-only input: the worker never touches a scene, widget, document proxy,
// settings singleton, or the history controller's mutable state.
struct UBLessonBackupJob {
    QString root, source, id, name, clearedDigest;
    bool keepVersion = false, force = false, recordDeletion = false;
};
struct UBLessonBackupResult {
    QString id, digest, error;
    bool success = false, suppressed = false, sourceChanged = false;
};
namespace UBLessonBackupStore {
    UBLessonBackupResult save(const UBLessonBackupJob &job);
}
