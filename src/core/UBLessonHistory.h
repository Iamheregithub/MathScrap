// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QSet>
#include <memory>
#include <QFutureWatcher>
#include "UBLessonBackupStore.h"
class UBDocumentProxy;
class UBLessonHistory : public QObject {
    Q_OBJECT
public:
    explicit UBLessonHistory(QObject *parent = nullptr);
    ~UBLessonHistory() override;
    bool checkpoint(bool keepVersion = false, QString *error = nullptr, bool force = false);
    bool recoveryPromptPending() const { return !mRecoveryNoticeKey.isEmpty() && !mRecoveryAcknowledged; }
    bool acknowledgeRecoveryPrompt(QString *error = nullptr);
    bool clearHistory(QString *error = nullptr);
    QJsonArray documents() const;
    QJsonArray versions(const QString &id) const;
    std::shared_ptr<UBDocumentProxy> restoreCopy(const QString &id, const QString &version, QString *error = nullptr);
    bool uncleanSessionDetected() const { return mUnclean; }
    QString rootPath() const { return mRoot; }
    QString lastError() const { return mError; }
private:
    bool saveSnapshot(bool keepVersion, QString *error, bool force, bool recordDeletion);
    bool saveSession(bool clean, const QString &checkpointError = {});
    bool saveRecoveryNotice(QString *error);
    bool prepareSnapshot(UBLessonBackupJob &job, QString *error);
    void startAutomaticCheckpoint();
    void finishAutomaticSave();
    bool applyBackupResult(const UBLessonBackupJob &job, const UBLessonBackupResult &result, QString *error);
    bool readyForAutomaticSave() const;
    QFutureWatcher<void> mDrainWatcher;
    QFutureWatcher<UBLessonBackupResult> mBackupWatcher;
    UBLessonBackupJob mAutomaticJob;
    bool mDraining = false, mBackingUp = false, mAutomaticDue = false;
    QString mRoot, mError, mRecoveryNoticeKey;
    QSet<QString> mTouched;
    QHash<QString, QString> mClearedDigests;
    bool mRecoveryAcknowledged = false;
    bool mUnclean = false, mDisabled = false, mTesting = false, mBusy = false;
};
