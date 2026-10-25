// SPDX-License-Identifier: GPL-3.0-only
#include "UBPaperInk.h"
#include <QtWidgets>
#include <QSvgRenderer>
#include "gui/UBMainWindow.h"
#include "gui/UBStylusPalette.h"
#include "gui/UBZoomPalette.h"
#include "gui/UBDockPalette.h"
#include "core/UBApplication.h"
#include "core/UBApplicationController.h"
#include "core/UBSettings.h"
#include "board/UBBoardController.h"
#include "board/UBBoardView.h"
#include "board/UBBoardPaletteManager.h"
#include "gui/UBLeftPalette.h"
#include "gui/UBRightPalette.h"
#include "board/UBDrawingController.h"
#include "board/UBLessonTools.h"
#include "document/UBDocumentProxy.h"
#include "domain/UBGraphicsScene.h"
#include "adaptors/UBThumbnailAdaptor.h"
namespace {
QIcon inkIcon(const QString &name)
{
    static const QMap<QString,QString> paths={
        {"actionPen","M5 19l1-5L17 3l4 4L10 18z M14 6l4 4 M5 19l5-1"},
        {"actionMarker","M5 16l10-11 5 5-10 11z M3 21h8 M12 8l5 5"},
        {"actionEraser","M4 12l9-9 8 8-9 10H9z M8 17l8-8 M12 21h10"},
        {"actionSelector","M5 3l14 11-7 1-3 7z"},
        {"actionLine","M4 20L20 4"},{"actionTeachingDash","M4 20l3-3 M10 14l3-3 M16 8l4-4"},
        {"actionTeachingShapes","M3 4h12v12H3z M16 9a6 6 0 1 1-5 7"},
        {"actionLessonCover","M4 4h16v16H4z M8 8h8v8H8z"},
        {"actionText","M3 5h18 M12 5v16 M8 21h8"},
        {"actionHand","M7 12V6a1.5 1.5 0 0 1 3 0v6-8a1.5 1.5 0 0 1 3 0v8-6a1.5 1.5 0 0 1 3 0v7-5a1.5 1.5 0 0 1 3 0v8q-1 6-7 6-3 0-5-3l-4-5q-1-3 2-2l3 2"},
        {"actionUndo","M9 5L4 10l5 5 M4 10h10a5 5 0 0 1 0 10h-3"},
        {"actionRedo","M15 5l5 5-5 5 M20 10H10a5 5 0 0 0 0 10h3"},
        {"previous","M15 5l-7 7 7 7"},{"next","M9 5l7 7-7 7"},
        {"pages","M4 3h12v15H4z M8 21h12V7"},{"plus","M12 4v16 M4 12h16"},
        {"more","M5 12h.1 M12 12h.1 M19 12h.1"},{"close","M5 5l14 14 M19 5L5 19"},
        {"lesson","M3 4h18v14H3z M8 22l4-4 4 4 M7 9h10 M7 13h6"},
        {"settings","M10 2h4l.5 3 2 1 2.5-1 2 3.5-2 2v3l2 2-2 3.5-2.5-1-2 1-.5 3h-4l-.5-3-2-1-2.5 1-2-3.5 2-2v-3l-2-2 2-3.5 2.5 1 2-1z M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8"},
        {"actionTeachingGraphDesk","M4 3v17h17 M5 17l5-8 5 5 5-10"},
        {"actionTeachingFunctions","M2 9h7 M3 21q3 1 4-6L9 5q1-4 4-3 M14 6q-5 6 0 12 M21 6q3 6 0 12 M16 9l4 6 M20 9l-4 6"},
        {"actionDocument","M3 7h7l2-3h8v16H3z M8 10h8v7H8z"},
        {"actionPlay","M7 4l13 8-13 8z"},
        {"actionZoomIn","M10 3a7 7 0 1 0 0 14 7 7 0 0 0 0-14 M15 15l6 6 M7 10h6 M10 7v6"},
        {"actionZoomOut","M10 3a7 7 0 1 0 0 14 7 7 0 0 0 0-14 M15 15l6 6 M7 10h6"},
        {"actionPointer","M12 2v5 M12 17v5 M2 12h5 M17 12h5 M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8"},
        {"actionCapture","M3 8V3h5 M16 3h5v5 M21 16v5h-5 M8 21H3v-5 M7 7h10v10H7z"},
        {"actionVirtualKeyboard","M2 5h20v14H2z M6 9h1 M11 9h1 M16 9h1 M6 13h1 M11 13h1 M16 13h1 M7 16h10"},
        {"actionSnap","M5 3v10a7 7 0 0 0 14 0V3h-4v10a3 3 0 0 1-6 0V3z M5 7h4 M15 7h4"},
        {"actionNewPage","M12 4v16 M4 12h16"},
        {"leaf","M5 19Q0 6 19 3q2 15-14 16z M5 19L16 6"}
    };
    const QString path=paths.value(name,paths.value("more"));QIcon icon;
    for(auto mode:{QIcon::Normal,QIcon::Disabled}){
        const QString colour=mode==QIcon::Disabled?"#b4afa5":"#243b3b";
        const auto svg=QString("<svg xmlns='http://www.w3.org/2000/svg' width='24' height='24' viewBox='0 0 24 24'><path d='%1' fill='none' stroke='%2' stroke-width='1.65' stroke-linecap='round' stroke-linejoin='round'/></svg>").arg(path,colour).toUtf8();
        QSvgRenderer renderer(svg);QPixmap pix(64,64);pix.fill(Qt::transparent);QPainter p(&pix);renderer.render(&p);p.end();icon.addPixmap(pix,mode);
    }
    return icon;
}
QIcon pageIcon(const QPixmap &pix)
{
    QIcon icon;icon.addPixmap(pix,QIcon::Normal);icon.addPixmap(pix,QIcon::Selected);icon.addPixmap(pix,QIcon::Active);return icon;
}
// Paint the paper inside the frame rather than letting IconMode fill its edges.
class PaperPageDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p,const QStyleOptionViewItem &o,const QModelIndex &index) const override {
        p->save();p->setRenderHint(QPainter::Antialiasing);
        const bool selected=o.state&QStyle::State_Selected;
        const QRect frame=o.rect.adjusted(1,1,-1,-1);
        p->setPen(QPen(QColor(selected?"#c69c47":"#ded8cc"),selected?2:1));
        p->setBrush(QColor(selected?"#fbf5e4":"#f1ecdf"));p->drawRoundedRect(frame,7,7);
        const QRect preview=frame.adjusted(9,9,-9,-26);
        const QPixmap pix=qvariant_cast<QIcon>(index.data(Qt::DecorationRole)).pixmap(QSize(236,164));
        if(!pix.isNull()){
            const QSize size=pix.size().scaled(preview.size(),Qt::KeepAspectRatio);
            const QRect paper(preview.center()-QPoint(size.width()/2,size.height()/2),size);
            p->drawPixmap(paper,pix);p->setPen(QColor("#e5e0d6"));p->setBrush(Qt::NoBrush);p->drawRect(paper.adjusted(0,0,-1,-1));
        }
        p->setPen(QColor(selected?"#6e5429":"#53625b"));
        p->drawText(QRect(frame.left(),frame.bottom()-22,frame.width(),20),Qt::AlignCenter,index.data().toString());p->restore();
    }
};
QString panelStyle(){return "QFrame#paperDock,QFrame#paperPenPanel {background:#fffdf7;border:1px solid #ddd7cb;border-radius:16px;} QToolButton {background:transparent;border:1px solid transparent;border-radius:9px;color:#273b3b;padding:5px;} QToolButton:hover {background:#eee9dd;} QToolButton:checked {background:#dceae4;border-color:#77a99a;} QToolButton:pressed {background:#c9ded4;} QLabel {color:#56625b;background:transparent;}";}
}
QStringList UBPaperInk::defaultTools(){return {"actionSelector","actionPen","actionMarker","actionEraser","actionLine","actionTeachingShapes","actionLessonCover","actionText","actionTeachingFunctions","actionZoomIn"};}
QString UBPaperInk::toolLabel(const QString &id){
    const QMap<QString,QString> names={{"actionSelector",tr("Select")},{"actionPen",tr("Pen")},{"actionMarker",tr("Highlight")},{"actionEraser",tr("Eraser")},{"actionLine",tr("Line")},{"actionTeachingShapes",tr("Shapes")},{"actionLessonCover",tr("Cover")},{"actionText",tr("Text")},{"actionTeachingFunctions",tr("Functions")},{"actionZoomIn",tr("Zoom")}};
    return names.value(id);
}
QIcon UBPaperInk::toolIcon(const QString &id){return inkIcon(id);}
void UBPaperInk::applyPreferences(){
    mPagesOpen=UBSettings::settings()->value("Teaching/PaperInkPages",true).toBool();
    mPageToggle->setChecked(mPagesOpen);rebuild();sync();
}
QToolButton *UBPaperInk::button(QAction *source,const QString &label,QWidget *parent)
{
    auto b=new QToolButton(parent);b->setObjectName("paper-"+source->objectName());b->setAutoRaise(true);
    auto dash=source->objectName()=="actionLine"?mWindow->findChild<QAction*>("actionTeachingDash"):nullptr;
    auto zoom=source->objectName()=="actionZoomIn"?mWindow->actionZoomOut:nullptr;
    auto proxy=new QAction(b);proxy->setText(label);proxy->setToolTip(source->toolTip());
    auto update=[source,proxy,label,dash,zoom]{
        const bool dashed=dash&&UBSettings::settings()->value("Teaching/DashedLines",false).toBool();
        const bool out=zoom&&UBSettings::settings()->value("Teaching/ZoomOutTool",false).toBool();
        proxy->setText(zoom?(out?tr("Zoom −"):tr("Zoom +")):dash?(dashed?tr("Dashed"):tr("Line")):label);
        proxy->setIcon(inkIcon(out?QString("actionZoomOut"):dashed?QString("actionTeachingDash"):source->objectName()));
        proxy->setToolTip(zoom?tr("Zoom: click again to return to Select; double-click to switch in / out"):source->toolTip());
        proxy->setEnabled(source->isEnabled());proxy->setCheckable(source->isCheckable());
        proxy->setChecked(source->isChecked()||(dash&&dash->isChecked())||(zoom&&zoom->isChecked()));
    };
    connect(source,&QAction::changed,b,update);if(dash)connect(dash,&QAction::changed,b,update);if(zoom)connect(zoom,&QAction::changed,b,update);
    connect(proxy,&QAction::triggered,this,[this,source,b,dash,zoom,update]{
        const bool same=mTrayOpen&&mTrayAction==source->objectName()&&UBDrawingController::drawingController()->stylusTool()==trayTool();
        const int active=UBDrawingController::drawingController()->stylusTool();
        if(zoom&&(active==UBStylusTool::ZoomIn||active==UBStylusTool::ZoomOut))mWindow->actionSelector->trigger();
        else if(zoom&&UBSettings::settings()->value("Teaching/ZoomOutTool",false).toBool())zoom->trigger();
        else if(dash&&UBSettings::settings()->value("Teaching/DashedLines",false).toBool())dash->trigger();else source->trigger();
        // A presentation action can toggle without the exclusive native action
        // emitting changed(). Always reconcile the visible selection state.
        update();
        if(b->property("paperTray").toBool()){if(same)closeTray();else openTray(b);}
    });update();
    b->setDefaultAction(proxy);if(source->menu()){b->setMenu(source->menu());b->setPopupMode(QToolButton::InstantPopup);}b->setIconSize(QSize(26,26));return b;
}
UBPaperInk::UBPaperInk(UBMainWindow *window):QObject(window),mWindow(window)
{
    setObjectName("paperInk");mWindow->setProperty("paperInk",true);
    auto settings=UBSettings::settings();mPagesOpen=settings->value("Teaching/PaperInkPages",true).toBool();
    if(!settings->value("Teaching/CombinedLineButton",false).toBool()){
        const bool visible=settings->value("Teaching/Bottom/actionLine",true).toBool()||settings->value("Teaching/Bottom/actionTeachingDash",false).toBool();
        settings->setValue("Teaching/Bottom/actionLine",visible);settings->setValue("Teaching/Bottom/actionTeachingDash",false);
        auto order=settings->value("Teaching/BottomOrder").toStringList();
        if(!order.contains("actionLine")&&order.contains("actionTeachingDash"))order[order.indexOf("actionTeachingDash")]=QString("actionLine");
        order.removeAll("actionTeachingDash");if(!order.isEmpty())settings->setValue("Teaching/BottomOrder",order);
        settings->setValue("Teaching/CombinedLineButton",true);settings->save();
    }
    if(!settings->value("Teaching/CombinedZoomButton",false).toBool()){
        const bool shown=settings->value("Teaching/Bottom/actionZoomIn",false).toBool()||settings->value("Teaching/Bottom/actionZoomOut",false).toBool();
        settings->setValue("Teaching/Bottom/actionZoomIn",shown);settings->setValue("Teaching/Bottom/actionZoomOut",false);
        auto order=settings->value("Teaching/BottomOrder").toStringList();
        if(!order.contains("actionZoomIn")&&order.contains("actionZoomOut"))order[order.indexOf("actionZoomOut")]=QString("actionZoomIn");
        order.removeAll("actionZoomOut");if(!order.isEmpty())settings->setValue("Teaching/BottomOrder",order);
        settings->setValue("Teaching/CombinedZoomButton",true);settings->save();
    }
    auto view=UBApplication::boardController->controlView();
    // Keep legacy actions registered for shortcuts, menus and saved preferences.
    // A zero-height presentation removes the ribbon without hiding Board mode.
    view->setFrameShape(QFrame::NoFrame);view->setStyleSheet("UBBoardView{background:#f0ece2;border:0;}");
    mWindow->addToolBar(Qt::TopToolBarArea,mWindow->boardToolBar);mWindow->boardToolBar->setMaximumHeight(0);mWindow->boardToolBar->setMaximumWidth(0);
    mWindow->teachingTabs()->tabBar()->hide();
    mWindow->teachingTabs()->setStyleSheet("QTabWidget::pane{border:0;background:#f0ece2;}");
    for(const auto &id:QStringList{"actionPen","actionMarker","actionEraser","actionSelector","actionLine","actionTeachingDash","actionTeachingShapes","actionLessonCover","actionText","actionHand","actionUndo","actionRedo","actionTeachingGraphDesk"})
        if(auto a=mWindow->findChild<QAction*>(id))a->setIcon(inkIcon(id));
    mWindow->setStyleSheet(mWindow->styleSheet()+QString(
        "QMainWindow{background:#f8f5ec;} QMenu {background:#fffdf7;color:#263c3b;border:1px solid #d8d2c5;padding:6px;font-size:13px;} QMenu::item{background:transparent;padding:8px 25px;border-radius:5px;} QMenu::item:selected{background:#dceae4;} QMenu::separator{height:1px;background:#e4ded2;margin:5px;} QDialog{background:#f7f3e9;} QDialog QLabel,QDialog QCheckBox{color:#263c3b;} QDialog QListWidget{background:#fffdf7;border:1px solid #d8d2c5;border-radius:8px;selection-background-color:#dceae4;selection-color:#263c3b;} QDialog QPushButton{background:#fffdf7;border:1px solid #d4cebf;border-radius:7px;padding:7px 14px;}"));
    mHeader=new QToolBar(tr("Lesson navigation"),mWindow);mHeader->setObjectName("paperTopBar");mHeader->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);mHeader->setMovable(false);mHeader->setFloatable(false);
    mHeader->setStyleSheet("QToolBar{background:#f8f5ec;border:0;border-bottom:1px solid #ded8cc;spacing:8px;padding:6px;} QToolButton{background:transparent;color:#273b3b;border:1px solid transparent;border-radius:7px;height:30px;min-height:30px;max-height:36px;padding:4px 8px;margin:0;} QToolButton:hover{background:#ebe6da;} QToolButton:checked{background:#dceae4;border-color:#7ba99d;} QLabel{background:transparent;color:#273b3b;}");
    mHeader->setProperty("paperBaseStyle",mHeader->styleSheet());mHeader->setMaximumHeight(58);mWindow->setMenuWidget(mHeader);
    auto documents=button(mWindow->actionDocument,tr("Documents"),mHeader);documents->setObjectName("paperDocuments");documents->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);documents->setToolTip(tr("Open your lesson documents"));mHeader->addWidget(documents);
    mHeader->setIconSize(QSize(26,26));mHeader->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    auto lesson=mHeader->addAction(inkIcon("lesson"),tr("Lesson"));lesson->setObjectName("paperLesson");lesson->setCheckable(true);lesson->setChecked(true);
    connect(lesson,&QAction::triggered,this,[this,lesson]{mWindow->teachingTabs()->setCurrentIndex(0);lesson->setChecked(true);});
    auto graph=mHeader->addAction(inkIcon("actionTeachingGraphDesk"),tr("GraphDesk"));graph->setObjectName("paperGraphDesk");graph->setCheckable(true);
    connect(graph,&QAction::triggered,this,[this,graph]{mWindow->findChild<QAction*>("actionTeachingGraphDesk")->trigger();graph->setChecked(mWindow->teachingTabs()->currentIndex()!=0);});
    auto spacer=new QWidget;spacer->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);mHeader->addWidget(spacer);
    for(auto a:{mWindow->actionUndo,mWindow->actionRedo})mHeader->addWidget(button(a,QString(),mHeader));
    mPageToggle=new QToolButton;mPageToggle->setIcon(inkIcon("pages"));mPageToggle->setToolTip(tr("Show or hide pages"));mPageToggle->setCheckable(true);mPageToggle->setChecked(mPagesOpen);mHeader->addWidget(mPageToggle);
    connect(mPageToggle,&QToolButton::clicked,this,[this](bool open){mPagesOpen=open;UBSettings::settings()->setValue("Teaching/PaperInkPages",open);UBSettings::settings()->save();layoutPanels();});
    auto prev=new QToolButton;prev->setIcon(inkIcon("previous"));prev->setToolTip(tr("Previous page"));mHeader->addWidget(prev);connect(prev,&QToolButton::clicked,this,[this]{auto b=UBApplication::boardController;if(b->activeSceneIndex()>0)goToPage(b->activeSceneIndex()-1);});
    mPageCount=new QLabel;mPageCount->setMinimumWidth(50);mPageCount->setAlignment(Qt::AlignCenter);mHeader->addWidget(mPageCount);
    auto next=new QToolButton;next->setIcon(inkIcon("next"));next->setToolTip(tr("Next page"));mHeader->addWidget(next);connect(next,&QToolButton::clicked,this,[this]{auto b=UBApplication::boardController;if(b->selectedDocument()&&b->activeSceneIndex()+1<b->selectedDocument()->pageCount())goToPage(b->activeSceneIndex()+1);});
    auto menu=new QToolButton;menu->setObjectName("paperMenu");menu->setIcon(inkIcon("settings"));menu->setToolTip(tr("Settings & more"));menu->setAccessibleName(tr("Settings & more"));menu->setPopupMode(QToolButton::InstantPopup);
    buildSettingsMenu(menu);
    mHeader->addWidget(menu);
    mPages=new QFrame(view);mPages->setObjectName("paperPages");auto pl=new QVBoxLayout(mPages);pl->setContentsMargins(12,14,12,12);pl->setSpacing(10);
    auto title=new QLabel(tr("Pages"));title->setStyleSheet("font-size:14px;font-weight:600;color:#344a43;");pl->addWidget(title);
    mPageList=new QListWidget;mPageList->setObjectName("paperPageList");mPageList->setItemDelegate(new PaperPageDelegate(mPageList));mPageList->setViewMode(QListView::IconMode);mPageList->setFlow(QListView::TopToBottom);mPageList->setWrapping(false);mPageList->setMovement(QListView::Static);mPageList->setResizeMode(QListView::Adjust);mPageList->setIconSize(QSize(118,82));mPageList->setSpacing(7);mPageList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);pl->addWidget(mPageList);
    mPages->setStyleSheet("QFrame#paperPages{background:#f6f2e8;border:0;border-right:1px solid #ded8cc;} QListWidget{background:transparent;border:0;color:#53625b;outline:0;} QListWidget::item{background:#fffdf8;border:1px solid #ded8cc;border-radius:7px;padding:5px;} QListWidget::item:selected{border:2px solid #c69c47;background:#fbf5e4;color:#6e5429;} QToolButton{background:#fffdf8;border:1px solid #ded8cc;border-radius:8px;padding:8px;}");
    auto add=button(mWindow->actionNewPage,tr("Add page"),mPages);add->setIcon(inkIcon("plus"));add->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);pl->addWidget(add);
    connect(mPageList,&QListWidget::itemClicked,this,[this](QListWidgetItem *item){goToPage(item->data(Qt::UserRole).toInt());});
    mDock=new QFrame(view);mDock->setObjectName("paperDock");mDock->setStyleSheet(panelStyle());mDock->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(mDock,&QWidget::customContextMenuRequested,this,[this]{mWindow->findChild<QAction*>("actionTeachingCustomize")->trigger();});auto dl=new QHBoxLayout(mDock);dl->setContentsMargins(10,7,10,7);dl->setSpacing(3);
    mPenPanel=new QFrame(view);mPenPanel->setObjectName("paperPenPanel");mPenPanel->setStyleSheet(panelStyle());auto penLayout=new QVBoxLayout(mPenPanel);penLayout->setContentsMargins(14,12,14,12);penLayout->setSpacing(9);
    auto colours=new QGridLayout;colours->setSpacing(7);penLayout->addLayout(colours);
    for(int i=0;i<10;++i){auto b=new QToolButton;b->setObjectName(QString("paperColour%1").arg(i));b->setCheckable(true);b->setFixedSize(34,32);b->setIconSize(QSize(22,22));colours->addWidget(b,i/5,i%5);mColours<<b;connect(b,&QToolButton::clicked,this,[this,i]{selectTrayTool();UBDrawingController::drawingController()->setColorIndex(i);});}
    auto widths=new QHBoxLayout;penLayout->addLayout(widths);
    for(int i=0;i<3;++i){auto b=new QToolButton;b->setObjectName(QString("paperWidth%1").arg(i));b->setCheckable(true);b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);b->setIconSize(QSize(48,18));b->setText(QStringList{tr("Thin"),tr("Medium"),tr("Thick")}[i]);widths->addWidget(b);mWidths<<b;
        connect(b,&QToolButton::clicked,this,[this,i]{selectTrayTool();auto dc=UBDrawingController::drawingController();if(dc->stylusTool()==UBStylusTool::Eraser)dc->setEraserWidthIndex(i);else dc->setLineWidthIndex(i);refreshPen();});}
    mHoverTimer=new QTimer(this);mHoverTimer->setSingleShot(true);mHoverTimer->setInterval(280);
    connect(mHoverTimer,&QTimer::timeout,this,[this]{if(mHoverAnchor&&mHoverAnchor->underMouse())openTray(mHoverAnchor);});
    mCloseTimer=new QTimer(this);mCloseTimer->setSingleShot(true);mCloseTimer->setInterval(350);
    connect(mCloseTimer,&QTimer::timeout,this,[this]{if(!mTabletUiGesture&&!mPenPanel->underMouse()&&(!mTrayAnchor||!mTrayAnchor->underMouse()))closeTray();});
    for(auto bar:mWindow->findChildren<UBDockPalette*>())bar->setBackgroundBrush(QColor("#f1ecdf"));
    connect(mWindow->boardToolBar,&QToolBar::visibilityChanged,this,[this](bool){sync();});
    connect(mWindow->teachingTabs(),&QTabWidget::currentChanged,this,[this,lesson,graph](int index){lesson->setChecked(index==0);graph->setChecked(index!=0);sync();});
    connect(UBApplication::boardController,&UBBoardController::activeSceneChanged,this,[this]{closeTray();refreshPages();sync();});
    connect(UBApplication::boardController,&UBBoardController::zoomChanged,this,[this](qreal){scheduleLayout();});
    auto dc=UBDrawingController::drawingController();
    connect(dc,&UBDrawingController::stylusToolChanged,this,[this](int,int){closeTray();sync();});
    connect(dc,&UBDrawingController::colorIndexChanged,this,[this](int){refreshPen();});connect(dc,&UBDrawingController::lineWidthIndexChanged,this,[this](int){refreshPen();});connect(dc,&UBDrawingController::colorPaletteChanged,this,&UBPaperInk::refreshPen);
    for(const auto &id:QStringList{"actionTeachingShapes","actionLessonCover"})if(auto a=mWindow->findChild<QAction*>(id))connect(a,&QAction::changed,this,[this]{scheduleLayout();});
    auto timer=new QTimer(this);timer->setInterval(3000);connect(timer,&QTimer::timeout,this,[this]{if(mPages->isVisible())refreshPages();});timer->start();
    connect(qApp,&QCoreApplication::aboutToQuit,this,[this,timer]{mClosing=true;timer->stop();qApp->removeEventFilter(this);});
    connect(view,&QObject::destroyed,this,[this]{mClosing=true;});
    // A widget supports one effect; a later drop shadow would replace opacity.
    for(auto panel:{mDock,mPenPanel}){auto opacity=new QGraphicsOpacityEffect(panel);opacity->setOpacity(0.8);panel->setGraphicsEffect(opacity);panel->setAttribute(Qt::WA_NoMousePropagation);}
    qApp->installEventFilter(this);adoptContextBars();if(auto lesson=mWindow->findChild<UBLessonTools*>())lesson->applyTouchMode();rebuild();refreshPages();sync();
}
void UBPaperInk::adoptContextBars()
{
    auto view=UBApplication::boardController->controlView();
    for(const auto &name:QStringList{"lessonShapeToolbar","lessonCoverToolbar"})if(auto bar=mWindow->findChild<QToolBar*>(name)){
        mWindow->removeToolBarBreak(bar);mWindow->removeToolBar(bar);bar->setParent(view);bar->setWindowFlags(Qt::Widget);bar->setMovable(false);bar->setFloatable(false);bar->setOrientation(Qt::Horizontal);
        bar->setProperty("lessonOriginalStyle",QString("QToolBar{background:#fffdf7;border:1px solid #ddd7cb;border-radius:12px;spacing:3px;padding:7px;} QToolButton{color:#2b403b;background:transparent;border:1px solid transparent;border-radius:7px;height:30px;min-height:30px;padding:4px;margin:0;} QToolButton:hover{background:#eee9dd;} QToolButton:checked{background:#dceae4;border-color:#77a99a;} QLabel{color:#53625b;} QComboBox{background:#fffdf7;border:1px solid #d8d1c3;border-radius:5px;padding:4px;}"));
        bar->setProperty("lessonOriginalIconSize",QSize(22,22));bar->setIconSize(QSize(22,22));
        bar->setStyleSheet(bar->property("lessonOriginalStyle").toString());
        if(name=="lessonShapeToolbar"){
            auto opacity=new QGraphicsOpacityEffect(bar);opacity->setOpacity(.8);bar->setGraphicsEffect(opacity);
        }else{
            for(auto a:bar->actions()){
                if(a->text()==tr("Reveal selected")){a->setText(tr("Reveal"));a->setToolTip(tr("Reveal selected covers"));}
                else if(a->text()==tr("Hide selected")){a->setText(tr("Hide"));a->setToolTip(tr("Hide selected covers"));}
                else if(a->objectName()=="lessonCoverColour")a->setText(tr("Colour"));
            }
        }
        for(auto a:bar->actions())if(a->text()==tr("Close")){a->setIcon(inkIcon("close"));a->setToolTip(tr("Close tool options"));}
        bar->hide();
    }
}
void UBPaperInk::buildSettingsMenu(QToolButton *button)
{
    mSettingsMenu=new QMenu(button);mSettingsMenu->setObjectName("paperSettingsMenu");button->setMenu(mSettingsMenu);
    auto category=[this](const QString &title,const QString &name){auto menu=mSettingsMenu->addMenu(title);menu->setObjectName(name);return menu;};
    auto add=[this](QMenu *menu,const QStringList &ids){for(const auto &id:ids)if(auto action=mWindow->findChild<QAction*>(id))menu->addAction(action);};
    auto edit=category(tr("Edit"),"paperEditMenu");add(edit,{"actionCut","actionCopy","actionPaste"});
    auto page=category(tr("Page and paper"),"paperPageMenu");
    add(page,{"actionNewPage","actionDuplicatePage","actionImportPage","actionBackgrounds"});
    auto size=page->addMenu(tr("Page size"));add(size,{"actionWidePageSize","actionRegularPageSize","actionCustomPageSize"});
    page->addSeparator();add(page,{"actionClearPage"});
    auto view=category(tr("View"),"paperViewMenu");
    auto pages=view->addAction(tr("Pages sidebar"));pages->setCheckable(true);pages->setChecked(mPagesOpen);
    connect(pages,&QAction::triggered,this,[this](bool open){if(mPageToggle->isChecked()!=open)mPageToggle->click();});
    connect(mPageToggle,&QToolButton::toggled,pages,&QAction::setChecked);
    auto library=view->addAction(tr("Library"));library->setObjectName("paperLibrary");library->setCheckable(true);library->setChecked(mLibraryOpen);
    connect(library,&QAction::triggered,this,[this](bool show){mLibraryOpen=show;UBApplication::boardController->paletteManager()->rightPalette()->setVisible(show);});
    auto zoom=view->addMenu(tr("Zoom"));add(zoom,{"actionTeachingZoomIn","actionTeachingZoomOut","actionTeachingZoomReset"});
    auto documents=category(tr("Documents and versions"),"paperDocumentsMenu");add(documents,{"actionDocument"});documents->addSeparator();add(documents,{"actionLessonHistory","actionLessonSaveVersion"});
    auto workspace=category(tr("Workspace"),"paperWorkspaceMenu");add(workspace,{"actionTeachingCustomize","actionPreferences"});
    auto presentation=category(tr("Presentation"),"paperPresentationMenu");add(presentation,{"actionMultiScreen","actionHideApplication","actionSleep","actionPodcast"});
    auto help=category(tr("Help"),"paperHelpMenu");add(help,{"actionOpenTutorial","actionHintsAndTips"});
    if(!UBSettings::settings()->appHideCheckForSoftwareUpdate->get().toBool())add(help,{"actionCheckUpdate"});
    mOverflowMenu=category(tr("Toolbar overflow"),"paperToolbarOverflow");mOverflowMenu->menuAction()->setVisible(false);
    mSettingsMenu->addSeparator();add(mSettingsMenu,{"actionQuit"});
}

void UBPaperInk::rebuild()
{
    if(mClosing)return;
    closeTray();
    if(mOverflowMenu){mOverflowMenu->clear();mOverflowMenu->menuAction()->setVisible(false);}
    auto layout=qobject_cast<QHBoxLayout*>(mDock->layout());while(auto item=layout->takeAt(0)){delete item->widget();delete item;}
    const QStringList defaults=defaultTools();
    QStringList order=UBSettings::settings()->value("Teaching/BottomOrder",defaults).toStringList();order.removeDuplicates();for(const auto &id:defaults)if(!order.contains(id))order<<id;
    const QMap<QString,QString> labels={{"actionSelector",tr("Select")},{"actionPen",tr("Pen")},{"actionMarker",tr("Highlight")},{"actionEraser",tr("Eraser")},{"actionLine",tr("Line")},{"actionTeachingShapes",tr("Shapes")},{"actionLessonCover",tr("Cover")},{"actionText",tr("Text")},{"actionTeachingFunctions",tr("Functions")}};
    const bool touch=UBSettings::settings()->value("Teaching/TouchFriendly",false).toBool();
    mHeader->setMaximumHeight(touch?70:58);mHeader->setStyleSheet(mHeader->property("paperBaseStyle").toString()+(touch?QString(" QToolButton{height:40px;min-height:40px;max-height:44px;min-width:36px;}"):QString()));
    for(const auto &id:order){if(id=="actionTeachingDash"||id=="actionZoomOut"||id=="actionPointer")continue;auto source=mWindow->findChild<QAction*>(id);if(!source)continue;
        const bool show=UBSettings::settings()->value("Teaching/Bottom/"+id,defaults.contains(id)).toBool();
        if(!show||!defaults.contains(id)&&!UBSettings::settings()->value("Teaching/Bottom/"+id).isValid()){continue;}
        auto b=button(source,labels.value(id,source->text()),mDock);const bool showLabels=UBSettings::settings()->value("Teaching/ToolLabels",true).toBool();b->setToolButtonStyle(showLabels?Qt::ToolButtonTextUnderIcon:Qt::ToolButtonIconOnly);b->setFixedSize(showLabels?(id=="actionTeachingFunctions"?(touch?94:84):(touch?76:64)):(touch?64:48),showLabels?(touch?76:62):(touch?64:48));b->setIconSize(touch?QSize(34,34):QSize(26,26));layout->addWidget(b);
        const bool tray=id=="actionPen"||id=="actionMarker"||id=="actionEraser"||id=="actionLine";
        b->setProperty("paperTray",tray);b->setProperty("paperSource",id);
        if(source==mWindow->actionLine)b->setToolTip(tr("Line: double-click to switch solid / dashed"));
    }
    refreshPen();scheduleLayout();
}
void UBPaperInk::refreshPen()
{
    if(mClosing)return;
    auto dc=UBDrawingController::drawingController();auto s=UBSettings::settings();const int tool=trayTool();
    const bool touch=s->value("Teaching/TouchFriendly",false).toBool();
    const auto colours=tool==UBStylusTool::Marker?s->markerColors(s->isDarkBackground()):s->penColors(s->isDarkBackground());
    for(int i=0;i<mColours.size()&&i<colours.size();++i){mColours[i]->setFixedSize(touch?44:34,touch?42:32);mColours[i]->setIconSize(touch?QSize(28,28):QSize(22,22));QPixmap pix(44,44);pix.fill(Qt::transparent);QPainter p(&pix);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#b8b2a5"),1));auto c=colours[i];c.setAlpha(255);p.setBrush(c);p.drawEllipse(QRectF(3,3,38,38));p.end();mColours[i]->setIcon(QIcon(pix));mColours[i]->setChecked((tool==UBStylusTool::Marker?s->markerColorIndex():s->penColorIndex())==i);mColours[i]->setToolTip(colours[i].name());mColours[i]->setVisible(tool!=UBStylusTool::Eraser);}
    const QString mode=tool==UBStylusTool::Marker?"marker":tool==UBStylusTool::Line?(s->value("Teaching/DashedLines",false).toBool()?"dashed":"solid"):"pen";
    const int colourIndex=tool==UBStylusTool::Marker?s->markerColorIndex():s->penColorIndex();
    const QString colourKey=colours.value(colourIndex).name(QColor::HexRgb).mid(1);
    const int remembered=s->value("Teaching/ColourWidths/"+mode+"/"+colourKey,s->value("Teaching/ColourWidthDefaults/"+mode,0)).toInt();
    for(int i=0;i<mWidths.size();++i){mWidths[i]->setFixedSize(touch?80:66,touch?62:48);mWidths[i]->setText((tool==UBStylusTool::Eraser?QStringList{tr("Small"),tr("Medium"),tr("Large")}:QStringList{tr("Thin"),tr("Medium"),tr("Thick")})[i]);QPixmap pix(96,36);pix.fill(Qt::transparent);QPainter p(&pix);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#273b3b"),2.+i*3,Qt::SolidLine,Qt::RoundCap));QPainterPath path;path.moveTo(10,24);path.cubicTo(32,2,60,34,86,12);p.drawPath(path);p.end();mWidths[i]->setIcon(QIcon(pix));mWidths[i]->setChecked((tool==UBStylusTool::Eraser?s->eraserWidthIndex():remembered)==i);}
    mPenPanel->adjustSize();scheduleLayout();
}
void UBPaperInk::goToPage(int index)
{
    auto board=UBApplication::boardController;auto doc=board->selectedDocument();
    if(!doc||index<0||index>=doc->pageCount()||index==board->activeSceneIndex())return;
    board->persistViewPositionOnCurrentScene();board->persistCurrentScene();board->setActiveDocumentScene(index);
}
void UBPaperInk::refreshPages()
{
    if(mClosing)return;
    auto board=UBApplication::boardController;auto doc=board->selectedDocument();if(!doc)return;
    mPageCount->setText(QString("%1 / %2").arg(board->activeSceneIndex()+1).arg(doc->pageCount()));
    const QString id=doc->uuid().toString();const bool changed=mDocumentId!=id||mPageList->count()!=doc->pageCount();
    QSignalBlocker blocker(mPageList);
    if(changed){mPageList->clear();mDocumentId=id;for(int i=0;i<doc->pageCount();++i){auto item=new QListWidgetItem(QString::number(i+1),mPageList);item->setData(Qt::UserRole,i);item->setSizeHint(QSize(128,112));item->setIcon(pageIcon(UBThumbnailAdaptor::get(doc,i)));}}
    const int index=board->activeSceneIndex();if(index>=0&&index<mPageList->count()){
        if(mPageList->currentRow()!=index){mPageList->setCurrentRow(index);mPageList->scrollToItem(mPageList->currentItem());}
        if(mPages->isVisible())if(auto scene=board->activeScene()){
            QPixmap pix(236,164);pix.fill(scene->isDarkBackground()?QColor("#202020"):Qt::white);QPainter p(&pix);p.setRenderHint(QPainter::Antialiasing);const auto context=scene->renderingContext();scene->setRenderingContext(UBGraphicsScene::NonScreen);scene->render(&p,QRectF(0,0,236,164),scene->normalizedSceneRect());scene->setRenderingContext(context);p.end();mPageList->item(index)->setIcon(pageIcon(pix));
        }
    }
}
void UBPaperInk::sync()
{
    if(mClosing)return;
    const bool board=mWindow->boardToolBar->isVisible();const bool lesson=board&&mWindow->teachingTabs()->currentIndex()==0;
    mHeader->setVisible(board);mDock->setVisible(lesson);mPages->setVisible(lesson&&mPagesOpen);
    for(auto palette:mWindow->findChildren<UBStylusPalette*>())if(lesson)palette->hide();
    if(lesson){UBApplication::boardController->paletteManager()->leftPalette()->hide();UBApplication::boardController->paletteManager()->rightPalette()->setVisible(mLibraryOpen);}
    auto dc=UBDrawingController::drawingController();const int tool=trayTool();
    const bool contextual=mWindow->findChild<QAction*>("actionTeachingShapes")->isChecked()||mWindow->findChild<QAction*>("actionLessonCover")->isChecked();
    mPenPanel->setVisible(mTrayOpen&&lesson&&!contextual&&(tool==UBStylusTool::Pen||tool==UBStylusTool::Marker||tool==UBStylusTool::Line||tool==UBStylusTool::Eraser));
    refreshPen();layoutPanels();
}
void UBPaperInk::scheduleLayout()
{
    if(mClosing)return;
    if(mScheduled)return;mScheduled=true;QTimer::singleShot(0,this,[this]{mScheduled=false;layoutPanels();});
}
void UBPaperInk::layoutPanels()
{
    if(mClosing)return;
    auto view=UBApplication::boardController->controlView();if(!view)return;
    const bool touch=UBSettings::settings()->value("Teaching/TouchFriendly",false).toBool();
    const int rail=mPagesOpen&&view->width()>740?158:0;view->setPaperInkMargins(rail);
    mPages->setGeometry(0,0,rail,view->height());mPages->setVisible(mDock->isVisible()&&rail>0);mPages->raise();
    const int left=rail+12;
    int right=view->width()-12;
    if (auto zoom=UBApplication::boardController->controlContainer()->findChild<UBZoomPalette*>();
        zoom && zoom->isVisible())
        right=qMin(right,zoom->mapTo(view,QPoint(0,0)).x()-12);
    const int available=qMax(220,right-left);mDock->adjustSize();
    // Keep configured tools reachable from the top menu on narrow windows.
    if(mOverflowMenu)mOverflowMenu->clear();
    for(int i=0;i<mDock->layout()->count();++i)if(auto b=qobject_cast<QToolButton*>(mDock->layout()->itemAt(i)->widget()))b->show();
    mDock->layout()->activate();
    for(int i=mDock->layout()->count()-1;i>=0&&mDock->sizeHint().width()>available;--i){
        auto b=qobject_cast<QToolButton*>(mDock->layout()->itemAt(i)->widget());if(!b)continue;
        b->hide();if(mOverflowMenu)mOverflowMenu->insertAction(mOverflowMenu->actions().value(0),b->defaultAction());mDock->layout()->invalidate();mDock->layout()->activate();
    }
    if(mOverflowMenu)mOverflowMenu->menuAction()->setVisible(!mOverflowMenu->actions().isEmpty());
    mDock->adjustSize();mDock->move(left+(available-mDock->width())/2,view->height()-mDock->height()-14);mDock->raise();
    mPenPanel->adjustSize();const int anchor=mTrayAnchor&&!mTrayAnchor->isHidden()?mTrayAnchor->mapTo(view,QPoint(mTrayAnchor->width()/2,0)).x():mDock->x()+mDock->width()/2;mPenPanel->move(qBound(left,anchor-mPenPanel->width()/2,qMax(left,view->width()-mPenPanel->width()-12)),mDock->y()-mPenPanel->height()-9);mPenPanel->raise();
    const bool lesson=mDock->isVisible();auto dc=UBDrawingController::drawingController();const int tool=trayTool();
    bool context=false;
    for(const auto &name:QStringList{"lessonShapeToolbar","lessonCoverToolbar"})if(auto bar=mWindow->findChild<QToolBar*>(name)){
        if(!bar->isHidden()&&lesson){
            context=true;
            bar->setMaximumWidth(available);bar->adjustSize();auto anchor=mDock->findChild<QToolButton*>(name=="lessonShapeToolbar"?"paper-actionTeachingShapes":"paper-actionLessonCover");const int centre=anchor&&!anchor->isHidden()?anchor->mapTo(view,QPoint(anchor->width()/2,0)).x():mDock->x()+mDock->width()/2;bar->move(qBound(left,centre-bar->width()/2,qMax(left,view->width()-bar->width()-12)),mDock->y()-bar->height()-9);bar->raise();}
    }
    mPenPanel->setVisible(mTrayOpen&&lesson&&!context&&(tool==UBStylusTool::Pen||tool==UBStylusTool::Marker||tool==UBStylusTool::Line||tool==UBStylusTool::Eraser));
}
int UBPaperInk::trayTool() const
{
    if(mTrayAction=="actionMarker")return UBStylusTool::Marker;
    if(mTrayAction=="actionEraser")return UBStylusTool::Eraser;
    if(mTrayAction=="actionLine")return UBStylusTool::Line;
    return UBStylusTool::Pen;
}
void UBPaperInk::openTray(QToolButton *anchor)
{
    if(!anchor||mClosing)return;
    mCloseTimer->stop();mTrayAnchor=anchor;mTrayAction=anchor->property("paperSource").toString();mTrayOpen=true;
    refreshPen();layoutPanels();
}
void UBPaperInk::closeTray()
{
    mTrayOpen=false;if(mHoverTimer)mHoverTimer->stop();if(mCloseTimer)mCloseTimer->stop();
    if(mPenPanel)mPenPanel->hide();
}
void UBPaperInk::selectTrayTool()
{
    const QString id=mTrayAction;auto anchor=mTrayAnchor;
    auto dc=UBDrawingController::drawingController();
    if(dc->stylusTool()!=trayTool()){
        if(id=="actionLine")dc->setLineTool(UBSettings::settings()->value("Teaching/DashedLines",false).toBool());
        else if(auto source=mWindow->findChild<QAction*>(id))source->trigger();
        if(anchor)openTray(anchor);
    }
}
bool UBPaperInk::routeTabletEvent(QWidget *receiver,QTabletEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    const QPointF global=event->globalPosition();
#else
    const QPointF global=event->globalPosF();
#endif
    const auto type=event->type();
    // Popups are top-level widgets, outside the floating-toolbar ancestry.
    // Route every packet while a popup is open, including taps outside it.
    if(auto popup=QApplication::activePopupWidget()){
        event->accept();
        if(type==QEvent::TabletPress){mTabletUiGesture=true;mTabletTarget=nullptr;}
        QWidget *hit=nullptr;
        for(auto candidate:QApplication::topLevelWidgets())if(candidate->isVisible()&&candidate->windowType()==Qt::Popup&&candidate->rect().contains(candidate->mapFromGlobal(global.toPoint()))){
            hit=candidate->childAt(candidate->mapFromGlobal(global.toPoint()));if(!hit)hit=candidate;if(candidate==popup)break;
        }
        if(!hit&&type==QEvent::TabletPress){
            while(auto active=QApplication::activePopupWidget()){active->close();if(QApplication::activePopupWidget()==active)break;}
        }else if(auto menu=qobject_cast<QMenu*>(hit)){
            auto action=menu->actionAt(menu->mapFromGlobal(global.toPoint()));
            menu->setActiveAction(action);
            if(type==QEvent::TabletPress){mTabletMenu=menu;mTabletMenuAction=action;}
            if(action&&action->isEnabled()&&action->menu()&&(type==QEvent::TabletPress||type==QEvent::TabletMove))
                action->menu()->popup(menu->mapToGlobal(menu->actionGeometry(action).topRight()));
            if(type==QEvent::TabletRelease){
                QPointer<QAction> chosen=mTabletMenu==menu&&mTabletMenuAction==action?action:nullptr;
                mTabletMenu=nullptr;mTabletMenuAction=nullptr;
                if(chosen&&chosen->isEnabled()&&!chosen->isSeparator()&&!chosen->menu()&&event->button()==Qt::LeftButton){
                    while(auto active=QApplication::activePopupWidget()){active->close();if(QApplication::activePopupWidget()==active)break;}
                    if(chosen)chosen->trigger();
                }
            }
        }else if(hit){
            const auto mouseType=type==QEvent::TabletPress?QEvent::MouseButtonPress:type==QEvent::TabletRelease?QEvent::MouseButtonRelease:QEvent::MouseMove;
            QMouseEvent mouse(mouseType,hit->mapFromGlobal(global.toPoint()),global,event->button(),event->buttons(),event->modifiers());QApplication::sendEvent(hit,&mouse);
        }
        if(type==QEvent::TabletRelease){mTabletUiGesture=false;mTabletTarget=nullptr;}
        return true;
    }
    // Each press starts a new gesture. A popup may consume the previous release,
    // and hovering must never prevent the next tip-down from reaching a control.
    if(type==QEvent::TabletPress){
        mTabletDoubleClick=false;
        const QList<QWidget*> roots={mPenPanel,mDock,mHeader,mPages,mWindow->findChild<QToolBar*>("lessonShapeToolbar"),mWindow->findChild<QToolBar*>("lessonCoverToolbar")};
        auto overlay=[&roots](QWidget *widget)->QWidget*{
            if(!widget)return nullptr;
            for(auto root:roots)if(root&&root->isVisible()&&(widget==root||root->isAncestorOf(widget)))return root;
            return nullptr;
        };
        QWidget *target=receiver;auto root=overlay(target);
        if(!root){target=QApplication::widgetAt(global.toPoint());root=overlay(target);}
        // Tablet drivers can address the canvas even over a child overlay.
        // Hit-test those controls directly instead of relying on native widgetAt.
        if(!root)for(auto candidate:roots)if(candidate&&candidate->isVisible()&&candidate->rect().contains(candidate->mapFromGlobal(global.toPoint()))){
            root=candidate;target=candidate->childAt(candidate->mapFromGlobal(global.toPoint()));if(!target)target=candidate;break;
        }
        if(root){
            root->setAttribute(Qt::WA_NoMousePropagation);
            mTabletTarget=target;mTabletUiGesture=true;mCloseTimer->stop();mHoverTimer->stop();
        }else{mTabletUiGesture=false;mTabletTarget=nullptr;mLastTabletTap=nullptr;mTabletTapTimer.invalidate();}
    }
    // A stroke that started on the page stays a stroke when crossing controls.
    if(!mTabletUiGesture)return false;
    QPointer<QWidget> target=mTabletTarget;
    if(auto popup=QApplication::activePopupWidget();popup&&popup->rect().contains(popup->mapFromGlobal(global.toPoint())))
        target=popup->childAt(popup->mapFromGlobal(global.toPoint()))?popup->childAt(popup->mapFromGlobal(global.toPoint())):popup;
    auto mouseType=type==QEvent::TabletPress?QEvent::MouseButtonPress:type==QEvent::TabletRelease?QEvent::MouseButtonRelease:QEvent::MouseMove;
    const bool combined=target&&(target->property("paperSource").toString()=="actionLine"||target->objectName()=="paper-actionZoomIn");
    if(type==QEvent::TabletPress&&event->button()==Qt::LeftButton&&combined&&target==mLastTabletTap&&mTabletTapTimer.isValid()&&mTabletTapTimer.elapsed()<QApplication::doubleClickInterval()){
        mouseType=QEvent::MouseButtonDblClick;mTabletDoubleClick=true;mLastTabletTap=nullptr;mTabletTapTimer.invalidate();
    }
    if(type==QEvent::TabletRelease){
        if(!mTabletDoubleClick&&target&&event->button()==Qt::LeftButton&&target->rect().contains(target->mapFromGlobal(global.toPoint()))){mLastTabletTap=target;mTabletTapTimer.start();}
        else{mLastTabletTap=nullptr;mTabletTapTimer.invalidate();}
        mTabletUiGesture=false;mTabletTarget=nullptr;
    }
    // Accept the tablet packet so Qt does not also synthesize a second click.
    // The normal widget mouse handlers retain click, drag and popup semantics.
    event->accept();
    if(target){QMouseEvent mouse(mouseType,target->mapFromGlobal(global.toPoint()),global,event->button(),event->buttons(),event->modifiers());QApplication::sendEvent(target,&mouse);}
    return true;
}

bool UBPaperInk::eventFilter(QObject *object,QEvent *event)
{
    if(mClosing)return false;
    auto view=UBApplication::boardController->controlView();
    auto widget=qobject_cast<QWidget*>(object);
    if(widget&&(event->type()==QEvent::TabletPress||event->type()==QEvent::TabletMove||event->type()==QEvent::TabletRelease))
        if(routeTabletEvent(widget,static_cast<QTabletEvent*>(event)))return true;
    if(event->type()==QEvent::TabletLeaveProximity){
        if(auto button=qobject_cast<QAbstractButton*>(mTabletTarget.data()))button->setDown(false);
        mTabletUiGesture=false;mTabletTarget=nullptr;mLastTabletTap=nullptr;mTabletTapTimer.invalidate();
    }
    auto tool=qobject_cast<QToolButton*>(object);
    if(tool&&tool->property("paperTray").toBool()){
        if(event->type()==QEvent::Enter){mCloseTimer->stop();mHoverAnchor=tool;mHoverTimer->start();}
        else if(event->type()==QEvent::Leave){mHoverTimer->stop();mCloseTimer->start();}
        else if(event->type()==QEvent::MouseButtonDblClick&&tool->property("paperSource").toString()=="actionLine"){
            auto e=static_cast<QMouseEvent*>(event);if(e->button()==Qt::LeftButton){
                mWindow->findChild<QAction*>(UBSettings::settings()->value("Teaching/DashedLines",false).toBool()?"actionLine":"actionTeachingDash")->trigger();
                tool->setDown(false);tool->setProperty("paperIgnoreRelease",true);openTray(tool);return true;
            }
        }else if(event->type()==QEvent::MouseButtonRelease&&tool->property("paperIgnoreRelease").toBool()){
            tool->setProperty("paperIgnoreRelease",false);tool->setDown(false);return true;
        }
    }
    if(tool&&tool->objectName()=="paper-actionZoomIn"){
        if(event->type()==QEvent::MouseButtonDblClick&&static_cast<QMouseEvent*>(event)->button()==Qt::LeftButton){
            const bool out=!UBSettings::settings()->value("Teaching/ZoomOutTool",false).toBool();
            UBSettings::settings()->setValue("Teaching/ZoomOutTool",out);UBSettings::settings()->save();
            (out?mWindow->actionZoomOut:mWindow->actionZoomIn)->trigger();
            tool->setDown(false);tool->setProperty("paperIgnoreRelease",true);return true;
        }
        if(event->type()==QEvent::MouseButtonRelease&&tool->property("paperIgnoreRelease").toBool()){
            tool->setProperty("paperIgnoreRelease",false);tool->setDown(false);return true;
        }
    }
    if(widget&&(widget==mPenPanel||mPenPanel->isAncestorOf(widget))){
        if(event->type()==QEvent::Enter)mCloseTimer->stop();
        else if(event->type()==QEvent::Leave)mCloseTimer->start();
    }
    if(event->type()==QEvent::KeyPress&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape){
        closeTray();if(auto scene=UBApplication::boardController->activeScene())scene->cancelCircleErase();
    }
    if((event->type()==QEvent::MouseButtonPress||event->type()==QEvent::TabletPress||event->type()==QEvent::TouchBegin)&&widget){
        const bool inside=widget==mPenPanel||mPenPanel->isAncestorOf(widget)||(tool&&tool->property("paperTray").toBool());
        if(!inside){closeTray();}
        // Never consume this event: it is also the beginning of the drawing stroke.
    }
    if(event->type()==QEvent::Show&&(qobject_cast<UBStylusPalette*>(object)||object==UBApplication::boardController->paletteManager()->leftPalette()||(!mLibraryOpen&&object==UBApplication::boardController->paletteManager()->rightPalette()))&&mWindow->boardToolBar->isVisible()){
        QPointer<QWidget> palette=qobject_cast<QWidget*>(object);QTimer::singleShot(0,this,[palette]{if(palette)palette->hide();});
    }
    if(object==view&&(event->type()==QEvent::Resize||event->type()==QEvent::Show))scheduleLayout();
    if(qobject_cast<UBZoomPalette*>(object)
       &&(event->type()==QEvent::Show||event->type()==QEvent::Hide||event->type()==QEvent::Resize))scheduleLayout();
    if(auto bar=qobject_cast<QToolBar*>(object);bar&&(bar->objectName()=="lessonShapeToolbar"||bar->objectName()=="lessonCoverToolbar")&&(event->type()==QEvent::Show||event->type()==QEvent::Hide||event->type()==QEvent::Resize))scheduleLayout();
    return false;
}
