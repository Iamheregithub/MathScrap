// SPDX-License-Identifier: GPL-3.0-only
#include "UBLessonHistory.h"
#include <QtWidgets>
#include <QtConcurrent>
#include "UBInputActivity.h"
#include "UBApplication.h"
#include "UBSettings.h"
#include "UBPersistenceManager.h"
#include "adaptors/UBSvgSubsetAdaptor.h"
#include "adaptors/UBMetadataDcSubsetAdaptor.h"
#include "board/UBBoardController.h"
#include "document/UBDocumentProxy.h"
#include "domain/UBGraphicsScene.h"
#include "frameworks/UBFileSystemUtils.h"
namespace {
QJsonObject readObject(const QString &path) {
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}
bool writeObject(const QString &path, const QJsonObject &value) {
    QSaveFile f(path);if(!f.open(QIODevice::WriteOnly))return false;
    const auto bytes=QJsonDocument(value).toJson();return f.write(bytes)==bytes.size() && f.commit();
}
QString shaFile(const QString &path) {
    QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};
    QCryptographicHash h(QCryptographicHash::Sha256);if(!h.addData(&f))return {};
    return QString::fromLatin1(h.result().toHex());
}
bool validId(const QString &id){return QRegularExpression("^[0-9a-fA-F-]{36}$").match(id).hasMatch();}
}
UBLessonHistory::UBLessonHistory(QObject *parent):QObject(parent) {
    setObjectName("lessonHistory");const auto args=qApp->arguments();
    mDisabled=args.contains("--teaching-smoke-test")||args.contains("--file-compatibility-test");
    mTesting=args.contains("--lesson-tools-test");
    mRoot=UBSettings::userDataDirectory()+"/lesson-history";
    if(mTesting&&!qEnvironmentVariable("LESSONBOARD_HISTORY_TEST_ROOT").isEmpty())mRoot=qEnvironmentVariable("LESSONBOARD_HISTORY_TEST_ROOT");
    if(mDisabled)return;
    QDir().mkpath(mRoot);
    const auto previous=readObject(mRoot+"/session.json");mUnclean=!previous.isEmpty()&&!previous.value("clean").toBool();
    const auto notice=readObject(mRoot+"/recovery-notice.json");
    const auto cleared=notice.value("cleared").toObject();for(auto it=cleared.begin();it!=cleared.end();++it)if(validId(it.key()))mClearedDigests.insert(it.key(),it.value().toString());
    if(mUnclean||!previous.value("checkpointError").toString().isEmpty()){
        QCryptographicHash key(QCryptographicHash::Sha256);int copies=0;
        for(const auto &value:documents()){
            const auto info=value.toObject();const auto snapshot=readObject(mRoot+"/"+info.value("id").toString()+"/recovery.json");
            if(snapshot.value("files").toArray().isEmpty())continue;
            key.addData(info.value("id").toString().toUtf8());key.addData(QByteArray(1,'\0'));
            key.addData(snapshot.value("digest").toString().isEmpty()?QJsonDocument(snapshot).toJson(QJsonDocument::Compact):snapshot.value("digest").toString().toLatin1());++copies;
        }
        if(copies)mRecoveryNoticeKey=QString::fromLatin1(key.result().toHex());
        mRecoveryAcknowledged=!mRecoveryNoticeKey.isEmpty()&&notice.value("acknowledged").toString()==mRecoveryNoticeKey;
    }
    for(const auto &id:previous.value("documents").toArray())mTouched.insert(id.toString());
    saveSession(false);
    UBInputActivity::instance();
    connect(&mDrainWatcher,&QFutureWatcher<void>::finished,this,[this]{
        if(!mDraining || !mDrainWatcher.isFinished())return;
        mDraining=false;
        // The user may have resumed writing while the worker drained.
        if(!readyForAutomaticSave())return;
        QString error;UBLessonBackupJob job;
        if(!prepareSnapshot(job,&error)){
            if(!error.isEmpty())qWarning()<<"Recovery preparation failed:"<<error;
            return;
        }
        mAutomaticJob=job;mBackingUp=true;mAutomaticDue=false;
        mBackupWatcher.setFuture(QtConcurrent::run([job]{return UBLessonBackupStore::save(job);}));
    });
    connect(&mBackupWatcher,&QFutureWatcher<UBLessonBackupResult>::finished,this,[this]{
        if(mBackingUp && mBackupWatcher.isFinished())finishAutomaticSave();
    });
    if(!mTesting||qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_MODE")=="crash"||qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_MODE")=="save-write"){
        auto interval=new QTimer(this);interval->setInterval(20000);
        connect(interval,&QTimer::timeout,this,[this]{mAutomaticDue=true;startAutomaticCheckpoint();});interval->start();
        auto retry=new QTimer(this);retry->setInterval(500);
        connect(retry,&QTimer::timeout,this,[this]{if(mAutomaticDue)startAutomaticCheckpoint();});retry->start();
    }
    connect(qApp,&QCoreApplication::aboutToQuit,this,[this]{
        // The save-lifecycle fixture exercises the same final checkpoint as a
        // normal lesson session, while the older tool fixtures remain isolated.
        const bool exerciseNormalExit=qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_MODE")=="save-write"||qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_MODE")=="recovery-dismiss"||qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_MODE")=="recovery-delete"||qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_MODE")=="save-checkpoint-failure";
        QString error;const bool saved=(mTesting&&!exerciseNormalExit)||checkpoint(true,&error);
        // A failed backup is not a crashed process. Preserve its error separately.
        if(!saved)qWarning()<<"Recovery checkpoint failed during orderly exit:"<<error;
        saveSession(true,saved?QString():error);
    });
}
bool UBLessonHistory::saveSession(bool clean,const QString &checkpointError){QJsonArray ids;for(const auto &id:mTouched)ids.append(id);QJsonObject o;o["clean"]=clean;if(!checkpointError.isEmpty())o["checkpointError"]=checkpointError;o["documents"]=ids;o["time"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);if(!writeObject(mRoot+"/session.json",o)){mError=tr("Cannot save the recovery session marker.");return false;}return true;}
bool UBLessonHistory::checkpoint(bool keepVersion,QString *error,bool force) {
    return saveSnapshot(keepVersion,error,force,false);
}
UBLessonHistory::~UBLessonHistory()
{
    finishAutomaticSave();
}

bool UBLessonHistory::readyForAutomaticSave() const
{
    const auto board=UBApplication::boardController;
    return board && board->selectedDocument() && board->activeScene()
        && !QApplication::activeModalWidget() && !QApplication::activePopupWidget()
        && !board->activeScene()->isInputDevicePressed() && UBInputActivity::instance()->isIdle();
}

void UBLessonHistory::startAutomaticCheckpoint()
{
    if(mDisabled||mBusy||mDraining||mBackingUp||!readyForAutomaticSave())return;
    // Wait for the serialization queue on a worker, never on the GUI thread.
    // Only the completion callback may read the live scene.
    mDraining=true;
    auto manager=UBPersistenceManager::persistenceManager();
    mDrainWatcher.setFuture(QtConcurrent::run([manager]{manager->flushPendingWrites();}));
}

void UBLessonHistory::finishAutomaticSave()
{
    if(mDraining){
        mDrainWatcher.waitForFinished();
        mDraining=false; // Also invalidates an already queued finished callback.
    }
    if(!mBackingUp)return;
    mBackupWatcher.waitForFinished();
    mBackingUp=false;
    const auto result=mBackupWatcher.result();
    QString error;
    if(!applyBackupResult(mAutomaticJob,result,&error)){
        mAutomaticDue=true;
        if(!result.sourceChanged && !error.isEmpty())qWarning()<<"Recovery save failed:"<<error;
    }
}

bool UBLessonHistory::prepareSnapshot(UBLessonBackupJob &job,QString *error)
{
    auto board=UBApplication::boardController;
    if(!board||!board->selectedDocument()||!board->activeScene())return false;
    QElapsedTimer elapsed;elapsed.start();
    auto doc=board->selectedDocument();auto scene=board->activeScene();
    job.root=mRoot;job.source=doc->persistencePath();job.name=doc->name();
    job.id=doc->uuid().toString(QUuid::WithoutBraces);job.clearedDigest=mClearedDigests.value(job.id);
    const QString pagePath=job.source+UBFileSystemUtils::digitFileFormat("/page%1.svg",board->activeSceneIndex());
    const bool savePage=scene->isModified()||!QFile::exists(pagePath);
    // Serialize the GUI-owned scene exactly once. The expensive repository
    // scan, hashing, copy verification and pruning happen on the worker.
    scene->saveWidgetSnapshots();
    const bool saved=(!savePage||UBSvgSubsetAdaptor::persistScene(doc,scene,board->activeSceneIndex()))
        && QFile::exists(pagePath) && UBMetadataDcSubsetAdaptor::persist(doc);
    // Leave the scene's modified flag intact: normal persistence still needs
    // to refresh its on-disk thumbnail. This path intentionally skips rendering
    // a thumbnail on the handwriting thread.
    if(elapsed.elapsed()>100)qWarning()<<"mathscrap.performance recovery preparation ms:"<<elapsed.elapsed();
    if(!saved){if(error)*error=tr("The current lesson could not be saved. The previous recovery copy was kept.");return false;}
    return true;
}

bool UBLessonHistory::applyBackupResult(const UBLessonBackupJob &job,const UBLessonBackupResult &result,QString *error)
{
    if(!result.success){if(error)*error=result.error;return false;}
    if(job.recordDeletion){mClearedDigests.insert(job.id,result.digest);mError.clear();return true;}
    if(result.suppressed){mError.clear();return true;}
    if(mClearedDigests.remove(job.id)&&!saveRecoveryNotice(error))return false;
    mTouched.insert(job.id);
    if(!saveSession(false)){if(error)*error=mError;return false;}
    mError.clear();return true;
}

bool UBLessonHistory::saveSnapshot(bool keepVersion,QString *error,bool force,bool recordDeletion)
{
    if(mBusy){if(error)*error=tr("A recovery save is already running.");return false;}
    finishAutomaticSave();
    auto board=UBApplication::boardController;
    if(!board||!board->selectedDocument()||!board->activeScene())return true;
    QScopedValueRollback<bool> busy(mBusy,true);
    // Explicit saves, deletion, and shutdown must finish before returning.
    UBPersistenceManager::persistenceManager()->flushPendingWrites();
    UBLessonBackupJob job;
    if(!prepareSnapshot(job,error))return false;
    job.keepVersion=keepVersion;job.force=force;job.recordDeletion=recordDeletion;
    return applyBackupResult(job,UBLessonBackupStore::save(job),error);
}
bool UBLessonHistory::saveRecoveryNotice(QString *error)
{
    QJsonObject cleared;for(auto it=mClearedDigests.begin();it!=mClearedDigests.end();++it)cleared[it.key()]=it.value();
    if(writeObject(mRoot+"/recovery-notice.json",{{"acknowledged",mRecoveryNoticeKey},{"cleared",cleared}}))return true;
    mError=tr("Cannot remember the recovery choice. Check that the lesson history folder is writable.");if(error)*error=mError;return false;
}
bool UBLessonHistory::acknowledgeRecoveryPrompt(QString *error)
{
    if(!saveRecoveryNotice(error))return false;mRecoveryAcknowledged=true;return true;
}
bool UBLessonHistory::clearHistory(QString *error)
{
    finishAutomaticSave();
    auto fail=[&](const QString &message){mError=message;if(error)*error=message;return false;};
    const QDir root(mRoot);const auto live=QDir(UBSettings::userDocumentDirectory()).canonicalPath();
    if(!live.isEmpty()&&root.canonicalPath()==live)return fail(tr("The history folder cannot be the Documents folder."));
    const auto folders=root.entryList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name);
    for(const auto &id:folders)if(validId(id)){
        const auto digest=readObject(root.filePath(id+"/recovery.json")).value("digest").toString();if(!digest.isEmpty())mClearedDigests.insert(id,digest);
    }
    // Record current content without creating another backup. Deletion must
    // also work when the recovery storage itself is damaged or unwritable.
    if(!saveSnapshot(false,error,false,true))return false;
    if(!saveRecoveryNotice(error))return false;
    for(const auto &id:folders)if(validId(id)){
        const QString path=root.filePath(id);
        if(!(QFileInfo(path).isSymLink()?QFile::remove(path):QDir(path).removeRecursively()))return fail(tr("Cannot remove all recovery history. Your lessons have not been deleted."));
    }
    mTouched.clear();mRecoveryAcknowledged=true;mError.clear();
    if(!saveSession(false)){if(error)*error=mError;return false;}return true;
}
QJsonArray UBLessonHistory::documents() const {
    QJsonArray result;const QDir dir(mRoot);for(const auto &id:dir.entryList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name))if(validId(id)){
        auto o=readObject(dir.filePath(id+"/recovery.json"));if(!o.isEmpty()){QJsonObject info;info["id"]=id;info["name"]=o.value("name");result.append(info);}
    }return result;
}
QJsonArray UBLessonHistory::versions(const QString &id) const {
    QJsonArray result;if(!validId(id))return result;
    const QString folder=mRoot+"/"+id;auto latest=readObject(folder+"/recovery.json");if(!latest.isEmpty()){latest["file"]="recovery.json";result.append(latest);}
    QDir versions(folder+"/versions");for(const auto &name:versions.entryList({"*.json"},QDir::Files,QDir::Name|QDir::Reversed)){
        auto entry=readObject(versions.filePath(name));if(!entry.isEmpty()){entry["file"]="versions/"+name;result.append(entry);}
    }return result;
}
std::shared_ptr<UBDocumentProxy> UBLessonHistory::restoreCopy(const QString &id,const QString &version,QString *error) {
    finishAutomaticSave();
    auto fail=[&](const QString &message)->std::shared_ptr<UBDocumentProxy>{if(error)*error=message;return nullptr;};
    if(!validId(id)||!(version=="recovery.json"||QRegularExpression("^versions/[a-f0-9-]+\\.json$").match(version).hasMatch()))return fail(tr("Invalid recovery selection."));
    const QString folder=mRoot+"/"+id;const auto snapshot=readObject(folder+"/"+version);const auto files=snapshot.value("files").toArray();
    if(files.isEmpty())return fail(tr("This version has no readable files."));
    QTemporaryDir stage(UBSettings::userDocumentDirectory()+"/.LessonBoard-restore-XXXXXX");if(!stage.isValid())return fail(tr("Cannot create a recovery working folder."));
    bool metadata=false,page=false;
    for(const auto &value:files){auto f=value.toObject();const QString relative=f.value("path").toString(),hash=f.value("sha256").toString();
        if(relative.isEmpty()||QDir::isAbsolutePath(relative)||relative.contains('\\')||relative.split('/').contains("..")||relative.contains(':')||!QRegularExpression("^[a-f0-9]{64}$").match(hash).hasMatch())return fail(tr("Invalid path in the saved version."));
        const QString blob=folder+"/blobs/"+hash;if(shaFile(blob)!=hash)return fail(tr("A recovery file is missing or damaged. The current lesson was not changed."));
        const QString destination=stage.filePath(relative);QDir().mkpath(QFileInfo(destination).absolutePath());if(!QFile::copy(blob,destination))return fail(tr("Cannot copy a saved lesson file."));
        metadata|=relative=="metadata.rdf";page|=QRegularExpression("^page[0-9]+\\.svg$").match(relative).hasMatch();
    }
    if(!metadata||!page)return fail(tr("This version is missing lesson metadata or pages."));
    auto manager=UBPersistenceManager::persistenceManager();const QString target=manager->generateUniqueDocumentPath();
    if(!QDir().rename(stage.path(),target))return fail(tr("Cannot activate the recovered copy."));stage.setAutoRemove(false);
    auto doc=manager->createDocumentFromDir(target,UBPersistenceManager::myDocumentsName,tr("Recovered — %1 — %2").arg(snapshot.value("name").toString(),QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm-ss")),false,false,false);
    if(!doc)return fail(tr("Cannot open the recovered copy."));return doc;
}
