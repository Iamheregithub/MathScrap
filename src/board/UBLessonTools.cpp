// SPDX-License-Identifier: GPL-3.0-only
#include "UBLessonTools.h"
#include <QtWidgets>
#include <QtXml>
#include "gui/UBMainWindow.h"
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
// Handles are part of the saved SVG, so they survive page changes and reloads.
void updateShadeHandles(QDomDocument &doc)
{
    auto root=doc.documentElement();if(!root.hasAttribute("data-screen-shade"))return;
    auto rect=doc.elementsByTagName("rect").at(0).toElement();
    const double x=rect.attribute("x").toDouble(),y=rect.attribute("y").toDouble();
    const double w=rect.attribute("width").toDouble(),h=rect.attribute("height").toDouble();
    const double dx=qMin(18.,w/4),dy=qMin(18.,h/4);
    auto handles=doc.elementsByTagName("path").at(0).toElement();
    if(handles.isNull()){handles=doc.createElement("path");root.appendChild(handles);}
    handles.setAttribute("d",QString("M %1 %2 h %3 M %1 %4 h %3 M %5 %6 v %7 M %8 %6 v %7")
        .arg(x+w/2-dx).arg(y+3).arg(2*dx).arg(y+h-3).arg(x+3).arg(y+h/2-dy).arg(2*dy).arg(x+w-3));
    handles.setAttribute("stroke","#475569");handles.setAttribute("stroke-width","5");
    handles.setAttribute("stroke-linecap","round");handles.setAttribute("fill","none");
    handles.setAttribute("opacity",doc.elementsByTagName("metadata").at(0).toElement().text()=="revealed"?"0.3":"1");
}
}

UBLessonTools::UBLessonTools(UBMainWindow *window):QObject(window),mWindow(window) {
    setObjectName("lessonTools");mHistory=new UBLessonHistory(this);
    auto add=[this](const QString &name,const QString &label,const QIcon &icon){auto a=new QAction(icon,label,mWindow);a->setObjectName(name);mWindow->boardToolBar->insertAction(mWindow->actionBoard,a);mWindow->addAction(a);return a;};
    auto cover=mCoverAction=add("actionLessonCover",tr("Cover"),QIcon(":/images/teaching/cover.svg"));
    cover->setCheckable(true);cover->setShortcut(QKeySequence("Ctrl+Alt+C"));
    connect(cover,&QAction::triggered,this,[this](bool checked){
        if(checked){mCoverBar->show();armCover();}
        else{cancelCoverDrag();mDrawingCover=false;mCoverBar->hide();UBApplication::boardController->controlView()->viewport()->unsetCursor();}
    });
    auto reveal=new QAction(tr("Reveal / cover"),mWindow);reveal->setObjectName("actionLessonReveal");mWindow->addAction(reveal);reveal->setShortcut(QKeySequence("Ctrl+Alt+R"));connect(reveal,&QAction::triggered,this,[this]{auto scene=UBApplication::boardController->activeScene();if(!scene)return;bool found=false;UBApplication::undoStack->beginMacro(tr("Reveal covers"));for(auto item:scene->selectedItems())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover()){setCoverRevealed(svg,!isCoverRevealed(svg));found=true;}UBApplication::undoStack->endMacro();if(!found)manageCovers();});
    createCoverToolbar();
    auto versions=add("actionLessonHistory",tr("Versions…"),QIcon(":/images/teaching/history.svg"));versions->setShortcut(QKeySequence("Ctrl+Alt+H"));connect(versions,&QAction::triggered,this,&UBLessonTools::showHistory);
    auto save=add("actionLessonSaveVersion",tr("Save version"),QIcon(":/images/teaching/history.svg"));save->setShortcut(QKeySequence("Ctrl+Alt+S"));connect(save,&QAction::triggered,this,[this]{QString error;if(mHistory->checkpoint(true,&error))UBApplication::showMessage(tr("Lesson version saved."));else QMessageBox::warning(mWindow,tr("Save version"),error);});
    QList<QAction*> actions={cover,reveal,versions,save};UBShortcutManager::shortcutManager()->addActions(tr("Lesson tools"),actions,mWindow);
    if(mHistory->uncleanSessionDetected()&&!qApp->arguments().contains("--lesson-tools-test"))QTimer::singleShot(2000,this,[this]{if(QMessageBox::question(mWindow,tr("Recover lesson"),tr("LessonBoard did not close normally. Saved recovery copies are available. Review them now?"),QMessageBox::Yes|QMessageBox::No)==QMessageBox::Yes)showHistory();});
}
UBGraphicsSvgItem *UBLessonTools::addCover(){if(UBApplication::applicationController->displayMode()!=UBApplicationController::Board)UBApplication::applicationController->showBoard();auto board=UBApplication::boardController;auto scene=board->activeScene();if(!scene)return nullptr;
    const QByteArray data="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"360\" height=\"160\" viewBox=\"0 0 360 160\"><metadata id=\"lessonboard-cover\">covered</metadata><rect x=\"2\" y=\"2\" width=\"356\" height=\"156\" rx=\"4\" fill=\"#334155\" stroke=\"#64748b\" stroke-width=\"2\"/></svg>";
    auto item=scene->addSvg(QUrl(),board->controlView()->mapToScene(board->controlView()->viewport()->rect().center()),data);if(item){if(mWindow->teachingTabs())mWindow->teachingTabs()->setCurrentIndex(0);mWindow->actionSelector->trigger();scene->clearSelection();item->setSelected(true);item->setZValue(2000000.0);}return item;
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
    updateShadeHandles(doc);
    const auto before=cover->fileData();cover->setTeachingSvg(doc.toByteArray());cover->commitTeachingSvg(before);
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
            scene->clearSelection();svg->setSelected(true);setCoverRevealed(svg,false);return;
        }
        auto view=board->controlView();createCover(view->mapToScene(view->viewport()->rect()).boundingRect(),true);
    });
    mCoverBar->addSeparator();
    for(bool selected:{true,false})for(bool revealed:{true,false}){
        auto a=mCoverBar->addAction(selected?(revealed?tr("Reveal selected"):tr("Hide selected")):(revealed?tr("Reveal all"):tr("Hide all")));
        connect(a,&QAction::triggered,this,[this,selected,revealed]{revealCovers(selected,revealed);});
    }
    auto colour=mCoverBar->addAction(tr("Cover colour"));colour->setObjectName("lessonCoverColour");
    connect(colour,&QAction::triggered,this,[this]{
        auto c=QColorDialog::getColor(mCoverColour,mWindow,tr("Cover colour"));if(!c.isValid())return;c.setAlpha(255);mCoverColour=c;
        UBSettings::settings()->setValue("Teaching/CoverColour",c);UBSettings::settings()->sync();
        auto scene=UBApplication::boardController->activeScene();if(!scene)return;
        UBApplication::undoStack->beginMacro(tr("Cover colour"));
        for(auto item:scene->selectedItems())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover()){
            auto before=svg->fileData();QDomDocument doc;doc.setContent(before);doc.documentElement().setAttribute("data-cover-colour",c.name());
            if(!isCoverRevealed(svg))doc.elementsByTagName("rect").at(0).toElement().setAttribute("fill",c.name());
            svg->setTeachingSvg(doc.toByteArray());svg->commitTeachingSvg(before);
        }
        UBApplication::undoStack->endMacro();
    });
    auto close=mCoverBar->addAction(tr("Close"));connect(close,&QAction::triggered,mCoverAction,&QAction::trigger);
    connect(UBApplication::boardController,&UBBoardController::activeSceneChanged,this,[this]{cancelCoverDrag();mDrawingCover=false;mDrawCover->setChecked(false);UBApplication::boardController->controlView()->viewport()->unsetCursor();});
    auto stop=[this]{cancelCoverDrag();mDrawingCover=false;mDrawCover->setChecked(false);mCoverBar->hide();mCoverAction->setChecked(false);UBApplication::boardController->controlView()->viewport()->unsetCursor();};
    connect(UBDrawingController::drawingController(),&UBDrawingController::stylusToolChanged,this,[stop](int tool,int){if(tool!=UBStylusTool::Selector)stop();});
    connect(mWindow->teachingTabs(),&QTabWidget::currentChanged,this,[stop](int index){if(index!=0)stop();});
    connect(mWindow->boardToolBar,&QToolBar::visibilityChanged,this,[stop](bool visible){if(!visible)stop();});
    connect(qApp,&QCoreApplication::aboutToQuit,this,[this]{cancelCoverDrag();qApp->removeEventFilter(this);});
    qApp->installEventFilter(this);mCoverBar->hide();
}

void UBLessonTools::armCover()
{
    if(auto shapes=mWindow->findChild<QAction*>("actionTeachingShapes");shapes&&shapes->isChecked())shapes->trigger();
    mWindow->teachingTabs()->setCurrentIndex(0);
    cancelCoverDrag();mWindow->actionSelector->trigger();mDrawingCover=true;mDrawCover->setChecked(true);
    UBApplication::boardController->controlView()->viewport()->setCursor(Qt::CrossCursor);
}

void UBLessonTools::cancelCoverDrag()
{
    if(mCoverPreview){if(mCoverScene)mCoverScene->QGraphicsScene::removeItem(mCoverPreview);delete mCoverPreview;mCoverPreview=nullptr;}
    mCoverScene.clear();
    if(mShadeDrag)mShadeDrag->setTeachingSvg(mShadeBefore);
    mShadeDrag.clear();mShadeEdge=-1;
}

UBGraphicsSvgItem *UBLessonTools::createCover(const QRectF &rect,bool shade)
{
    auto scene=UBApplication::boardController->activeScene();if(!scene)return nullptr;
    const auto svg=QString("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%2\" viewBox=\"0 0 %1 %2\" data-cover-colour=\"%3\" %4><metadata id=\"lessonboard-cover\">covered</metadata><rect x=\"0\" y=\"0\" width=\"%1\" height=\"%2\" fill=\"%3\" stroke=\"#64748b\" stroke-width=\"2\"/></svg>")
        .arg(rect.width()).arg(rect.height()).arg(mCoverColour.name()).arg(shade?"data-screen-shade=\"true\"":"").toUtf8();
    QDomDocument document;document.setContent(svg);updateShadeHandles(document);
    auto item=scene->addSvg(QUrl(),rect.center(),document.toByteArray());if(!item)return nullptr;
    item->setScale(1);item->setTransform(QTransform());item->setPos(rect.topLeft());item->setZValue(2000000.0);
    scene->clearSelection();item->setSelected(true);return item;
}

void UBLessonTools::revealCovers(bool selected,bool revealed)
{
    auto scene=UBApplication::boardController->activeScene();if(!scene)return;
    UBApplication::undoStack->beginMacro(revealed?tr("Reveal covers"):tr("Hide covers"));
    for(auto item:selected?scene->selectedItems():scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover())setCoverRevealed(svg,revealed);
    UBApplication::undoStack->endMacro();
}

bool UBLessonTools::eventFilter(QObject *watched,QEvent *event)
{
    auto board=UBApplication::boardController;if(!board||!board->controlView())return false;
    auto view=board->controlView();
    if(event->type()==QEvent::KeyPress&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape&&(mDrawingCover||mShadeDrag)){
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
        position=e->position();
#else
        position=e->posF();
#endif
    }else{auto e=static_cast<QMouseEvent*>(event);if(type!=QEvent::MouseMove&&e->button()!=Qt::LeftButton)return false;position=e->pos();}
    auto scene=board->activeScene();if(!scene)return false;auto point=view->mapToScene(position.toPoint());
    if(mDrawingCover){
        if(press){cancelCoverDrag();mCoverStart=point;mCoverScene=scene.get();mCoverPreview=new QGraphicsRectItem;mCoverPreview->setZValue(2000001);mCoverPreview->setPen(QPen(QColor("#64748b"),2));mCoverPreview->setBrush(mCoverColour);scene->QGraphicsScene::addItem(mCoverPreview);}
        if(mCoverPreview&&mCoverScene==scene.get()){
            const auto rect=QRectF(mCoverStart,point).normalized();mCoverPreview->setRect(rect);
            if(release){cancelCoverDrag();if(rect.width()>=3&&rect.height()>=3){createCover(rect,false);mDrawingCover=false;mDrawCover->setChecked(false);view->viewport()->unsetCursor();}}
        }
        event->accept();return true;
    }
    if(UBDrawingController::drawingController()->stylusTool()!=UBStylusTool::Selector)return false;
    if(type==QEvent::MouseButtonDblClick){
        for(auto item:scene->items(point))if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover()){
            setCoverRevealed(svg,!isCoverRevealed(svg));event->accept();return true;
        }
    }
    if(press){
        for(auto item:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->fileData().contains("data-screen-shade")&&!isCoverRevealed(svg)){
            QDomDocument doc;doc.setContent(svg->fileData());auto r=doc.elementsByTagName("rect").at(0).toElement();
            QRectF rect(r.attribute("x").toDouble(),r.attribute("y").toDouble(),r.attribute("width").toDouble(),r.attribute("height").toDouble());
            const auto p=svg->mapFromScene(point);const double tolerance=14/qMax(.01,view->transform().m11()*svg->sceneTransform().m11());
            if(!rect.adjusted(-tolerance,-tolerance,tolerance,tolerance).contains(p))continue;
            const double distances[]={qAbs(p.x()-rect.left()),qAbs(p.x()-rect.right()),qAbs(p.y()-rect.top()),qAbs(p.y()-rect.bottom())};
            for(int i=0;i<4;++i)if(distances[i]<=tolerance){mShadeDrag=svg;mShadeBefore=svg->fileData();mShadeEdge=i;scene->clearSelection();break;}
            break;
        }
    }
    if(mShadeDrag){
        QDomDocument doc;doc.setContent(mShadeBefore);auto r=doc.elementsByTagName("rect").at(0).toElement();auto root=doc.documentElement();
        QRectF rect(r.attribute("x").toDouble(),r.attribute("y").toDouble(),r.attribute("width").toDouble(),r.attribute("height").toDouble());
        auto p=mShadeDrag->mapFromScene(point);const double w=root.attribute("width").toDouble(),h=root.attribute("height").toDouble();
        if(mShadeEdge==0)rect.setLeft(qBound(0.,p.x(),rect.right()-8));
        if(mShadeEdge==1)rect.setRight(qBound(rect.left()+8,p.x(),w));
        if(mShadeEdge==2)rect.setTop(qBound(0.,p.y(),rect.bottom()-8));
        if(mShadeEdge==3)rect.setBottom(qBound(rect.top()+8,p.y(),h));
        r.setAttribute("x",rect.x());r.setAttribute("y",rect.y());r.setAttribute("width",rect.width());r.setAttribute("height",rect.height());
        updateShadeHandles(doc);mShadeDrag->setTeachingSvg(doc.toByteArray());
        if(release){mShadeDrag->commitTeachingSvg(mShadeBefore);mShadeDrag.clear();mShadeEdge=-1;}
        event->accept();return true;
    }
    return false;
}

void UBLessonTools::manageCovers(){QDialog dialog(mWindow);dialog.setWindowTitle(tr("Page reveal covers"));auto layout=new QVBoxLayout(&dialog);auto info=new QLabel(tr("Unchecked covers hide the page. Check a cover to reveal it. Use the selection tool to move, resize or rotate covers."));info->setWordWrap(true);layout->addWidget(info);auto list=new QListWidget;layout->addWidget(list);QList<QPointer<UBGraphicsSvgItem>> covers;auto scene=UBApplication::boardController->activeScene();if(scene)for(auto item:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isLessonCover()){covers<<svg;auto row=new QListWidgetItem(tr("Cover %1 — revealed").arg(covers.size()),list);row->setFlags(row->flags()|Qt::ItemIsUserCheckable);row->setCheckState(isCoverRevealed(svg)?Qt::Checked:Qt::Unchecked);}
    connect(list,&QListWidget::itemChanged,&dialog,[=](QListWidgetItem *row){const int index=list->row(row);if(index>=0&&index<covers.size()&&covers[index])setCoverRevealed(covers[index],row->checkState()==Qt::Checked);});auto buttons=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);dialog.resize(450,300);dialog.exec();}
void UBLessonTools::showHistory(){QDialog dialog(mWindow);dialog.setWindowTitle(tr("Lesson recovery and version history"));auto layout=new QVBoxLayout(&dialog);auto info=new QLabel(tr("Recovery copies save every 20 seconds while you are not drawing. Versions are kept every 5 minutes when content changes, on exit, and with Save version. The latest recovery plus 50 versions per lesson are retained. Restore always opens a separate copy."));info->setWordWrap(true);layout->addWidget(info);auto lessons=new QComboBox;for(const auto &v:mHistory->documents()){auto d=v.toObject();lessons->addItem(d.value("name").toString(),d.value("id").toString());}layout->addWidget(lessons);auto list=new QListWidget;layout->addWidget(list);auto fill=[=]{list->clear();for(const auto &v:mHistory->versions(lessons->currentData().toString())){auto entry=v.toObject();const auto date=QDateTime::fromString(entry.value("created").toString(),Qt::ISODateWithMs).toLocalTime();auto item=new QListWidgetItem((entry.value("file").toString()=="recovery.json"?tr("Latest recovery — "):QString())+date.toString("yyyy-MM-dd HH:mm:ss"),list);item->setData(Qt::UserRole,entry.value("file").toString());}if(list->count())list->setCurrentRow(0);};connect(lessons,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[fill](int){fill();});auto current=UBApplication::boardController->selectedDocument();if(current){const int index=lessons->findData(current->uuid().toString(QUuid::WithoutBraces));if(index>=0)lessons->setCurrentIndex(index);}fill();
    if(!mHistory->lastError().isEmpty())layout->addWidget(new QLabel(mHistory->lastError()));auto restore=new QPushButton(tr("Restore selected version as a new lesson"));restore->setEnabled(list->count()>0);connect(list,&QListWidget::currentRowChanged,restore,[restore](int index){restore->setEnabled(index>=0);});layout->addWidget(restore);connect(restore,&QPushButton::clicked,&dialog,[=,&dialog]{if(!list->currentItem())return;QString error;auto copy=mHistory->restoreCopy(lessons->currentData().toString(),list->currentItem()->data(Qt::UserRole).toString(),&error);if(!copy){QMessageBox::warning(&dialog,tr("Cannot restore version"),error);return;}UBApplication::boardController->setActiveDocumentScene(copy,0);UBApplication::applicationController->showBoard();dialog.accept();});auto buttons=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);dialog.resize(600,420);dialog.exec();}
