// SPDX-License-Identifier: GPL-3.0-only
#include <QtWidgets>
#include <QtConcurrent>
#include <QtTest>
#include "stubs.h"
#define private public
#include "UBLessonHistory.h"
#undef private
#include "UBInputActivity.h"
#include "UBLessonBackupStore.h"
#include <atomic>
#include <thread>

namespace {
int checks=0;
void check(bool ok,const char *description){++checks;if(!ok)qFatal("FAIL: %s",description);}
QByteArray bytes(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
void put(const QString &path,const QByteArray &data){QSaveFile f(path);check(f.open(QIODevice::WriteOnly)&&f.write(data)==data.size()&&f.commit(),"write fixture");}
QJsonObject json(const QString &path){return QJsonDocument::fromJson(bytes(path)).object();}
void settle(UBLessonHistory &history){QElapsedTimer deadline;deadline.start();while((history.mDraining||history.mBackingUp)&&deadline.elapsed()<10000)QTest::qWait(5);check(!history.mDraining&&!history.mBackingUp,"automatic checkpoint completes");}
void activity(QWidget &target){QMouseEvent e(QEvent::MouseButtonRelease,QPointF(1,1),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(&target,&e);}
}
int main(int argc,char **argv){
    QApplication app(argc,argv);
    QTemporaryDir temp;check(temp.isValid(),"temporary directory");Fixture::root=temp.path();
    UBBoardController board;UBApplication::boardController=&board;
    board.doc=std::make_shared<UBDocumentProxy>(temp.path()+"/documents/lesson");QDir().mkpath(board.doc->persistencePath());
    QWidget target;
    UBLessonHistory history;
    QString error;const auto id=board.doc->uuid().toString(QUuid::WithoutBraces);
    const auto folder=history.rootPath()+"/"+id,manifest=folder+"/recovery.json";
    check(history.checkpoint(true,&error),"manual checkpoint");
    check(Fixture::pageWrites==1,"one page serialization per checkpoint");
    const int count=history.versions(id).size();
    board.scene->modified=false;
    check(history.checkpoint(true,&error)&&Fixture::pageWrites==1&&history.versions(id).size()==count,"unchanged page does not rewrite or duplicate version");
    auto restored=history.restoreCopy(id,"recovery.json",&error);
    check(restored&&bytes(restored->persistencePath()+"/page000.svg")=="original writing","restore preserves saved bytes in a separate lesson");
    check(bytes(board.doc->persistencePath()+"/page000.svg")=="original writing","restore leaves live lesson intact");

    activity(target);check(!UBInputActivity::instance()->isIdle(),"input postpones background work");
    history.startAutomaticCheckpoint();check(!history.mDraining,"automatic checkpoint does not begin between letters");
    QTest::qWait(1600);
    board.scene->pressed=true;history.startAutomaticCheckpoint();check(!history.mDraining,"held stroke postpones automatic checkpoint");board.scene->pressed=false;
    Fixture::holdDrain=true;
    QElapsedTimer call;call.start();history.startAutomaticCheckpoint();
    check(call.elapsed()<100&&history.mDraining,"automatic drain returns without blocking GUI");
    check(Fixture::enteredDrain.tryAcquire(1,1000),"worker entered pending-write drain");
    int ticks=0;QTimer heartbeat;heartbeat.setInterval(5);QObject::connect(&heartbeat,&QTimer::timeout,[&]{++ticks;});heartbeat.start();QTest::qWait(50);
    check(ticks>=3,"GUI remains responsive while disk queue is blocked");
    const int before=Fixture::pageWrites;board.scene->modified=true;board.scene->text="writing resumed during drain";activity(target);
    Fixture::holdDrain=false;Fixture::releaseDrain.release();settle(history);
    check(Fixture::pageWrites==before,"resumed writing cancels preparation after drain");
    QTest::qWait(1600);history.startAutomaticCheckpoint();settle(history);
    check(Fixture::pageWrites==before+1,"automatic checkpoint serializes only once");
    check(json(manifest).value("files").toArray().size()==2,"automatic checkpoint publishes readable manifest");

    // A long disk job runs off-thread while input/timers continue on the GUI.
    const auto asset=board.doc->persistencePath()+"/asset.bin";
    put(asset,QByteArray(48*1024*1024,'x'));
    ticks=0;history.startAutomaticCheckpoint();
    QElapsedTimer deadline;deadline.start();while(!history.mBackingUp&&deadline.elapsed()<3000)QTest::qWait(1);
    check(history.mBackingUp,"repository backup launched asynchronously");
    activity(target);QTest::qWait(30);check(ticks>0,"GUI processes input while backup hashes and copies files");
    check(history.clearHistory(&error)&&!QFile::exists(manifest),"Delete all waits for worker and removes its published history");
    QTest::qWait(30);check(!QFile::exists(manifest),"no late completion recreates deleted recovery history");
    check(history.checkpoint(false,&error)&&history.documents().isEmpty(),"unchanged cleared history stays suppressed");
    check(history.checkpoint(true,&error,true)&&QFile::exists(manifest),"explicit saved version resumes cleared history");
    QFile::remove(asset);

    const auto oldManifest=bytes(manifest);Fixture::failPage=true;
    check(!history.checkpoint(true,&error)&&bytes(manifest)==oldManifest,"failed page serialization preserves previous recovery manifest");Fixture::failPage=false;
    check(history.checkpoint(true,&error),"checkpoint retries after serialization failure");
    auto first=json(manifest).value("files").toArray().first().toObject();
    const auto blob=folder+"/blobs/"+first.value("sha256").toString();put(blob,"corrupt blob");
    check(history.checkpoint(true,&error),"backup repairs a corrupted existing blob");
    check(bytes(blob)==bytes(board.doc->persistencePath()+"/"+first.value("path").toString()),"repaired blob matches original content");

    // Continuously rewrite a live page while the worker scans a large asset.
    // A changing repository must not replace its previous recovery manifest.
    put(asset,QByteArray(64*1024*1024,'y'));
    UBLessonBackupJob job;job.root=history.rootPath();job.source=board.doc->persistencePath();job.id=id;job.name=board.doc->name();job.keepVersion=true;
    const auto previous=bytes(manifest);std::atomic<bool> stop{false};
    std::thread writer([&]{int i=0;while(!stop){QSaveFile f(job.source+"/page000.svg");if(f.open(QIODevice::WriteOnly)){f.write(QByteArray::number(++i));f.commit();}std::this_thread::sleep_for(std::chrono::milliseconds(1));}});
    const auto raced=UBLessonBackupStore::save(job);stop=true;writer.join();
    check(!raced.success&&bytes(manifest)==previous,"concurrent source modification retains previous recovery manifest");
    QFile::remove(asset);board.scene->text="final writing survives shutdown";
    for(int i=0;i<53;++i){board.scene->text=QString("version %1").arg(i);check(history.checkpoint(true,&error),"create saved version");}
    check(QDir(folder+"/versions").entryList({"*.json"},QDir::Files).size()==50,"history retains newest fifty saved versions");
    board.scene->text="final writing survives shutdown";
    QMetaObject::invokeMethod(&app,"aboutToQuit",Qt::DirectConnection);
    check(json(history.rootPath()+"/session.json").value("clean").toBool(),"shutdown writes clean session marker");
    restored=history.restoreCopy(id,"recovery.json",&error);
    check(restored&&bytes(restored->persistencePath()+"/page000.svg")=="final writing survives shutdown","shutdown recovery contains final writing");
    qInfo()<<"PASS:"<<checks<<"recovery, responsiveness, failure, deletion, version and shutdown checks";
}
