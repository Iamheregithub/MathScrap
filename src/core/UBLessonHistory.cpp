// SPDX-License-Identifier: GPL-3.0-only
#include "UBLessonHistory.h"
#include <QtWidgets>
#include "UBApplication.h"
#include "UBSettings.h"
#include "UBPersistenceManager.h"
#include "adaptors/UBSvgSubsetAdaptor.h"
#include "adaptors/UBMetadataDcSubsetAdaptor.h"
#include "board/UBBoardController.h"
#include "document/UBDocumentProxy.h"
#include "domain/UBGraphicsScene.h"
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
bool atomicCopy(const QString &from,const QString &to) {
    QFile input(from);QSaveFile output(to);
    if(!input.open(QIODevice::ReadOnly)||!output.open(QIODevice::WriteOnly))return false;
    while(!input.atEnd()){auto bytes=input.read(1024*1024);if(bytes.isEmpty()&&input.error()!=QFile::NoError)return false;if(output.write(bytes)!=bytes.size())return false;}
    return output.commit();
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
    for(const auto &id:previous.value("documents").toArray())mTouched.insert(id.toString());
    saveSession(false);
    if(!mTesting){auto timer=new QTimer(this);timer->setInterval(20000);connect(timer,&QTimer::timeout,this,[this]{
        if(!QApplication::activeModalWidget()&&QApplication::mouseButtons()==Qt::NoButton&&UBApplication::boardController->activeScene()&&!UBApplication::boardController->activeScene()->isInputDevicePressed()){QString error;if(!checkpoint(false,&error)&&!error.isEmpty()&&error!=mError){mError=error;UBApplication::showMessage(tr("Recovery save failed: %1").arg(error));}}
    });timer->start();}
    connect(qApp,&QCoreApplication::aboutToQuit,this,[this]{
        QString error;const bool saved=mTesting||checkpoint(true,&error);saveSession(saved);
    });
}
bool UBLessonHistory::saveSession(bool clean){QJsonArray ids;for(const auto &id:mTouched)ids.append(id);QJsonObject o;o["clean"]=clean;o["documents"]=ids;o["time"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);if(!writeObject(mRoot+"/session.json",o)){mError=tr("Cannot save the recovery session marker.");return false;}return true;}
bool UBLessonHistory::checkpoint(bool keepVersion,QString *error) {
    auto fail=[&](const QString &message){if(error)*error=message;return false;};
    if(mBusy)return fail(tr("A recovery save is already running."));
    auto board=UBApplication::boardController;
    if(!board||!board->selectedDocument()||!board->activeScene())return true;
    QScopedValueRollback<bool> busy(mBusy,true);
    auto doc=board->selectedDocument();auto manager=UBPersistenceManager::persistenceManager();
    // Drain all earlier background saves before reading a consistent repository.
    manager->flushPendingWrites();
    board->persistCurrentScene(true,true);
    if(!UBSvgSubsetAdaptor::persistScene(doc,board->activeScene(),board->activeSceneIndex())||!UBMetadataDcSubsetAdaptor::persist(doc))return fail(tr("The current lesson could not be saved. The previous recovery copy was kept."));
    const QString id=doc->uuid().toString(QUuid::WithoutBraces);
    if(!validId(id))return fail(tr("The lesson has no valid identifier."));
    const QString folder=mRoot+"/"+id, blobs=folder+"/blobs";
    if(!QDir().mkpath(blobs)||!QDir().mkpath(folder+"/versions"))return fail(tr("Cannot create the lesson history folder."));
    const QDir source(doc->persistencePath());QDirIterator iterator(source.absolutePath(),QDir::Files|QDir::Hidden|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
    QStringList paths;while(iterator.hasNext()){iterator.next();if(iterator.fileInfo().isSymLink())return fail(tr("Linked files cannot be included in recovery copies."));if(!iterator.fileName().contains(".thumbnail.")&&!iterator.fileName().startsWith('.'))paths<<source.relativeFilePath(iterator.filePath());}paths.sort();
    QJsonArray entries;QCryptographicHash content(QCryptographicHash::Sha256);
    content.addData(doc->name().toUtf8());
    for(const auto &relative:paths){const QString path=source.filePath(relative);const QFileInfo info(path);
        const QString fingerprint=QString("%1|%2|%3").arg(path).arg(info.size()).arg(info.lastModified().toMSecsSinceEpoch());
        const bool immutableAsset=relative.endsWith(".pdf",Qt::CaseInsensitive)||relative.endsWith(".png",Qt::CaseInsensitive)||relative.endsWith(".jpg",Qt::CaseInsensitive);
        QString hash=immutableAsset?mFingerprints.value(fingerprint):QString();if(hash.isEmpty())hash=shaFile(path);
        if(hash.isEmpty())return fail(tr("Cannot read %1.").arg(relative));
        if(!QFile::exists(blobs+"/"+hash)){if(!atomicCopy(path,blobs+"/"+hash)||shaFile(blobs+"/"+hash)!=hash)return fail(tr("Could not verify the recovery copy of %1.").arg(relative));}
        mFingerprints.insert(fingerprint,hash);
        QJsonObject entry;entry["path"]=relative;entry["sha256"]=hash;entries.append(entry);
        // Metadata contains a save timestamp. It is retained, but does not create redundant versions.
        if(relative!="metadata.rdf"){content.addData(relative.toUtf8());content.addData(hash.toLatin1());}
    }
    const QString digest=QString::fromLatin1(content.result().toHex());auto previous=readObject(folder+"/recovery.json");
    QJsonObject snapshot;snapshot["name"]=doc->name();snapshot["id"]=id;snapshot["created"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);snapshot["digest"]=digest;snapshot["files"]=entries;
    const auto oldVersions=versions(id);QJsonObject last;
    for(const auto &v:oldVersions)if(v.toObject().value("file")!="recovery.json"){last=v.toObject();break;}
    const bool changed=digest!=previous.value("digest").toString();
    const bool versionChanged=digest!=last.value("digest").toString();
    if(versionChanged&&(keepVersion||last.isEmpty()||QDateTime::fromString(last.value("created").toString(),Qt::ISODateWithMs).secsTo(QDateTime::currentDateTimeUtc())>=300)){
        const QString name=QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+"-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".json";
        if(!writeObject(folder+"/versions/"+name,snapshot))return fail(tr("Cannot commit the saved version."));
    }
    if(changed||previous.isEmpty())if(!writeObject(folder+"/recovery.json",snapshot))return fail(tr("Cannot commit the recovery copy."));
    mTouched.insert(id);if(!saveSession(false))return fail(mError);prune(id);mError.clear();return true;
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
void UBLessonHistory::prune(const QString &id) {
    const QString folder=mRoot+"/"+id;QDir dir(folder+"/versions");auto names=dir.entryList({"*.json"},QDir::Files,QDir::Name|QDir::Reversed);
    for(int i=50;i<names.size();++i)QFile::remove(dir.filePath(names[i]));
    QSet<QString> needed;for(const auto &v:versions(id))for(const auto &f:v.toObject().value("files").toArray())needed.insert(f.toObject().value("sha256").toString());
    QDir blobs(folder+"/blobs");for(const auto &name:blobs.entryList(QDir::Files))if(QRegularExpression("^[a-f0-9]{64}$").match(name).hasMatch()&&!needed.contains(name))QFile::remove(blobs.filePath(name));
    if(mFingerprints.size()>50000)mFingerprints.clear();
}
std::shared_ptr<UBDocumentProxy> UBLessonHistory::restoreCopy(const QString &id,const QString &version,QString *error) {
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
