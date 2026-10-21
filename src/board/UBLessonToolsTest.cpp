// SPDX-License-Identifier: GPL-3.0-only
#include "UBLessonToolsTest.h"
#include "UBLessonTools.h"
#include <QtWidgets>
#include "core/UBApplication.h"
#include "core/UBSettings.h"
#include "core/UBLessonHistory.h"
#include "core/UBPersistenceManager.h"
#include "gui/UBMainWindow.h"
#include "board/UBBoardController.h"
#include "board/UBDrawingController.h"
#include "document/UBDocumentProxy.h"
#include "domain/UBGraphicsScene.h"
#include "domain/UBGraphicsTextItem.h"
#include "domain/UBGraphicsSvgItem.h"
#include "adaptors/UBSvgSubsetAdaptor.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace {
QJsonObject read(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(f.readAll()).object();}
bool write(const QString &path,const QJsonObject &o){QSaveFile f(path);if(!f.open(QIODevice::WriteOnly))return false;const auto bytes=QJsonDocument(o).toJson();return f.write(bytes)==bytes.size()&&f.commit();}
QString sceneText(std::shared_ptr<UBGraphicsScene> scene){QStringList text;for(auto item:scene->items())if(auto t=dynamic_cast<UBGraphicsTextItem*>(item))text<<t->toPlainText();return text.join("\n");}
void run(){const QString mode=qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_MODE");const QString reportPath=qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_REPORT");const QDir output=QFileInfo(reportPath).dir();QJsonObject report;report["mode"]=mode;
    auto done=[&](bool passed,const QString &detail){report["passed"]=passed;report["detail"]=detail;write(reportPath,report);qApp->exit(passed?0:31);};
    auto tools=UBApplication::mainWindow->findChild<UBLessonTools*>("lessonTools");if(!tools){done(false,"Lesson tools unavailable");return;}auto history=tools->history();auto board=UBApplication::boardController;auto manager=UBPersistenceManager::persistenceManager();QString error;
    if(mode=="exercise"){
        const auto palette=UBSettings::settings()->penColors(false);
        QJsonObject custom;custom["name"]="Test favourite";custom["tool"]=UBStylusTool::Line;custom["color"]="#ff123456";custom["width"]=2;custom["dashed"]=true;
        if(!tools->saveFavourite(0,custom)||!tools->applyFavourite(0)){done(false,"Favourite save/apply failed");return;}auto dc=UBDrawingController::drawingController();
        if(dc->stylusTool()!=UBStylusTool::Line||dc->currentToolColor()!=QColor("#123456")||dc->currentToolWidthIndex()!=2||!UBSettings::settings()->value("Teaching/DashedLines").toBool()||UBSettings::settings()->penColors(false)!=palette){done(false,"Favourite did not apply colour/width/style without altering palette");return;}
        dc->setLineWidthIndex(1);if(dc->currentToolColor()!=QColor("#123456")){done(false,"Changing width lost favourite colour");return;}dc->setColorIndex(0);if(dc->currentToolColor()==QColor("#123456")){done(false,"Palette selection did not clear favourite override");return;}
        auto action=UBApplication::mainWindow->findChild<QAction*>("actionLessonFavourite1");if(!action||action->shortcut()!=QKeySequence("Ctrl+Alt+1")){done(false,"Favourite shortcut missing");return;}
        auto doc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Lesson tools regression",true);board->setActiveDocumentScene(doc,0);auto scene=board->activeScene();scene->setBackground(false,plain);scene->setNominalSize(800,600);
        auto text=scene->addText("Version one",QPointF(-250,-200));auto cover=tools->addCover();auto second=tools->addCover();if(!cover||!second){done(false,"Cover creation failed");return;}cover->setPos(-180,-80);cover->setScale(1);cover->setTransform(QTransform());second->setPos(180,100);second->setScale(.5);second->setRotation(15);tools->setCoverRevealed(second,true);
        dc->setStylusTool(UBStylusTool::Pen);dc->setColorIndex(0);scene->inputDevicePress(QPointF(-80,0));scene->inputDeviceMove(QPointF(80,0));scene->inputDeviceRelease();dc->setStylusTool(UBStylusTool::Selector);scene->clearSelection();
        auto render=[scene]{QImage im(800,600,QImage::Format_ARGB32_Premultiplied);im.fill(Qt::white);QPainter p(&im);scene->render(&p,QRectF(0,0,800,600),QRectF(-400,-300,800,600));p.end();return im;};
        auto covered=render();covered.save(output.filePath("cover-hidden.png"));if(covered.pixelColor(400,300)!=QColor("#334155")){done(false,"Cover did not conceal existing ink");return;}
        const auto before=cover->fileData();tools->setCoverRevealed(cover,true);if(!tools->isCoverRevealed(cover)){done(false,"Cover reveal failed");return;}auto revealed=render();revealed.save(output.filePath("cover-revealed.png"));if(revealed.pixelColor(400,300).lightness()>80){done(false,"Revealing cover did not expose handwriting");return;}
        UBApplication::undoStack->undo();if(cover->fileData()!=before){done(false,"Cover undo failed");return;}UBApplication::undoStack->redo();if(!tools->isCoverRevealed(cover)){done(false,"Cover redo failed");return;}
        if(!history->checkpoint(true,&error)){done(false,error);return;}const QString id=doc->uuid().toString(QUuid::WithoutBraces);auto versions=history->versions(id);QString first;for(const auto &v:versions)if(v.toObject().value("file").toString().startsWith("versions/")){first=v.toObject().value("file").toString();break;}if(first.isEmpty()){done(false,"Initial saved version missing");return;}
        text->setPlainText("Version two");scene->setModified(true);if(!history->checkpoint(true,&error)){done(false,error);return;}const int count=history->versions(id).size();if(count<3){done(false,"Second saved version missing");return;}if(!history->checkpoint(true,&error)||history->versions(id).size()!=count){done(false,"Unchanged lesson created redundant versions");return;}
        auto fresh=UBSvgSubsetAdaptor::loadScene(doc,0);int covers=0,reveals=0;for(auto item:fresh->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover()){++covers;if(tools->isCoverRevealed(svg))++reveals;if(svg->zValue()<=1000000){done(false,"Reopened cover lost its layer");return;}}if(covers!=2||reveals!=2){done(false,"Cover state did not persist across page reload");return;}
        auto restored=history->restoreCopy(id,first,&error);if(!restored||restored==doc||restored->persistencePath()==doc->persistencePath()){done(false,"Restore did not create a separate copy: "+error);return;}if(!sceneText(manager->loadDocumentScene(restored,0)).contains("Version one")||!sceneText(UBSvgSubsetAdaptor::loadScene(doc,0)).contains("Version two")){done(false,"Restore changed the wrong lesson version");return;}
        // A damaged history blob must fail safely without changing the live lesson.
        const auto manifest=read(history->rootPath()+"/"+id+"/"+first);const auto entry=manifest.value("files").toArray().first().toObject();const QString blob=history->rootPath()+"/"+id+"/blobs/"+entry.value("sha256").toString();QFile file(blob);if(!file.open(QIODevice::ReadOnly)){done(false,"History blob missing");return;}const auto bytes=file.readAll();file.close();if(!file.open(QIODevice::WriteOnly|QIODevice::Truncate)){done(false,"Cannot prepare damaged history test");return;}file.write("damaged");file.close();auto rejected=history->restoreCopy(id,first,&error);file.open(QIODevice::WriteOnly|QIODevice::Truncate);file.write(bytes);file.close();if(rejected||!sceneText(UBSvgSubsetAdaptor::loadScene(doc,0)).contains("Version two")){done(false,"Damaged history was not rejected safely");return;}
        report["lessonId"]=id;report["lessonPath"]=doc->persistencePath();report["versions"]=count;report["historyRoot"]=history->rootPath();write(output.filePath("lesson-tools-state.json"),report);
        done(true,"Favourite presets/shortcuts, palette independence, cover occlusion/reveal/undo/reload, history deduplication, separate-copy restore and damaged-blob rejection passed");return;
    }
    if(tools->favourites()[0].toObject().value("name")!="Test favourite"||!tools->applyFavourite(0)||UBDrawingController::drawingController()->currentToolColor()!=QColor("#123456")){done(false,"Favourite did not survive restart");return;}
    if(mode=="crash"){
        auto doc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Crash recovery fixture",true);board->setActiveDocumentScene(doc,0);board->activeScene()->addText("Recovered crash draft",QPointF(0,0));if(!history->checkpoint(false,&error)){done(false,error);return;}report["lessonId"]=doc->uuid().toString(QUuid::WithoutBraces);report["prepared"]=true;write(output.filePath("crash-tools-state.json"),report);write(reportPath,report);
#ifdef Q_OS_WIN
        TerminateProcess(GetCurrentProcess(),89);
#else
        std::_Exit(89);
#endif
        return;
    }
    if(mode=="recover"){
        if(!history->uncleanSessionDetected()){done(false,"Unclean shutdown was not detected");return;}const auto state=read(output.filePath("crash-tools-state.json"));auto copy=history->restoreCopy(state.value("lessonId").toString(),"recovery.json",&error);if(!copy||!sceneText(manager->loadDocumentScene(copy,0)).contains("Recovered crash draft")){done(false,"Crash draft recovery failed: "+error);return;}
        report["recoveredPath"]=copy->persistencePath();done(true,"Favourite persisted across restart; forced process termination detected; last recovery snapshot restored as a separate readable lesson");return;
    }
    done(false,"Unknown lesson tools test mode");
}
}
void scheduleLessonToolsTest(QObject *context){QTimer::singleShot(120000,context,[]{qCritical("Lesson tools test timed out");qApp->exit(32);});QTimer::singleShot(7000,context,run);}
