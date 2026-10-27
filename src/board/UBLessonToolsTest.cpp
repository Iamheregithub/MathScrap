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
#include "domain/UBGraphicsStrokesGroup.h"
#include "domain/UBGraphicsItemDelegate.h"
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
    auto fail=[error](const QString &s){*error=s;if(auto window=UBApplication::mainWindow)window->grab().save(QFileInfo(qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_REPORT")).dir().filePath("cover-failure-window.png"));return false;};
    auto window=UBApplication::mainWindow;auto board=UBApplication::boardController;
    auto manager=UBPersistenceManager::persistenceManager();
    auto document=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Cover lock regression",true);
    board->setActiveDocumentScene(document,0);auto scene=board->activeScene();
    auto cover=window->findChild<QAction*>("actionLessonCover");
    auto bar=window->findChild<QToolBar*>("lessonCoverToolbar");
    auto draw=window->findChild<QAction*>("lessonDrawCover");
    auto shadeAction=window->findChild<QAction*>("lessonScreenShade");
    if(!cover||!bar||!draw||!shadeAction||bar->actions().size()!=3)return fail("Cover tray is not the three approved controls");
    for(const auto &id:{"actionLessonReveal","lessonCoverSelectedReveal","lessonCoverSelectedHide","lessonCoverAllReveal","lessonCoverAllHide","lessonCoverLock","lessonResetReveals"})if(window->findChild<QAction*>(id))return fail("Removed cover command is still registered");
    cover->trigger();QApplication::processEvents();
    if(bar->isHidden()||!draw->isChecked())return fail("Cover did not default to Draw cover");
    auto view=board->controlView();auto viewport=view->viewport();
    auto send=[&](QEvent::Type type,QPointF point){
        const QPointF p=view->mapFromScene(point);QMouseEvent event(type,p,viewport->mapToGlobal(p.toPoint()),type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(viewport,&event);
    };
    auto drag=[&](const QPointF &from,const QPointF &to){send(QEvent::MouseButtonPress,from);send(QEvent::MouseMove,to);send(QEvent::MouseButtonRelease,to);};
    auto localRect=[](UBGraphicsSvgItem *item){QDomDocument doc;doc.setContent(item->fileData());auto r=doc.elementsByTagName("rect").at(0).toElement();return QRectF(r.attribute("x").toDouble(),r.attribute("y").toDouble(),r.attribute("width").toDouble(),r.attribute("height").toDouble());};
    auto controlPoint=[&](UBGraphicsSvgItem *item,const QString &role){item->refreshCoverControls();return view->mapToScene(item->coverControlRect(role).center().toPoint());};
    auto tap=[&](UBGraphicsSvgItem *item,const QString &role){const auto point=controlPoint(item,role);send(QEvent::MouseButtonPress,point);send(QEvent::MouseButtonRelease,point);};
    drag(QPointF(-150,-100),QPointF(150,100));
    UBGraphicsSvgItem *drawn=nullptr;for(auto item:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover())drawn=svg;
    if(!drawn||!drawn->Delegate()->isLocked()||drawn->flags().testFlag(QGraphicsItem::ItemIsMovable))return fail("Newly drawn cover did not start locked");
    const auto original=localRect(drawn);const auto fixed=drawn->pos();const auto transform=drawn->transform();
    drag(QPointF(0,0),QPointF(40,30));drawn->setPos(fixed+QPointF(10,20));drawn->setScale(2);drawn->setRotation(30);drawn->setTransform(QTransform().scale(2,1));
    if(drawn->pos()!=fixed||drawn->scale()!=1||drawn->rotation()!=0||drawn->transform()!=transform)return fail("Locked cover moved, stretched or rotated");
    const auto undo=UBApplication::undoStack->index();
    send(QEvent::MouseButtonDblClick,QPointF(0,0));if(tools->isCoverRevealed(drawn))return fail("Double-click revealed the cover");
    auto grip=controlPoint(drawn,"grip");drag(grip,grip+QPointF(0,60));
    if(qAbs(localRect(drawn).top()-original.top()-60)>4||qAbs(localRect(drawn).bottom()-original.bottom())>.01)return fail("Locked cover grip did not keep bottom fixed");
    const auto movedGrip=controlPoint(drawn,"grip");
    if(qAbs(view->mapFromScene(movedGrip).x()-view->mapFromScene(drawn->mapToScene(QPointF(localRect(drawn).center().x(),localRect(drawn).top()))).x())>2)return fail("Grip is not centred on the visible top edge");
    drag(movedGrip,movedGrip-QPointF(0,60));
    window->actionPen->trigger();tap(drawn,"lock");if(drawn->Delegate()->isLocked()||UBDrawingController::drawingController()->stylusTool()!=UBStylusTool::Selector)return fail("Circular lock button did not unlock into an immediately movable state");
    const auto body=drawn->mapToScene(localRect(drawn).center());drag(body,body+QPointF(35,25));
    if(QLineF(drawn->pos(),fixed+QPointF(35,25)).length()>3)return fail("Unlocked cover did not move");
    tap(drawn,"lock");if(!drawn->Delegate()->isLocked())return fail("Circular lock button did not relock");
    if(UBApplication::undoStack->index()!=undo)return fail("Cover controls added unrequested undo entries");
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    auto pen=[&](QEvent::Type type,const QPointF &point){const QPointF p=view->mapFromScene(point);QTabletEvent event(type,QPointingDevice::primaryPointingDevice(),p+QPointF(0,48),viewport->mapToGlobal(p.toPoint()),type==QEvent::TabletRelease?0.:.8,0,0,0,0,0,Qt::NoModifier,type==QEvent::TabletMove?Qt::NoButton:Qt::LeftButton,type==QEvent::TabletRelease?Qt::NoButton:Qt::LeftButton);QApplication::sendEvent(viewport,&event);};
    grip=controlPoint(drawn,"grip");const auto beforeTablet=localRect(drawn);pen(QEvent::TabletPress,grip);const auto pressedTablet=localRect(drawn);pen(QEvent::TabletMove,grip+QPointF(0,45));const auto movedTablet=localRect(drawn);pen(QEvent::TabletRelease,grip+QPointF(0,45));
    if(qAbs(localRect(drawn).top()-45)>4||qAbs(localRect(drawn).bottom()-original.bottom())>.01)return fail(QString("Stylus top-handle reveal failed: before=%1 press=%2 move=%3 release=%4 bottom=%5/%6 grip=%7,%8 tool=%9").arg(beforeTablet.top()).arg(pressedTablet.top()).arg(movedTablet.top()).arg(localRect(drawn).top()).arg(localRect(drawn).bottom()).arg(original.bottom()).arg(grip.x()).arg(grip.y()).arg(UBDrawingController::drawingController()->stylusTool()));
    auto button=controlPoint(drawn,"lock");pen(QEvent::TabletPress,button);pen(QEvent::TabletRelease,button);if(drawn->Delegate()->isLocked())return fail("Stylus lock button failed");
    tap(drawn,"lock");
#endif
    // A shape-drawing mode must not capture an on-cover button press.
    const auto beforeShape=drawn->Delegate()->isLocked();window->findChild<QAction*>("lessonShape0")->trigger();tap(drawn,"lock");
    if(drawn->Delegate()->isLocked()==beforeShape)return fail("Shape tool swallowed cover lock button");window->actionSelector->trigger();drawn->setCoverLocked(true);
    const auto coverData=drawn->fileData();const auto coverPos=drawn->pos();
    shadeAction->trigger();UBGraphicsSvgItem *shade=nullptr;for(auto item:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isScreenShade())shade=svg;
    if(!shade||!shade->Delegate()->isLocked())return fail("Screen shade did not start fixed");
    const auto shadePos=shade->pos();const auto shadeGeometry=localRect(shade);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    auto penButton=window->findChild<QToolButton*>("paper-actionPen");if(!penButton)return fail("Pen button missing");
    const auto penLocation=view->mapToScene(viewport->mapFromGlobal(penButton->mapToGlobal(penButton->rect().center())));
    const auto toolbarUndo=UBApplication::undoStack->index();pen(QEvent::TabletPress,penLocation);pen(QEvent::TabletRelease,penLocation);
    if(UBDrawingController::drawingController()->stylusTool()!=UBStylusTool::Pen||UBApplication::undoStack->index()!=toolbarUndo)return fail("Full-page shade swallowed a stylus toolbar tap");window->actionSelector->trigger();
#endif
    const auto shadeBody=shade->mapToScene(shadeGeometry.center());drag(shadeBody,shadeBody+QPointF(60,30));
    shade->Delegate()->setLocked(false);shade->setPos(shadePos+QPointF(20,20));shade->setScale(2);shade->setRotation(15);
    if(shade->pos()!=shadePos||!shade->Delegate()->isLocked()||shade->scale()!=1||shade->rotation()!=0)return fail("Screen shade could move, resize or unlock");
    // No side/bottom edge should resize or move a shade.
    for(const auto &edge:{QPointF(shadeGeometry.left(),shadeGeometry.center().y()),QPointF(shadeGeometry.right(),shadeGeometry.center().y()),QPointF(shadeGeometry.center().x(),shadeGeometry.bottom())})drag(shade->mapToScene(edge),shade->mapToScene(edge+QPointF(20,-20)));
    if(localRect(shade)!=shadeGeometry||shade->pos()!=shadePos)return fail("A side or bottom shade edge moved");
    auto shadeGrip=controlPoint(shade,"grip");drag(shadeGrip,shadeGrip+QPointF(0,80));
    if(qAbs(localRect(shade).top()-shadeGeometry.top()-80)>4||qAbs(localRect(shade).bottom()-shadeGeometry.bottom())>.01)return fail("Shade top handle did not preserve bottom");
    const auto shadeData=shade->fileData();
    // Save/reload preserves each distinct object's position, lock and reveal.
    auto unlocked=tools->addCover();unlocked->setCoverLocked(false);unlocked->setPos(210,130);unlocked->setScale(.7);unlocked->setRotation(10);const auto unlockedId=unlocked->uuid();
    UBSvgSubsetAdaptor::persistScene(document,scene,0);auto fresh=UBSvgSubsetAdaptor::loadScene(document,0);if(!fresh)return fail("Cover lesson did not reopen");
    bool foundCover=false,foundShade=false,foundUnlocked=false;
    for(auto item:fresh->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover()){
        if(svg->fileData()==coverData&&QLineF(svg->pos(),coverPos).length()<.02&&svg->Delegate()->isLocked())foundCover=true;
        if(svg->fileData()==shadeData&&QLineF(svg->pos(),shadePos).length()<.02&&svg->Delegate()->isLocked())foundShade=true;
        if(svg->uuid()==unlockedId&&!svg->Delegate()->isLocked()&&QLineF(svg->pos(),unlocked->pos()).length()<.1&&qAbs(svg->sceneTransform().m11()-unlocked->sceneTransform().m11())<.01&&qAbs(svg->sceneTransform().m12()-unlocked->sceneTransform().m12())<.01)foundUnlocked=true;
    }
    if(!foundCover||!foundShade||!foundUnlocked)return fail("Position, lock, size or reveal state changed on reopen");
    // The close target stays full-size and on-screen at near-total reveal and zoom.
    shadeGrip=controlPoint(shade,"grip");drag(shadeGrip,shadeGrip+QPointF(0,10000));
    shade->refreshCoverControls();auto close=shade->coverControlRect("close");
    if(close.width()<44||close.height()<44||!QRectF(viewport->rect()).contains(close))return fail("Pulled-down shade close button became inaccessible");
    if(auto dock=window->findChild<QWidget*>("paperDock");dock&&dock->isVisible()){
        const QRectF dockRect(QPointF(viewport->mapFromGlobal(dock->mapToGlobal(QPoint(0,0)))),QSizeF(dock->size()));
        if(close.intersects(dockRect)||shade->coverControlRect("grip").intersects(dockRect))return fail("Shade controls are hidden behind the pen toolbar");
    }
    const auto viewportTransform=view->transform();view->scale(.5,.5);shade->refreshCoverControls();
    if(shade->coverControlRect("close").width()<44)return fail("Shade close button shrank with zoom");view->setTransform(viewportTransform);shade->refreshCoverControls();
    view->viewport()->update();QApplication::processEvents();
    window->grab().save(QFileInfo(qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_REPORT")).dir().filePath("cover-controls-window.png"));
    window->actionPen->trigger();const auto closeUndo=UBApplication::undoStack->index();tap(shade,"close");
    if(shade->scene()||drawn->scene()!=scene||unlocked->scene()!=scene||UBApplication::undoStack->index()!=closeUndo)return fail("Shade close removed other content or added undo support");
    window->actionSelector->trigger();
    bool customizeChecked=false;QTimer::singleShot(0,qApp,[&]{auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;auto panel=dialog->findChild<QWidget*>("workspaceCoverControls");if(panel){auto buttons=panel->findChildren<QToolButton*>();customizeChecked=buttons.size()==1&&buttons.first()->defaultAction()->objectName()=="lessonCoverColour";}dialog->reject();});
    window->findChild<QAction*>("actionTeachingCustomize")->trigger();if(!customizeChecked)return fail("Customize Toolbar contains options other than Cover colour");
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
        auto list=dialog?dialog->findChild<QListWidget*>("workspaceToolbarPreview"):nullptr;
        if(!list||tabs->currentIndex()!=0||list->count()<4){if(dialog)dialog->reject();return;}
        list->clearSelection();list->item(1)->setSelected(true);list->item(2)->setSelected(true);
        const auto first=list->item(1)->data(Qt::UserRole),second=list->item(2)->data(Qt::UserRole);
        for(auto button:dialog->findChildren<QPushButton*>())if(button->text()=="Move left"){button->click();break;}
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
    write(qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_REPORT"),{{"phase","touch controls"}});
    const bool originalTouch=settings->value("Teaching/TouchFriendly",false).toBool();
    const auto baseSize=window->boardToolBar->iconSize();
    for(bool enabled:{true,false}){
        bool found=false;
        QTimer::singleShot(0,window,[&]{auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            auto touch=dialog?dialog->findChild<QCheckBox*>("lessonTouchFriendly"):nullptr;
            if(touch){touch->setChecked(enabled);found=true;}if(dialog)dialog->accept();});
        customize->trigger();checked &= found&&settings->value("Teaching/TouchFriendly").toBool()==enabled;
        if(enabled){checked &= window->boardToolBar->iconSize()==QSize(40,40);
            for(auto palette:window->findChildren<UBStylusPalette*>())checked &= palette->buttonSize()==QSize(56,56);
        }else checked &= window->boardToolBar->iconSize()==baseSize;
    }
    settings->setValue("Teaching/TouchFriendly",originalTouch);settings->save();
    window->findChild<UBLessonTools*>()->applyTouchMode();
    if(!checked)*error="Pen toolbar ordering or touch-friendly controls failed";
    return checked;
}

void run(){const QString mode=qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_MODE");const QString reportPath=qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_REPORT");const QDir output=QFileInfo(reportPath).dir();QJsonObject report;report["mode"]=mode;
    auto done=[&](bool passed,const QString &detail){report["passed"]=passed;report["detail"]=detail;write(reportPath,report);qApp->exit(passed?0:31);};
    auto tools=UBApplication::mainWindow->findChild<UBLessonTools*>("lessonTools");if(!tools){done(false,"Lesson tools unavailable");return;}auto history=tools->history();auto board=UBApplication::boardController;auto manager=UBPersistenceManager::persistenceManager();QString error;
    auto stopWithoutExit=[&]{
#ifdef Q_OS_WIN
        TerminateProcess(GetCurrentProcess(),89);
#else
        std::_Exit(89);
#endif
    };
    if(mode.startsWith("recovery-")||mode=="save-checkpoint-failure"){
        const QString statePath=output.filePath("recovery-notice-state.json");
        if(mode=="recovery-empty-seed"){report["prepared"]=true;write(reportPath,report);stopWithoutExit();return;}
        if(mode=="recovery-empty-reopen"){
            if(!history->uncleanSessionDetected()||history->recoveryPromptPending()||UBApplication::mainWindow->property("recoveryPromptShown").toBool()){done(false,"Empty history produced a recovery prompt");return;}
            done(true,"An unclean marker without recovery copies does not prompt");return;
        }
        if(mode=="recovery-seed"){
            auto doc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Recovery notice fixture",true);board->setActiveDocumentScene(doc,0);
            const QString marker="Recovery fixture "+QUuid::createUuid().toString();auto scene=board->activeScene();scene->addText(marker,QPointF(-160,-100));
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Pen);scene->inputDevicePress(QPointF(-100,0));scene->inputDeviceMove(QPointF(100,0));scene->inputDeviceRelease();
            if(!history->checkpoint(true,&error)){done(false,error);return;}
            QJsonObject state{{"lessonId",doc->uuid().toString(QUuid::WithoutBraces)},{"lessonPath",doc->persistencePath()},{"marker",marker}};
            state["digest"]=read(history->rootPath()+"/"+state.value("lessonId").toString()+"/recovery.json").value("digest");write(statePath,state);
            report["prepared"]=true;write(reportPath,report);stopWithoutExit();return;
        }
        const auto state=read(statePath);if(state.isEmpty()){done(false,"Recovery notice state missing");return;}
        auto doc=std::make_shared<UBDocumentProxy>(state.value("lessonPath").toString());auto scene=manager->loadDocumentScene(doc,0,false);
        int strokes=0;if(scene)for(auto item:scene->items())if(item->type()==UBGraphicsStrokesGroup::Type)++strokes;
        if(!scene||!sceneText(scene).contains(state.value("marker").toString())||!strokes){done(false,"The saved lesson changed or disappeared while handling recovery history");return;}
        board->setActiveDocumentScene(doc,0);
        if(mode=="save-checkpoint-failure"){
            if(!history->checkpoint(true,&error,true)){done(false,error);return;}
            const QString blobs=history->rootPath()+"/"+state.value("lessonId").toString()+"/blobs",held=blobs+"-held";
            if(!QDir().rename(blobs,held)){done(false,"Cannot prepare a backup-write failure");return;}QFile blocker(blobs);if(!blocker.open(QIODevice::WriteOnly)){done(false,"Cannot block backup folder for test");return;}blocker.write("Test blocker");blocker.close();
            report["blocked"]=blobs;report["held"]=held;report["prepared"]=true;write(reportPath,report);UBApplication::mainWindow->close();return;
        }
        const bool shown=UBApplication::mainWindow->property("recoveryPromptShown").toBool();
        const bool dismissed=mode=="recovery-dismiss"||mode=="recovery-dismiss-crash"||mode=="recovery-failure-dismiss";
        if(dismissed||mode=="recovery-review"){
            if(!shown||history->recoveryPromptPending()||history->documents().isEmpty()){done(false,"Recovery dismissal was not remembered or removed copies");return;}
            if(mode=="recovery-failure-dismiss"&&history->uncleanSessionDetected()){done(false,"Backup failure was misreported as a crash");return;}
            if(mode=="recovery-review"&&!UBApplication::mainWindow->property("recoveryReviewVisited").toBool()){done(false,"Review did not open readable recovery history");return;}
            auto restored=history->restoreCopy(state.value("lessonId").toString(),"recovery.json",&error);
            if(!restored||!sceneText(manager->loadDocumentScene(restored,0)).contains(state.value("marker").toString())){done(false,"Dismissal damaged recovery copies: "+error);return;}
            board->setActiveDocumentScene(doc,0);report["prepared"]=true;report["passed"]=true;report["detail"]="Recovery prompt choice persisted and copies remained restorable";write(reportPath,report);
            if(mode=="recovery-dismiss-crash")stopWithoutExit();else if(mode=="recovery-dismiss")UBApplication::mainWindow->close();else done(true,report.value("detail").toString());return;
        }
        if(mode=="recovery-delete-cancel"){
            if(!shown||!UBApplication::mainWindow->property("recoveryDeleteCancelled").toBool()||history->documents().isEmpty()){done(false,"Cancelling Delete all removed recovery copies");return;}
            report["prepared"]=true;write(reportPath,report);stopWithoutExit();return;
        }
        if(mode=="recovery-delete"){
            if(!shown||!UBApplication::mainWindow->property("recoveryDeleteConfirmed").toBool()||!history->documents().isEmpty()||history->recoveryPromptPending()){done(false,"Confirmed Delete all did not clear history and its prompt");return;}
            report["prepared"]=true;write(reportPath,report);UBApplication::mainWindow->close();return;
        }
        if(mode=="recovery-delete-reopen"||mode=="recovery-explicit-version"){
            if(shown||history->recoveryPromptPending()||!history->documents().isEmpty()){done(false,"Deleted history or prompt returned on restart");return;}
            if(!history->checkpoint(false,&error)||!history->documents().isEmpty()){done(false,"Unchanged content recreated deleted history");return;}
            if(mode=="recovery-explicit-version"){
                UBApplication::mainWindow->findChild<QAction*>("actionLessonSaveVersion")->trigger();
                if(history->documents().isEmpty()){done(false,"Explicit saved version did not resume recovery after Delete all");return;}
            }
            done(true,"Deleted history stayed empty across restart; live lesson and pen strokes were retained");return;
        }
        if(mode=="recovery-new-work"){
            scene->addText("Edits before deleting history",QPointF(-160,-160));
            if(!history->clearHistory(&error)||!history->documents().isEmpty()){done(false,"Cannot clear history with current edits: "+error);return;}
            if(!history->checkpoint(false,&error)||!history->documents().isEmpty()){done(false,"Current edits from before Delete all recreated history");return;}
            auto newText=scene->addText("New work after deleting history",QPointF(-160,-220));
            if(!history->checkpoint(false,&error)||history->documents().isEmpty()){done(false,"New lesson content did not resume automatic recovery: "+error);return;}
            auto restored=history->restoreCopy(state.value("lessonId").toString(),"recovery.json",&error);
            const auto recovered=restored?sceneText(manager->loadDocumentScene(restored,0)):QString();
            if(!recovered.contains("New work after deleting history")||!recovered.contains("Edits before deleting history")||!recovered.contains(state.value("marker").toString())){done(false,"New recovery copy lost lesson text: "+error);return;}
            // Returning to the pre-change content must update recovery too.
            board->setActiveDocumentScene(doc,0);
            // addText registers the item in the scene's deletion set. Keep that
            // ownership intact so shutdown does not try to delete it twice.
            scene->removeItem(newText);scene->setModified(true);
            if(!history->checkpoint(false,&error)){done(false,error);return;}
            auto reverted=history->restoreCopy(state.value("lessonId").toString(),"recovery.json",&error);
            const auto revertedText=reverted?sceneText(manager->loadDocumentScene(reverted,0)):QString();
            if(!reverted||revertedText.contains("New work after deleting history")||!revertedText.contains("Edits before deleting history")){done(false,"Old deletion baseline suppressed recovery after returning to previous content");return;}
            done(true,"Delete all includes current edits; new content resumes automatic recovery and remains restorable");return;
        }
        if(mode=="recovery-delete-damaged"){
            const QString blobs=history->rootPath()+"/"+state.value("lessonId").toString()+"/blobs",held=blobs+"-held";
            if(!QDir().rename(blobs,held)){done(false,"Cannot prepare damaged backup storage");return;}
            QFile blocker(blobs);if(!blocker.open(QIODevice::WriteOnly)){done(false,"Cannot create damaged backup storage fixture");return;}blocker.write("Test blocker");blocker.close();
            if(!history->clearHistory(&error)||!history->documents().isEmpty()){done(false,"Damaged backup storage prevented Delete all: "+error);return;}
            if(!history->checkpoint(false,&error)||!history->documents().isEmpty()){done(false,"Deleted damaged history returned without edits");return;}
            done(true,"Damaged recovery history can be deleted without deleting the saved lesson");return;
        }
        if(mode=="recovery-reopen"||mode=="recovery-reopen-ack"){
            if(shown||history->recoveryPromptPending()||history->documents().isEmpty()){done(false,"Dismissed recovery prompt returned without new recoverable work");return;}
            if(mode=="recovery-reopen"&&history->uncleanSessionDetected()){done(false,"An orderly exit was marked unclean");return;}
            if(mode=="recovery-reopen-ack"&&!history->uncleanSessionDetected()){done(false,"The interrupted-session fixture did not remain unclean");return;}
            done(true,"Dismissed copies remain available without repeatedly prompting after restart");return;
        }
        done(false,"Unknown recovery test mode");return;
    }
    if(mode=="save-write"){
        if(history->uncleanSessionDetected()){done(false,"Save fixture started with an unclean session");return;}
        auto doc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Normal exit save fixture",true);
        board->setActiveDocumentScene(doc,0);
        auto scene=board->activeScene();scene->setBackground(false,plain);scene->setNominalSize(800,600);
        scene->addText("Written lesson survives a normal exit",QPointF(-200,-160));
        auto dc=UBDrawingController::drawingController();dc->setStylusTool(UBStylusTool::Pen);dc->setColorIndex(0);
        scene->inputDevicePress(QPointF(-100,0));scene->inputDeviceMove(QPointF(100,0));scene->inputDeviceRelease();
        int strokes=0;for(auto item:scene->items())if(item->type()==UBGraphicsStrokesGroup::Type)++strokes;
        if(!strokes){done(false,"The fixture did not write a pen stroke");return;}
        report["lessonId"]=doc->uuid().toString(QUuid::WithoutBraces);
        report["lessonPath"]=doc->persistencePath();report["strokeGroups"]=strokes;
        const bool quick=qEnvironmentVariable("LESSONBOARD_SAVE_QUICK")=="1";
        QTimer::singleShot(quick?0:25000,qApp,[history,report,reportPath,output,quick]() mutable {
            const auto recovery=history->rootPath()+"/"+report.value("lessonId").toString()+"/recovery.json";
            if(!quick&&!QFile::exists(recovery)){report["passed"]=false;report["detail"]="Automatic save did not create a recovery copy";write(reportPath,report);qApp->exit(31);return;}
            write(output.filePath("normal-save-state.json"),report);
            report["prepared"]=true;write(reportPath,report);
            // Follow the actual window Close path, not qApp->exit used by other tests.
            UBApplication::mainWindow->close();
        });return;
    }
    if(mode=="save-reopen"){
        if(history->uncleanSessionDetected()){done(false,"A normal window close was mistaken for a crash");return;}
        const auto state=read(output.filePath("normal-save-state.json"));
        auto doc=std::make_shared<UBDocumentProxy>(state.value("lessonPath").toString());
        auto scene=manager->loadDocumentScene(doc,0,false);
        int strokes=0;if(scene)for(auto item:scene->items())if(item->type()==UBGraphicsStrokesGroup::Type)++strokes;
        if(!scene||!sceneText(scene).contains("Written lesson survives a normal exit")
           ||strokes<state.value("strokeGroups").toInt()){
            done(false,"Writing or pen strokes were lost after normal exit and restart");return;
        }
        report["lessonId"]=state.value("lessonId");report["strokeGroups"]=strokes;
        done(true,"Normal window close saved text and pen strokes; restart found a clean session and no recovery prompt condition");return;
    }
    if(mode=="exercise"){
        if(!colourMemory(true,&error)){done(false,error);return;}
        report["colourWidthMemoryChecked"]=true;
        if(!coverWorkflow(tools,&error)||!penToolbarWorkflow(&error)){done(false,error);return;}
        report["coverToolbarChecked"]=true;report["penToolbarCustomizationChecked"]=true;
        auto dc=UBDrawingController::drawingController();
        auto doc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Lesson tools regression",true);board->setActiveDocumentScene(doc,0);auto scene=board->activeScene();scene->setBackground(false,plain);scene->setNominalSize(800,600);
        auto text=scene->addText("Version one",QPointF(-250,-200));auto cover=tools->addCover();auto second=tools->addCover();if(!cover||!second){done(false,"Cover creation failed");return;}cover->setCoverLocked(false);cover->setPos(-180,-80);cover->setScale(1);cover->setTransform(QTransform());second->setCoverLocked(false);second->setPos(180,100);second->setScale(.5);second->setRotation(15);tools->setCoverRevealed(second,true);
        dc->setStylusTool(UBStylusTool::Pen);dc->setColorIndex(0);scene->inputDevicePress(QPointF(-80,0));scene->inputDeviceMove(QPointF(80,0));scene->inputDeviceRelease();dc->setStylusTool(UBStylusTool::Selector);scene->clearSelection();
        auto render=[scene]{QImage im(800,600,QImage::Format_ARGB32_Premultiplied);im.fill(Qt::white);QPainter p(&im);scene->render(&p,QRectF(0,0,800,600),QRectF(-400,-300,800,600));p.end();return im;};
        // Simulate old/ordinary object-layer assignment, followed by selection and deselection.
        UBGraphicsItem::assignZValue(cover,-900000.);cover->setSelected(true);cover->setSelected(false);
        if(cover->zValue()<1500000||cover->data(UBGraphicsItemData::ItemOwnZValue).toDouble()!=cover->zValue()){done(false,"Cover visible and persisted layers diverged");return;}
        dc->setStylusTool(UBStylusTool::Pen);scene->inputDevicePress(QPointF(-80,20));scene->inputDeviceMove(QPointF(80,20));
        auto wet=render();if(wet.pixelColor(400,320)!=QColor("#334155")){done(false,"Writing in progress appeared over the cover");return;}
        scene->inputDeviceRelease();dc->setStylusTool(UBStylusTool::Selector);scene->clearSelection();
        auto covered=render();covered.save(output.filePath("cover-hidden.png"));if(covered.pixelColor(400,300)!=QColor("#334155")){done(false,"Cover did not conceal existing ink");return;}
        const auto before=cover->fileData();tools->setCoverRevealed(cover,true);if(!tools->isCoverRevealed(cover)){done(false,"Cover reveal failed");return;}auto revealed=render();revealed.save(output.filePath("cover-revealed.png"));if(revealed.pixelColor(400,300).lightness()>80){done(false,"Revealing cover did not expose handwriting");return;}
        if(!history->checkpoint(true,&error)){done(false,error);return;}const QString id=doc->uuid().toString(QUuid::WithoutBraces);auto versions=history->versions(id);QString first;for(const auto &v:versions)if(v.toObject().value("file").toString().startsWith("versions/")){first=v.toObject().value("file").toString();break;}if(first.isEmpty()){done(false,"Initial saved version missing");return;}
        text->setPlainText("Version two");scene->setModified(true);if(!history->checkpoint(true,&error)){done(false,error);return;}const int count=history->versions(id).size();if(count<3){done(false,"Second saved version missing");return;}if(!history->checkpoint(true,&error)||history->versions(id).size()!=count){done(false,"Unchanged lesson created redundant versions");return;}
        auto fresh=UBSvgSubsetAdaptor::loadScene(doc,0);int covers=0,reveals=0;for(auto item:fresh->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover()){++covers;if(tools->isCoverRevealed(svg))++reveals;if(svg->zValue()<=1000000){done(false,"Reopened cover lost its layer");return;}}if(covers!=2||reveals!=2){done(false,"Cover state did not persist across page reload");return;}
        auto restored=history->restoreCopy(id,first,&error);if(!restored||restored==doc||restored->persistencePath()==doc->persistencePath()){done(false,"Restore did not create a separate copy: "+error);return;}if(!sceneText(manager->loadDocumentScene(restored,0)).contains("Version one")||!sceneText(UBSvgSubsetAdaptor::loadScene(doc,0)).contains("Version two")){done(false,"Restore changed the wrong lesson version");return;}
        // A damaged history blob must fail safely without changing the live lesson.
        const auto manifest=read(history->rootPath()+"/"+id+"/"+first);const auto entry=manifest.value("files").toArray().first().toObject();const QString blob=history->rootPath()+"/"+id+"/blobs/"+entry.value("sha256").toString();QFile file(blob);if(!file.open(QIODevice::ReadOnly)){done(false,"History blob missing");return;}const auto bytes=file.readAll();file.close();if(!file.open(QIODevice::WriteOnly|QIODevice::Truncate)){done(false,"Cannot prepare damaged history test");return;}file.write("damaged");file.close();auto rejected=history->restoreCopy(id,first,&error);file.open(QIODevice::WriteOnly|QIODevice::Truncate);file.write(bytes);file.close();if(rejected||!sceneText(UBSvgSubsetAdaptor::loadScene(doc,0)).contains("Version two")){done(false,"Damaged history was not rejected safely");return;}
        report["lessonId"]=id;report["lessonPath"]=doc->persistencePath();report["versions"]=count;report["historyRoot"]=history->rootPath();write(output.filePath("lesson-tools-state.json"),report);
        done(true,"Per-colour Pen/Highlighter/Solid Line/Dashed Line thickness, toolbar synchronization, palette editing/background/alpha handling, cover occlusion/reveal/lock/reload, history deduplication, separate-copy restore and damaged-blob rejection passed");return;
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
void scheduleLessonToolsTest(QObject *context){
    const auto mode=qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_MODE");
    if(mode.startsWith("recovery-")){
        auto poll=new QTimer(context);poll->setInterval(80);QObject::connect(poll,&QTimer::timeout,context,[mode]{
            auto window=UBApplication::mainWindow;auto modal=QApplication::activeModalWidget();if(!modal)return;
            if(auto box=qobject_cast<QMessageBox*>(modal)){
                if(box->objectName()=="lessonRecoveryPrompt"){
                    box->grab().save(QFileInfo(qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_REPORT")).dir().filePath("recovery-prompt-window.png"));
                    if(mode=="recovery-delete"||mode=="recovery-delete-cancel")box->findChild<QPushButton*>("deleteAllRecovery")->click();
                    else box->button(mode=="recovery-review"?QMessageBox::Yes:QMessageBox::No)->click();
                }else if(box->objectName()=="confirmDeleteRecoveryHistory"){
                    const bool cancel=mode=="recovery-delete-cancel";window->setProperty(cancel?"recoveryDeleteCancelled":"recoveryDeleteConfirmed",true);
                    box->grab().save(QFileInfo(qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_REPORT")).dir().filePath("recovery-delete-window.png"));box->button(cancel?QMessageBox::Cancel:QMessageBox::Yes)->click();
                }
            }else if(modal->objectName()=="lessonRecoveryHistory"&&mode=="recovery-review"){
                auto list=modal->findChild<QListWidget*>();auto remove=modal->findChild<QPushButton*>("deleteAllRecoveryHistory");window->setProperty("recoveryReviewVisited",list&&list->count()>0&&remove&&remove->isEnabled());modal->grab().save(QFileInfo(qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_REPORT")).dir().filePath("recovery-history-window.png"));qobject_cast<QDialog*>(modal)->accept();
            }
        });poll->start();
    }
    QTimer::singleShot(120000,context,[]{qCritical("Lesson tools test timed out");qApp->exit(32);});QTimer::singleShot(7000,context,run);
}
