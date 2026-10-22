// SPDX-License-Identifier: GPL-3.0-only
#include "UBLessonToolsTest.h"
#include "UBLessonTools.h"
#include <QtWidgets>
#include <QDomDocument>
#include "board/UBBoardView.h"
#include "gui/UBStylusPalette.h"
#include "gui/UBActionPalette.h"
#include "core/UBApplication.h"
#include "core/UBSettings.h"
#include "core/UBLessonHistory.h"
#include "core/UBPersistenceManager.h"
#include "gui/UBMainWindow.h"
#include "gui/UBToolbarButtonGroup.h"
#include "core/UBSetting.h"
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
bool colourMemory(bool seed, QString *error)
{
    auto dc=UBDrawingController::drawingController();auto window=UBApplication::mainWindow;
    auto settings=UBSettings::settings();settings->setDarkBackground(false);
    auto fail=[error](const QString &s){*error=s;return false;};
    const int widths[4][2]={{0,2},{2,0},{1,2},{2,1}};
    const int colours[2]={1,8};
    auto choose=[&](int mode){
        if(mode==0)window->actionPen->trigger();
        else if(mode==1)window->actionMarker->trigger();
        else if(mode==2)window->actionLine->trigger();
        else window->findChild<QAction*>("actionTeachingDash")->trigger();
    };
    const auto penBefore=settings->penColors(false),markerBefore=settings->markerColors(false);
    if(seed) for(int mode=0;mode<4;++mode)for(int c=0;c<2;++c){choose(mode);dc->setColorIndex(colours[c]);dc->setLineWidthIndex(widths[mode][c]);}
    int notified=-1;auto connection=QObject::connect(dc,&UBDrawingController::lineWidthIndexChanged,[&](int i){notified=i;});
    bool valid=true;
    for(int repeat=0;repeat<3;++repeat)for(int mode=3;mode>=0;--mode)for(int c=1;c>=0;--c){
        choose(mode);notified=-1;dc->setColorIndex(colours[c]);
        valid &= dc->currentToolWidthIndex()==widths[mode][c] && notified==widths[mode][c];
        if(mode>=2)valid &= settings->value("Teaching/DashedLines").toBool()==(mode==3);
        for(auto group:window->findChildren<UBToolbarButtonGroup*>())
            if(group->property("teachingLabel").toString()=="Line thickness")valid &= group->currentIndex()==widths[mode][c];
    }
    QObject::disconnect(connection);
    if(!valid)return fail("Per-colour widths or toolbar selection did not restore independently for Pen, Highlighter, Solid Line and Dashed Line");
    if(settings->penColors(false)!=penBefore||settings->markerColors(false)!=markerBefore)return fail("Remembering thickness changed a colour palette");
    if(window->findChild<QAction*>("actionLessonFavourites")||window->findChild<QAction*>("actionLessonFavourite1"))return fail("Numbered favourite controls remain");
    if(seed){
        // Real colour identity follows a recoloured/moved swatch, not its slot.
        choose(0);dc->setColorIndex(8);const auto spare=penBefore[9];
        dc->setPenColor(false,penBefore[1],9);dc->setColorIndex(9);
        if(dc->currentToolWidthIndex()!=0)return fail("Moving red to another swatch lost its thickness");
        dc->setLineWidthIndex(1);dc->setColorIndex(1);
        if(dc->currentToolWidthIndex()!=1)return fail("Duplicate red swatches did not share Pen thickness");
        dc->setPenColor(false,spare,9);dc->setLineWidthIndex(0);
        dc->setPenColor(false,penBefore[8],1);
        if(dc->currentToolWidthIndex()!=2)return fail("Editing the selected swatch did not recall the new colour's width");
        dc->setPenColor(false,penBefore[1],1);
        if(dc->currentToolWidthIndex()!=0)return fail("Restoring a swatch lost its old colour thickness");
        const auto dark=settings->penColors(true)[1];dc->setPenColor(true,penBefore[8],1);
        settings->setDarkBackground(true);
        if(dc->currentToolWidthIndex()!=2)return fail("Changing background did not recall the visible colour width");
        settings->setDarkBackground(false);dc->setPenColor(true,dark,1);
        if(dc->currentToolWidthIndex()!=0)return fail("Returning to the light palette lost its width");
        // Highlighter alpha is appearance, not a different colour preference.
        choose(1);dc->setColorIndex(1);const qreal alpha=settings->boardMarkerAlpha->get().toDouble();
        dc->setMarkerAlpha(.25);dc->setColorIndex(8);dc->setColorIndex(1);
        if(dc->currentToolWidthIndex()!=2)return fail("Highlighter opacity split its colour thickness memory");
        dc->setMarkerAlpha(alpha);
    }
    return true;
}
bool coverWorkflow(UBLessonTools *tools,QString *error)
{
    auto fail=[error](const QString &s){*error=s;return false;};
    auto window=UBApplication::mainWindow;auto board=UBApplication::boardController;
    auto manager=UBPersistenceManager::persistenceManager();
    auto document=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Cover toolbar regression",true);
    board->setActiveDocumentScene(document,0);auto scene=board->activeScene();
    auto cover=window->findChild<QAction*>("actionLessonCover");
    auto bar=window->findChild<QToolBar*>("lessonCoverToolbar");
    auto draw=window->findChild<QAction*>("lessonDrawCover");
    auto shadeAction=window->findChild<QAction*>("lessonScreenShade");
    if(!cover||!bar||!draw||!shadeAction||window->findChild<QAction*>("actionLessonCovers"))return fail("Single Cover toolbar unavailable");
    cover->trigger();QApplication::processEvents();
    if(bar->isHidden()||!draw->isChecked())return fail("Cover did not default to Draw cover");
    auto view=board->controlView();auto viewport=view->viewport();
    auto send=[&](QEvent::Type type,QPointF point){
        const QPointF p=view->mapFromScene(point);
        const auto button=type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton;
        const auto buttons=type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton;
        QMouseEvent event(type,p,viewport->mapToGlobal(p.toPoint()),button,buttons,Qt::NoModifier);
        QApplication::sendEvent(viewport,&event);
    };
    send(QEvent::MouseButtonPress,QPointF(-150,-100));send(QEvent::MouseMove,QPointF(150,100));send(QEvent::MouseButtonRelease,QPointF(150,100));
    UBGraphicsSvgItem *drawn=nullptr;
    for(auto item:scene->selectedItems())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover())drawn=svg;
    if(!drawn||qAbs(drawn->sceneBoundingRect().width()-300)>5||qAbs(drawn->sceneBoundingRect().height()-200)>5)return fail("Dragged cover has incorrect dimensions");
    send(QEvent::MouseButtonDblClick,QPointF(0,0));if(!tools->isCoverRevealed(drawn))return fail("Double click did not reveal the cover");
    send(QEvent::MouseButtonDblClick,QPointF(0,0));if(tools->isCoverRevealed(drawn))return fail("Double click did not conceal the cover again");
    draw->trigger();send(QEvent::MouseButtonPress,QPointF(-200,-200));
    QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(viewport,&escape);
    if(draw->isChecked())return fail("Escape did not cancel cover drawing");
    shadeAction->trigger();UBGraphicsSvgItem *shade=nullptr;
    for(auto item:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->fileData().contains("data-screen-shade"))shade=svg;
    if(!shade)return fail("Screen shade was not created");
    // Drag each edge inward and back. This checks that opposite edges stay fixed.
    for(int edge=0;edge<4;++edge){
        QDomDocument doc;doc.setContent(shade->fileData());auto r=doc.elementsByTagName("rect").at(0).toElement();
        const double x=r.attribute("x").toDouble(),y=r.attribute("y").toDouble(),w=r.attribute("width").toDouble(),h=r.attribute("height").toDouble();
        QPointF a(edge<2?(edge==0?x:x+w):x+w/2,edge>=2?(edge==2?y:y+h):y+h/2);
        const QPointF delta=edge<2?QPointF(edge==0?60:-60,0):QPointF(0,edge==2?60:-60);
        auto before=shade->fileData();send(QEvent::MouseButtonPress,shade->mapToScene(a));send(QEvent::MouseMove,shade->mapToScene(a+delta));send(QEvent::MouseButtonRelease,shade->mapToScene(a+delta));
        if(shade->fileData()==before)return fail(QString("Shade edge %1 did not drag").arg(edge));
        QDomDocument changed;changed.setContent(shade->fileData());auto cr=changed.elementsByTagName("rect").at(0).toElement();
        const auto dimension=edge<2?"width":"height";const double expected=(edge<2?w:h)-60;
        if(qAbs(cr.attribute(dimension).toDouble()-expected)>4)return fail("Shade edge drag changed the wrong extent");
        UBApplication::undoStack->undo();if(shade->fileData()!=before)return fail("Shade drag undo failed");
        UBApplication::undoStack->redo();
        send(QEvent::MouseButtonPress,shade->mapToScene(a+delta));send(QEvent::MouseMove,shade->mapToScene(a));send(QEvent::MouseButtonRelease,shade->mapToScene(a));
        QDomDocument restored;restored.setContent(shade->fileData());
        if(qAbs(restored.elementsByTagName("rect").at(0).toElement().attribute(dimension).toDouble()-(edge<2?w:h))>4)return fail("Shade edge did not conceal again");
    }
    const auto saved=shade->fileData();UBSvgSubsetAdaptor::persistScene(document,scene,0);
    auto fresh=UBSvgSubsetAdaptor::loadScene(document,0);if(!fresh)return fail("Saved shade page could not be reopened");bool persisted=false;
    for(auto item:fresh->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->fileData()==saved)persisted=true;
    if(!persisted)return fail("Shade geometry did not survive page reload");
    cover->trigger();
    return true;
}

bool penToolbarWorkflow(QString *error)
{
    auto window=UBApplication::mainWindow;auto settings=UBSettings::settings();
    auto customize=window->findChild<QAction*>("actionTeachingCustomize");
    const auto original=settings->value("Teaching/BottomOrder");bool checked=false;
    customize->setProperty("teachingOpenPenTab",true);
    QTimer::singleShot(0,window,[&]{
        auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());auto tabs=dialog?dialog->findChild<QTabWidget*>():nullptr;
        auto list=tabs?qobject_cast<QListWidget*>(tabs->currentWidget()):nullptr;
        if(!list||tabs->currentIndex()!=1||list->count()<4){if(dialog)dialog->reject();return;}
        list->clearSelection();list->item(1)->setSelected(true);list->item(2)->setSelected(true);
        const auto first=list->item(1)->data(Qt::UserRole),second=list->item(2)->data(Qt::UserRole);
        for(auto button:dialog->findChildren<QPushButton*>())if(button->text()=="Move up"){button->click();break;}
        checked=list->item(0)->data(Qt::UserRole)==first&&list->item(1)->data(Qt::UserRole)==second;
        dialog->accept();
    });
    customize->trigger();
    const auto order=settings->value("Teaching/BottomOrder").toStringList();
    for(auto palette:window->findChildren<UBStylusPalette*>()){
        int previous=-1;
        for(const auto &id:order)if(auto action=window->findChild<QAction*>(id))if(auto button=palette->getButtonFromAction(action)){
            const int index=palette->layout()->indexOf(button);if(index<=previous)checked=false;previous=index;
        }
    }
    // Restore the user's arrangement through the real apply path.
    settings->setValue("Teaching/BottomOrder",original);
    QTimer::singleShot(0,window,[]{if(auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->accept();});
    customize->trigger();
    if(!checked)*error="Pen toolbar tab, group movement or applied order failed";
    return checked;
}

void run(){const QString mode=qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_MODE");const QString reportPath=qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_REPORT");const QDir output=QFileInfo(reportPath).dir();QJsonObject report;report["mode"]=mode;
    auto done=[&](bool passed,const QString &detail){report["passed"]=passed;report["detail"]=detail;write(reportPath,report);qApp->exit(passed?0:31);};
    auto tools=UBApplication::mainWindow->findChild<UBLessonTools*>("lessonTools");if(!tools){done(false,"Lesson tools unavailable");return;}auto history=tools->history();auto board=UBApplication::boardController;auto manager=UBPersistenceManager::persistenceManager();QString error;
    if(mode=="exercise"){
        if(!colourMemory(true,&error)){done(false,error);return;}
        report["colourWidthMemoryChecked"]=true;
        if(!coverWorkflow(tools,&error)||!penToolbarWorkflow(&error)){done(false,error);return;}
        report["coverToolbarChecked"]=true;report["penToolbarCustomizationChecked"]=true;
        auto dc=UBDrawingController::drawingController();
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
        done(true,"Per-colour Pen/Highlighter/Solid Line/Dashed Line thickness, toolbar synchronization, palette editing/background/alpha handling, cover occlusion/reveal/undo/reload, history deduplication, separate-copy restore and damaged-blob rejection passed");return;
    }
    if(!colourMemory(false,&error)){done(false,"Thickness did not survive restart: "+error);return;}
    report["colourWidthMemoryChecked"]=true;
    if(mode=="crash"){
        auto doc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Crash recovery fixture",true);board->setActiveDocumentScene(doc,0);board->activeScene()->addText("Recovered crash draft",QPointF(0,0));report["lessonId"]=doc->uuid().toString(QUuid::WithoutBraces);
        // Let the real 20-second recovery timer save this draft before killing the process.
        QTimer::singleShot(25000,qApp,[history,report,reportPath,output]() mutable {
            const QString snapshot=history->rootPath()+"/"+report.value("lessonId").toString()+"/recovery.json";
            if(!QFile::exists(snapshot)){report["passed"]=false;report["detail"]="Automatic recovery timer did not save the crash draft";write(reportPath,report);qApp->exit(31);return;}
            report["prepared"]=true;report["automaticRecovery"]=true;write(output.filePath("crash-tools-state.json"),report);write(reportPath,report);
#ifdef Q_OS_WIN
            TerminateProcess(GetCurrentProcess(),89);
#else
            std::_Exit(89);
#endif
        });
        return;
    }
    if(mode=="recover"){
        if(!history->uncleanSessionDetected()){done(false,"Unclean shutdown was not detected");return;}const auto state=read(output.filePath("crash-tools-state.json"));auto copy=history->restoreCopy(state.value("lessonId").toString(),"recovery.json",&error);if(!copy||!sceneText(manager->loadDocumentScene(copy,0)).contains("Recovered crash draft")){done(false,"Crash draft recovery failed: "+error);return;}
        report["recoveredPath"]=copy->persistencePath();done(true,"Per-colour thickness persisted across restart; forced process termination detected; last recovery snapshot restored as a separate readable lesson");return;
    }
    done(false,"Unknown lesson tools test mode");
}
}
void scheduleLessonToolsTest(QObject *context){QTimer::singleShot(120000,context,[]{qCritical("Lesson tools test timed out");qApp->exit(32);});QTimer::singleShot(7000,context,run);}
