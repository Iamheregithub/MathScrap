// SPDX-License-Identifier: GPL-3.0-only
#include "UBLessonTools.h"
#include <QtWidgets>
#include <QtXml>
#include "gui/UBMainWindow.h"
#include "gui/UBStylusPalette.h"
#include "domain/UBGraphicsItemDelegate.h"
#include "domain/UBUndoCommand.h"
#include "core/UBApplication.h"
#include "core/UBApplicationController.h"
#include "core/UBSettings.h"
#include "core/UBLessonHistory.h"
#include "core/UBShortcutManager.h"
#include "board/UBBoardController.h"
#include "board/UBBoardView.h"
#include "board/UBDrawingController.h"
#include "domain/UBGraphicsScene.h"
#include "domain/UBGraphicsSvgItem.h"
#include "document/UBDocumentProxy.h"
namespace {
bool coverContainsOpaquePoint(UBGraphicsSvgItem *cover,const QPointF &point)
{
    QDomDocument doc;doc.setContent(cover->fileData());
    if(doc.elementsByTagName("metadata").at(0).toElement().text()=="revealed")return false;
    const auto r=doc.elementsByTagName("rect").at(0).toElement();if(r.attribute("fill")=="none")return false;
    return QRectF(r.attribute("x").toDouble(),r.attribute("y").toDouble(),r.attribute("width").toDouble(),r.attribute("height").toDouble()).contains(cover->mapFromScene(point));
}
}
UBLessonTools::UBLessonTools(UBMainWindow *window):QObject(window),mWindow(window) {
    setObjectName("lessonTools");mHistory=new UBLessonHistory(this);
    auto add=[this](const QString &name,const QString &label,const QIcon &icon){auto a=new QAction(icon,label,mWindow);a->setObjectName(name);mWindow->boardToolBar->insertAction(mWindow->actionBoard,a);mWindow->addAction(a);return a;};
    auto cover=mCoverAction=add("actionLessonCover",tr("Cover"),QIcon(":/images/teaching/cover.svg"));
    cover->setCheckable(true);cover->setShortcut(QKeySequence("Ctrl+Alt+C"));
    connect(mWindow->actionSelector,&QAction::triggered,this,[this]{
        cancelCoverDrag();mDrawingCover=false;mDrawCover->setChecked(false);mCoverBar->hide();mCoverAction->setChecked(false);
    });
    connect(cover,&QAction::triggered,this,[this](bool checked){
        if(checked){mCoverBar->show();armCover();}
        else{cancelCoverDrag();mDrawingCover=false;mCoverBar->hide();UBApplication::boardController->controlView()->viewport()->unsetCursor();}
    });
    createCoverToolbar();
    auto versions=add("actionLessonHistory",tr("Versions…"),QIcon(":/images/teaching/history.svg"));versions->setShortcut(QKeySequence("Ctrl+Alt+H"));connect(versions,&QAction::triggered,this,&UBLessonTools::showHistory);
    auto save=add("actionLessonSaveVersion",tr("Create saved version…"),QIcon(":/images/teaching/history.svg"));save->setShortcut(QKeySequence("Ctrl+Alt+S"));connect(save,&QAction::triggered,this,[this]{QString error;if(mHistory->checkpoint(true,&error,true))UBApplication::showMessage(tr("Lesson version saved."));else QMessageBox::warning(mWindow,tr("Create saved version"),error);});
    QList<QAction*> actions={cover,versions,save};UBShortcutManager::shortcutManager()->addActions(tr("Lesson tools"),actions,mWindow);
    const auto testMode=qEnvironmentVariable("LESSONBOARD_TOOLS_TEST_MODE");
    const bool testPrompt=testMode=="recovery-dismiss"||testMode=="recovery-dismiss-crash"||testMode=="recovery-delete"||testMode=="recovery-review"||testMode=="recovery-delete-cancel"||testMode=="recovery-failure-dismiss";
    if(mHistory->recoveryPromptPending()&&(!qApp->arguments().contains("--lesson-tools-test")||testPrompt))QTimer::singleShot(2000,this,[this]{
        if(!mHistory->recoveryPromptPending())return;
        QMessageBox prompt(QMessageBox::Question,tr("Recover lesson"),tr("Recovery copies from an earlier session are available. Review them now?"),QMessageBox::Yes|QMessageBox::No,mWindow);
        prompt.setObjectName("lessonRecoveryPrompt");prompt.button(QMessageBox::Yes)->setText(tr("Review copies"));prompt.button(QMessageBox::No)->setText(tr("Not now"));
        auto remove=prompt.addButton(tr("Delete all…"),QMessageBox::DestructiveRole);remove->setObjectName("deleteAllRecovery");
        prompt.setDefaultButton(QMessageBox::No);prompt.setEscapeButton(QMessageBox::No);mWindow->setProperty("recoveryPromptShown",true);prompt.exec();
        if(prompt.clickedButton()==remove){deleteAllHistory(mWindow);return;}
        QString error;if(!mHistory->acknowledgeRecoveryPrompt(&error)){QMessageBox::warning(mWindow,tr("Recovery choice"),error);return;}
        if(prompt.clickedButton()==prompt.button(QMessageBox::Yes))showHistory();
    });
}
UBGraphicsSvgItem *UBLessonTools::addCover(){if(UBApplication::applicationController->displayMode()!=UBApplicationController::Board)UBApplication::applicationController->showBoard();auto board=UBApplication::boardController;auto scene=board->activeScene();if(!scene)return nullptr;
    const QByteArray data="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"360\" height=\"160\" viewBox=\"0 0 360 160\"><metadata id=\"lessonboard-cover\">covered</metadata><rect x=\"2\" y=\"2\" width=\"356\" height=\"156\" rx=\"4\" fill=\"#334155\" stroke=\"#64748b\" stroke-width=\"2\"/></svg>";
    auto item=scene->addSvg(QUrl(),board->controlView()->mapToScene(board->controlView()->viewport()->rect().center()),data);if(item){if(mWindow->teachingTabs())mWindow->teachingTabs()->setCurrentIndex(0);mWindow->actionSelector->trigger();scene->clearSelection();item->setSelected(true);scene->updateSelectionFrame();item->setZValue(2000000.0);item->setCoverLocked(true);}return item;
}
bool UBLessonTools::isCoverRevealed(UBGraphicsSvgItem *cover)const{return cover&&cover->isLessonCover()&&cover->fileData().contains(">revealed</metadata>");}
void UBLessonTools::setCoverRevealed(UBGraphicsSvgItem *cover,bool revealed)
{
    if(!cover||!cover->isLessonCover())return;
    QDomDocument doc;doc.setContent(cover->fileData());
    auto metadata=doc.elementsByTagName("metadata").at(0).toElement();
    while(!metadata.firstChild().isNull())metadata.removeChild(metadata.firstChild());
    metadata.appendChild(doc.createTextNode(revealed?"revealed":"covered"));
    auto rect=doc.elementsByTagName("rect").at(0).toElement();
    const auto colour=doc.documentElement().attribute("data-cover-colour","#334155");
    rect.setAttribute("fill",revealed?"none":colour);
    rect.setAttribute("stroke-dasharray",revealed?"8 5":"none");
    rect.setAttribute("stroke-opacity",revealed?"0.45":"1");
    cover->setTeachingSvg(doc.toByteArray());
}

void UBLessonTools::createCoverToolbar()
{
    mCoverColour=UBSettings::settings()->value("Teaching/CoverColour",QColor("#e2e8f0")).value<QColor>();
    mCoverColour.setAlpha(255);
    mCoverBar=new QToolBar(tr("Cover controls"),mWindow);
    mCoverBar->setObjectName("lessonCoverToolbar");mCoverBar->setMovable(false);mCoverBar->setFloatable(false);
    mCoverBar->setStyleSheet("QToolBar {spacing:6px;padding:4px;} QToolButton {min-height:32px;padding:4px;}");
    mWindow->addToolBarBreak(Qt::TopToolBarArea);mWindow->addToolBar(Qt::TopToolBarArea,mCoverBar);
    mDrawCover=mCoverBar->addAction(tr("Draw cover"));mDrawCover->setCheckable(true);mDrawCover->setObjectName("lessonDrawCover");
    connect(mDrawCover,&QAction::triggered,this,&UBLessonTools::armCover);
    auto shade=mCoverBar->addAction(tr("Screen shade"));shade->setObjectName("lessonScreenShade");
    connect(shade,&QAction::triggered,this,[this]{
        cancelCoverDrag();mDrawingCover=false;mDrawCover->setChecked(false);
        UBApplication::boardController->controlView()->viewport()->unsetCursor();
        mWindow->actionSelector->trigger();
        auto board=UBApplication::boardController;auto scene=board->activeScene();if(!scene)return;
        for(auto item:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->fileData().contains("data-screen-shade")){
            svg->setCoverLocked(true);scene->clearSelection();svg->setSelected(true);scene->updateSelectionFrame();setCoverRevealed(svg,false);mCoverBar->show();return;
        }
        auto view=board->controlView();createCover(view->mapToScene(view->viewport()->rect()).boundingRect(),true);mCoverBar->show();
    });
    auto command=[this](const QString &name,const QString &text){auto a=new QAction(text,mWindow);a->setObjectName(name);a->setProperty("coverControl",true);mWindow->addAction(a);return a;};
    auto colour=command("lessonCoverColour",tr("Cover colour"));colour->setObjectName("lessonCoverColour");
    connect(colour,&QAction::triggered,this,[this]{
        auto c=QColorDialog::getColor(mCoverColour,mWindow,tr("Cover colour"));if(!c.isValid())return;c.setAlpha(255);mCoverColour=c;
        UBSettings::settings()->setValue("Teaching/CoverColour",c);UBSettings::settings()->save();
        auto scene=UBApplication::boardController->activeScene();if(!scene)return;
        for(auto item:scene->selectedItems())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover()){
            auto before=svg->fileData();QDomDocument doc;doc.setContent(before);doc.documentElement().setAttribute("data-cover-colour",c.name());
            if(!isCoverRevealed(svg))doc.elementsByTagName("rect").at(0).toElement().setAttribute("fill",c.name());
            svg->setTeachingSvg(doc.toByteArray());
        }
    });
    auto close=mCoverBar->addAction(tr("Close"));close->setObjectName("lessonCoverClose");connect(close,&QAction::triggered,this,[this]{cancelCoverDrag();mDrawingCover=false;mDrawCover->setChecked(false);mCoverAction->setChecked(false);mCoverBar->hide();UBApplication::boardController->controlView()->viewport()->unsetCursor();});
    connect(UBApplication::boardController,&UBBoardController::activeSceneChanged,this,[this]{cancelCoverDrag();mDrawingCover=false;mDrawCover->setChecked(false);UBApplication::boardController->controlView()->viewport()->unsetCursor();});
    auto stop=[this]{cancelCoverDrag();mDrawingCover=false;mDrawCover->setChecked(false);mCoverBar->hide();mCoverAction->setChecked(false);UBApplication::boardController->controlView()->viewport()->unsetCursor();};
    connect(UBDrawingController::drawingController(),&UBDrawingController::stylusToolChanged,this,[stop](int tool,int){if(tool!=UBStylusTool::Selector)stop();});
    connect(mWindow->teachingTabs(),&QTabWidget::currentChanged,this,[stop](int index){if(index!=0)stop();});
    connect(mWindow->boardToolBar,&QToolBar::visibilityChanged,this,[stop](bool visible){if(!visible)stop();});
    connect(qApp,&QCoreApplication::aboutToQuit,this,[this]{cancelCoverDrag();qApp->removeEventFilter(this);});
    connect(UBApplication::boardController,&UBBoardController::activeSceneChanged,this,[this]{applyTouchMode();});
    applyTouchMode();
    qApp->installEventFilter(this);mCoverBar->hide();
}

void UBLessonTools::armCover()
{
    if(auto shapes=mWindow->findChild<QAction*>("actionTeachingShapes");shapes&&shapes->isChecked())shapes->trigger();
    mWindow->teachingTabs()->setCurrentIndex(0);
    cancelCoverDrag();mWindow->actionSelector->trigger();mDrawingCover=true;mDrawCover->setChecked(true);mCoverAction->setChecked(true);mCoverBar->show();
    UBApplication::boardController->controlView()->viewport()->setCursor(Qt::CrossCursor);
}

void UBLessonTools::cancelCoverDrag()
{
    if(mCoverPreview){if(mCoverScene)mCoverScene->QGraphicsScene::removeItem(mCoverPreview);delete mCoverPreview;mCoverPreview=nullptr;}
    mCoverScene.clear();
    if(mShadeDrag)mShadeDrag->setTeachingSvg(mShadeBefore);
    if(mMoveCover)mMoveCover->setPos(mMoveOriginal);
    mShadeDrag.clear();mMoveCover.clear();mButtonCover.clear();mLockedPress.clear();
}

UBGraphicsSvgItem *UBLessonTools::createCover(const QRectF &rect,bool shade)
{
    auto scene=UBApplication::boardController->activeScene();if(!scene)return nullptr;
    const auto svg=QString("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%2\" viewBox=\"0 0 %1 %2\" data-cover-colour=\"%3\" %4><metadata id=\"lessonboard-cover\">covered</metadata><rect x=\"0\" y=\"0\" width=\"%1\" height=\"%2\" fill=\"%3\" stroke=\"#64748b\" stroke-width=\"2\"/></svg>")
        .arg(rect.width()).arg(rect.height()).arg(mCoverColour.name()).arg(shade?"data-screen-shade=\"true\"":"").toUtf8();
    QDomDocument document;document.setContent(svg);
    if(shade){document.documentElement().setAttribute("data-shade-home-x",rect.x());document.documentElement().setAttribute("data-shade-home-y",rect.y());}
    auto item=scene->addSvg(QUrl(),rect.center(),document.toByteArray());if(!item)return nullptr;
    item->setScale(1);item->setTransform(QTransform());item->setPos(rect.topLeft());item->setZValue(2000000.0);
    item->setCoverLocked(true);
    scene->clearSelection();item->setSelected(true);scene->updateSelectionFrame();return item;
}

void UBLessonTools::applyTouchMode()
{
    const bool touch=UBSettings::settings()->value("Teaching/TouchFriendly",false).toBool();
    for(auto bar:mWindow->findChildren<QToolBar*>()){
        if(bar!=mWindow->boardToolBar&&bar->objectName()!="lessonCoverToolbar"&&bar->objectName()!="lessonShapeToolbar")continue;
        if(!bar->property("lessonOriginalStyle").isValid()){
            bar->setProperty("lessonOriginalStyle",bar->styleSheet());bar->setProperty("lessonOriginalIconSize",bar->iconSize());
        }
        bar->setIconSize(touch?QSize(40,40):bar->property("lessonOriginalIconSize").toSize());
        bar->setStyleSheet(bar->property("lessonOriginalStyle").toString()+(touch?QString(" QToolButton {min-height:48px;min-width:44px;padding:6px;} QComboBox {min-height:40px;}"):QString()));
    }
    for(auto palette:mWindow->findChildren<UBStylusPalette*>()){
        palette->setButtonIconSize(touch?QSize(56,56):QSize(42,42));palette->layout()->invalidate();palette->adjustSize();palette->initPosition();
    }
    if(auto scene=UBApplication::boardController->activeScene())for(auto item:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover()){
        // Runtime-only controls preserve legacy SVG assets on a simple open/save.
        svg->setCoverLocked(svg->isScreenShade()||svg->Delegate()->isLocked());
    }
}

bool UBLessonTools::eventFilter(QObject *watched,QEvent *event)
{
    auto board=UBApplication::boardController;if(!board||!board->controlView())return false;
    auto view=board->controlView();
    if(event->type()==QEvent::Move||event->type()==QEvent::Resize||event->type()==QEvent::Show||event->type()==QEvent::Hide){
        if(auto panel=qobject_cast<QWidget*>(watched);panel&&QStringList{"paperDock","paperPenPanel","lessonCoverToolbar","lessonShapeToolbar","paperPages"}.contains(panel->objectName())){
            QTimer::singleShot(0,this,[]{if(auto scene=UBApplication::boardController->activeScene())for(auto item:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover())svg->refreshCoverControls();});
        }
    }
    if(event->type()==QEvent::KeyPress&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape&&(mDrawingCover||mShadeDrag||mMoveCover||mButtonCover||mLockedPress)){
        cancelCoverDrag();mDrawingCover=false;mDrawCover->setChecked(false);view->viewport()->unsetCursor();return true;
    }
    const auto type=event->type();
    const bool tablet=type==QEvent::TabletPress||type==QEvent::TabletMove||type==QEvent::TabletRelease;
    const bool mouse=type==QEvent::MouseButtonPress||type==QEvent::MouseMove||type==QEvent::MouseButtonRelease||type==QEvent::MouseButtonDblClick;
    if((watched!=view->viewport()&&!(tablet&&watched==view))||(!mouse&&!tablet)||mWindow->teachingTabs()->currentIndex()!=0)return false;
    QPointF position;
    const bool press=type==QEvent::MouseButtonPress||type==QEvent::TabletPress;
    const bool release=type==QEvent::MouseButtonRelease||type==QEvent::TabletRelease;
    if(tablet){auto e=static_cast<QTabletEvent*>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        position=view->viewport()->mapFromGlobal(e->globalPosition().toPoint());
#else
        position=view->viewport()->mapFromGlobal(e->globalPosF().toPoint());
#endif
    }else{auto e=static_cast<QMouseEvent*>(event);if(type!=QEvent::MouseMove&&e->button()!=Qt::LeftButton)return false;position=e->pos();}
    // Tablet packets aimed at floating toolbars can be forwarded to the viewport.
    // Only an already-started gesture may continue outside the lesson canvas.
    if(!QRectF(view->viewport()->rect()).contains(position)&&!mShadeDrag&&!mMoveCover&&!mButtonCover&&!mCoverPreview&&!mLockedPress)return false;
    auto scene=board->activeScene();if(!scene)return false;const auto point=view->mapToScene(position.toPoint());
    if(mButtonCover){
        if(release){auto target=mButtonCover;const auto role=mButtonRole;mButtonCover.clear();
            if(target&&target->scene()==scene&&mButtonRect.contains(position)){
                if(role=="close")target->Delegate()->remove(false);
                else{
                    const bool unlock=target->Delegate()->isLocked();target->setCoverLocked(!unlock);
                    if(unlock){mWindow->actionSelector->trigger();scene->clearSelection();target->setSelected(true);scene->updateSelectionFrame();}
                }
            }
        }
        event->accept();return true;
    }
    // Full-size on-object buttons work with pen and mouse, regardless of tool.
    if(press)for(auto item:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover()){
        const QString role=svg->isScreenShade()?"close":"lock";
        if(svg->coverControlRect(role).contains(position)){mButtonCover=svg;mButtonRole=role;mButtonRect=svg->coverControlRect(role);event->accept();return true;}
        if(svg->coverControlRect("grip").contains(position)){
            mShadeDrag=svg;mShadeBefore=svg->fileData();mShadeStart=svg->mapFromScene(point);
            scene->clearSelection();break;
        }
        // Children of an older cover are painted below a newer opaque cover.
        // Their invisible controls must not receive a click through that cover.
        if(coverContainsOpaquePoint(svg,point))break;
    }
    if(mShadeDrag){
        QDomDocument doc;doc.setContent(mShadeBefore);auto rect=doc.elementsByTagName("rect").at(0).toElement();
        const double bottom=rect.attribute("y").toDouble()+rect.attribute("height").toDouble();
        auto metadata=doc.elementsByTagName("metadata").at(0).toElement();
        const bool wasRevealed=metadata.text()=="revealed";
        const double initialTop=wasRevealed?qMax(0.,bottom-8):rect.attribute("y").toDouble();
        const double top=qBound(0.,initialTop+mShadeDrag->mapFromScene(point).y()-mShadeStart.y(),qMax(0.,bottom-8));
        if(wasRevealed){while(!metadata.firstChild().isNull())metadata.removeChild(metadata.firstChild());metadata.appendChild(doc.createTextNode("covered"));rect.setAttribute("fill",doc.documentElement().attribute("data-cover-colour","#334155"));rect.setAttribute("stroke-dasharray","none");rect.setAttribute("stroke-opacity","1");}
        rect.setAttribute("y",top);rect.setAttribute("height",bottom-top);
        mShadeDrag->setTeachingSvg(doc.toByteArray());
        if(release)mShadeDrag.clear();
        event->accept();return true;
    }
    if(mDrawingCover){
        if(press){cancelCoverDrag();mCoverStart=point;mCoverScene=scene.get();mCoverPreview=new QGraphicsRectItem;mCoverPreview->setZValue(2000001);mCoverPreview->setPen(QPen(QColor("#64748b"),2));mCoverPreview->setBrush(mCoverColour);scene->QGraphicsScene::addItem(mCoverPreview);}
        if(mCoverPreview&&mCoverScene==scene.get()){
            const auto rect=QRectF(mCoverStart,point).normalized();mCoverPreview->setRect(rect);
            if(release){cancelCoverDrag();if(rect.width()>=3&&rect.height()>=3){createCover(rect,false);mWindow->actionSelector->trigger();mCoverBar->show();}}
        }
        event->accept();return true;
    }
    if(UBDrawingController::drawingController()->stylusTool()!=UBStylusTool::Selector)return false;
    // Shapes and Functions reuse the selection engine internally. Their canvas
    // gestures remain drawing gestures; only the real Select tool moves covers.
    if(!mWindow->actionSelector->isChecked())return false;
    if(type==QEvent::MouseButtonDblClick){
        for(auto item:scene->items(point))if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover()&&coverContainsOpaquePoint(svg,point)){event->accept();return true;}
    }
    if(mMoveCover){
        mMoveCover->setPos(mMoveOriginal+point-mMoveStart);
        if(release){mMoveCover->refreshCoverControls();mMoveCover.clear();}
        event->accept();return true;
    }
    if(press)for(auto item:scene->items(point))if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover()&&coverContainsOpaquePoint(svg,point)){
        scene->clearSelection();svg->setSelected(true);scene->updateSelectionFrame();
        if(svg->isScreenShade()||svg->Delegate()->isLocked())mLockedPress=svg;
        else{mMoveCover=svg;mMoveStart=point;mMoveOriginal=svg->pos();}
        event->accept();return true;
    }
    if(mLockedPress){if(release)mLockedPress.clear();event->accept();return true;}
    return false;
}

bool UBLessonTools::deleteAllHistory(QWidget *parent)
{
    QMessageBox confirm(QMessageBox::Warning,tr("Delete recovery history"),tr("Delete all recovery copies and saved versions? This cannot be undone. Your saved lessons in Documents will remain. New work will receive new recovery copies."),QMessageBox::Yes|QMessageBox::Cancel,parent);
    confirm.setObjectName("confirmDeleteRecoveryHistory");confirm.button(QMessageBox::Yes)->setText(tr("Delete all"));confirm.setDefaultButton(QMessageBox::Cancel);
    if(confirm.exec()!=QMessageBox::Yes)return false;
    QString error;if(!mHistory->clearHistory(&error)){QMessageBox::warning(parent,tr("Cannot delete recovery history"),error);return false;}
    return true;
}
void UBLessonTools::showHistory(){QDialog dialog(mWindow);dialog.setObjectName("lessonRecoveryHistory");dialog.setWindowTitle(tr("Lesson recovery and version history"));auto layout=new QVBoxLayout(&dialog);auto info=new QLabel(tr("Recovery copies save every 20 seconds while you are not drawing. Versions are kept every 5 minutes when content changes, on exit, and when you create a saved version. The latest recovery plus 50 versions per lesson are retained. Restore always opens a separate copy."));info->setWordWrap(true);layout->addWidget(info);auto lessons=new QComboBox;for(const auto &v:mHistory->documents()){auto d=v.toObject();lessons->addItem(d.value("name").toString(),d.value("id").toString());}layout->addWidget(lessons);auto list=new QListWidget;layout->addWidget(list);auto fill=[=]{list->clear();for(const auto &v:mHistory->versions(lessons->currentData().toString())){auto entry=v.toObject();const auto date=QDateTime::fromString(entry.value("created").toString(),Qt::ISODateWithMs).toLocalTime();auto item=new QListWidgetItem((entry.value("file").toString()=="recovery.json"?tr("Latest recovery — "):QString())+date.toString("yyyy-MM-dd HH:mm:ss"),list);item->setData(Qt::UserRole,entry.value("file").toString());}if(list->count())list->setCurrentRow(0);};connect(lessons,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[fill](int){fill();});auto current=UBApplication::boardController->selectedDocument();if(current){const int index=lessons->findData(current->uuid().toString(QUuid::WithoutBraces));if(index>=0)lessons->setCurrentIndex(index);}fill();
    if(!mHistory->lastError().isEmpty())layout->addWidget(new QLabel(mHistory->lastError()));auto restore=new QPushButton(tr("Restore selected version as a new lesson"));restore->setEnabled(list->count()>0);connect(list,&QListWidget::currentRowChanged,restore,[restore](int index){restore->setEnabled(index>=0);});layout->addWidget(restore);connect(restore,&QPushButton::clicked,&dialog,[=,&dialog]{if(!list->currentItem())return;QString error;auto copy=mHistory->restoreCopy(lessons->currentData().toString(),list->currentItem()->data(Qt::UserRole).toString(),&error);if(!copy){QMessageBox::warning(&dialog,tr("Cannot restore version"),error);return;}UBApplication::boardController->setActiveDocumentScene(copy,0);UBApplication::applicationController->showBoard();dialog.accept();});auto buttons=new QDialogButtonBox(QDialogButtonBox::Close);auto remove=buttons->addButton(tr("Delete all history…"),QDialogButtonBox::DestructiveRole);remove->setObjectName("deleteAllRecoveryHistory");remove->setEnabled(lessons->count()>0);
    connect(remove,&QPushButton::clicked,&dialog,[this,&dialog,lessons,list,restore,remove]{if(deleteAllHistory(&dialog)){lessons->clear();list->clear();restore->setEnabled(false);remove->setEnabled(false);}});
    layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);dialog.resize(600,420);dialog.exec();}
