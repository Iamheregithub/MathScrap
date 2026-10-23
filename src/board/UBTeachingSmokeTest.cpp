// SPDX-License-Identifier: GPL-3.0-only
#include "UBTeachingSmokeTest.h"
#include "UBPaperInk.h"
#include <QtWidgets>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QJsonDocument>
#include <QJsonObject>
#include "core/UBApplication.h"
#include "core/UBSettings.h"
#include "gui/UBMainWindow.h"
#include "gui/UBStylusPalette.h"
#include "gui/UBToolbarButtonGroup.h"
#include "adaptors/UBSvgSubsetAdaptor.h"
#include "domain/UBGraphicsItemTransformUndoCommand.h"
#include "UBBoardController.h"
#include "UBBoardView.h"
#include <QDomDocument>
#include "UBDrawingController.h"
#include "domain/UBGraphicsScene.h"
#include "domain/UBGraphicsSvgItem.h"
#include "domain/UBQuadraticFormula.h"
#include "domain/UBGraphicsStrokesGroup.h"
#include "document/UBDocumentProxy.h"
#include "document/UBDocument.h"
#include "document/UBDocumentController.h"
#include "gui/UBDocumentThumbnailWidget.h"
#include "core/UBDocumentManager.h"
#include "core/UBPersistenceManager.h"
#include "core/UBApplicationController.h"
#include "core/UBSetting.h"
#include "adaptors/UBExportPDF.h"
#include "adaptors/UBExportFullPDF.h"
#include "adaptors/UBImportPDF.h"
#include "pdf/PDFRenderer.h"
#include <QPdfWriter>
#include <QSvgRenderer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#pragma comment(lib, "dbghelp.lib")
namespace {
LONG CALLBACK recordTeachingCrash(EXCEPTION_POINTERS *exception) {
    const DWORD code = exception->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION
        && code != EXCEPTION_STACK_OVERFLOW && code != 0xC000041D) return EXCEPTION_CONTINUE_SEARCH;
    FILE *file = nullptr;
    fopen_s(&file, "teaching-crash.log", "a");
    if (!file) return EXCEPTION_CONTINUE_SEARCH;
    fprintf(file, "Exception %08lx at %p\n", code, exception->ExceptionRecord->ExceptionAddress);
    fflush(file);
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    SymInitialize(process, nullptr, TRUE);
    CONTEXT context = *exception->ContextRecord;
    STACKFRAME64 frame = {};
    frame.AddrPC.Offset = context.Rip; frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp; frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp; frame.AddrStack.Mode = AddrModeFlat;
    for (int i = 0; i < 48; ++i) {
        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
        auto symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO); symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
            fprintf(file, "%s + %llu", symbol->Name, displacement);
        else fprintf(file, "0x%llx", frame.AddrPC.Offset);
        IMAGEHLP_LINE64 line = {}; line.SizeOfStruct = sizeof(line); DWORD offset = 0;
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &offset, &line))
            fprintf(file, " (%s:%lu)", line.FileName, line.LineNumber);
        fprintf(file, "\n");
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame,
            &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
    }
    fclose(file);
    return EXCEPTION_CONTINUE_SEARCH;
}
}
#endif
void installTeachingCrashDiagnostics() {
#ifdef Q_OS_WIN
    AddVectoredExceptionHandler(1, recordTeachingCrash);
#endif
}

namespace {
bool checkWindowAndToolbar(QString *error)
{
    auto window = UBApplication::mainWindow;
    auto toolbar = window->boardToolBar;
    auto hasLeadingDivider = [toolbar] {
        for (auto action : toolbar->actions())
            if (action->isVisible()) return action->isSeparator();
        return true;
    };
    if (hasLeadingDivider()) { *error = "Stray dividers precede Stylus"; return false; }
    // Exercise the same reorder/apply path that previously stranded separators.
    auto customize = window->findChild<QAction*>("actionTeachingCustomize");
    auto wrapper = window->findChild<QWidgetAction*>("teaching-presentation-actionMenu");
    if (!customize || toolbar->actions().contains(customize) || !wrapper || !wrapper->menu() || !wrapper->menu()->actions().contains(customize)) {
        *error = "Customization must be in the application menu, not the toolbar"; return false;
    }
    for (bool restore : {false, true}) {
        bool reordered = false;
        QTimer::singleShot(0, window, [&] {
            auto dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            auto tabs = dialog ? dialog->findChild<QTabWidget*>() : nullptr;
            auto list = tabs ? qobject_cast<QListWidget*>(tabs->widget(0)) : nullptr;
            if (list && list->count() > 1) {
                auto item = list->takeItem(restore ? list->count() - 1 : 0);
                list->insertItem(restore ? 0 : list->count(), item);
                reordered = true;
            }
            if (dialog) dialog->accept();
        });
        customize->trigger();
        if (!reordered || hasLeadingDivider()) { *error = "Toolbar reorder restored leading dividers"; return false; }
    }
    window->setProperty("toolbarLayoutChecked", true);
#ifdef Q_OS_WIN
    const HWND handle = reinterpret_cast<HWND>(window->winId());
    auto hasChrome = [window, handle] {
        const LONG_PTR expected = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        const auto style = GetWindowLongPtr(handle, GWL_STYLE);
        const HMENU menu = GetSystemMenu(handle, FALSE);
        const UINT closeState = menu ? GetMenuState(menu, SC_CLOSE, MF_BYCOMMAND) : UINT(-1);
        return !window->isFullScreen() && !(window->windowFlags() & Qt::FramelessWindowHint)
            && (style & expected) == expected && closeState != UINT(-1)
            && !(closeState & (MF_DISABLED | MF_GRAYED));
    };
    if (!hasChrome() || !window->isMaximized()) {
        window->grab().save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("paper-startup-window.png"));
        *error=QString("Windows startup state: chrome=%1 maximized=%2 fullScreen=%3 flags=%4 style=%5 size=%6x%7 min=%8x%9")
            .arg(hasChrome()).arg(window->isMaximized()).arg(window->isFullScreen()).arg(qulonglong(window->windowFlags()),0,16).arg(qulonglong(GetWindowLongPtr(handle,GWL_STYLE)),0,16)
            .arg(window->width()).arg(window->height()).arg(window->minimumWidth()).arg(window->minimumHeight());return false;
    }
    window->showMinimized(); QApplication::processEvents();
    if (!IsIconic(handle)) { *error = "Native minimize failed"; return false; }
    window->showNormal(); QApplication::processEvents();
    if (IsIconic(handle) || IsZoomed(handle) || !hasChrome()) { *error = "Native restore failed"; return false; }
    window->showMaximized(); QApplication::processEvents();
    UBApplication::applicationController->showDocument();
    UBApplication::applicationController->showBoard();
    QApplication::processEvents();
    if (!IsZoomed(handle) || !hasChrome()) { *error = "Mode switching lost the native window controls"; return false; }
    window->setProperty("windowChromeChecked", true);
    const auto frame = window->frameGeometry().intersected(window->screen()->geometry());
    window->screen()->grabWindow(0, frame.x(), frame.y(), frame.width(), frame.height())
        .save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("window-chrome-window.png"));
#endif
    return true;
}

bool checkPaperInk(QString *error)
{
    auto fail=[error](const QString &s){*error=s;return false;};
    auto window=UBApplication::mainWindow;auto board=UBApplication::boardController;
    auto paper=window->findChild<UBPaperInk*>();auto dock=window->findChild<QFrame*>("paperDock");
    auto header=window->findChild<QToolBar*>("paperTopBar");auto pages=window->findChild<QListWidget*>("paperPageList");
    if(!paper||!dock||!header||!pages||window->boardToolBar->height()>1)return fail("Paper and ink shell is missing or legacy ribbon remains");
    QSet<QString> toolNames;
    for(auto button:dock->findChildren<QToolButton*>(QString(),Qt::FindDirectChildrenOnly)){
        const QString name=button->objectName();
        if(toolNames.contains(name))return fail("Repeated paper toolbar action: "+name);
        toolNames.insert(name);
    }
    auto pen=dock->findChild<QToolButton*>("paper-actionPen");auto shape=dock->findChild<QToolButton*>("paper-actionTeachingShapes");auto cover=dock->findChild<QToolButton*>("paper-actionLessonCover");
    if(!pen||!shape||!cover||!dock->findChild<QToolButton*>("paperMore"))return fail("Paper dock tools are missing");
    window->actionSelector->trigger();pen->click();QApplication::processEvents();auto panel=window->findChild<QFrame*>("paperPenPanel");
    if(!panel||panel->isHidden()||!dock->isVisible()||header->height()>62||header->width()<window->width()-12)return fail("Paper pen controls or compact header are not visible");
    auto docs=window->findChild<QToolButton*>("paperDocuments");
    if(!docs||!docs->isVisible()||docs->text()!=QObject::tr("Documents"))return fail("Documents is not visible in the header");
    if(dock->findChild<QToolButton*>("paper-actionTeachingDash"))return fail("Dashed Line still has a separate dock button");
    const int penCentre=pen->mapTo(board->controlView(),QPoint(pen->width()/2,0)).x();
    const int rail=window->findChild<QFrame*>("paperPages")->width();
    const int expected=qBound(rail+12,penCentre-panel->width()/2,qMax(rail+12,board->controlView()->width()-panel->width()-12));
    if(qAbs(panel->x()-expected)>2)return fail("Pen tray is not anchored to Pen");
    pen->click();paper->sync();QApplication::processEvents();
    if(!panel->isHidden()||UBDrawingController::drawingController()->stylusTool()!=UBStylusTool::Pen)return fail("Closing a tray changed the pen or layout reopened it");
    pen->click();QApplication::processEvents();if(panel->isHidden())return fail("Active Pen cannot reopen its tray");
    auto lineButton=dock->findChild<QToolButton*>("paper-actionLine");
    if(!lineButton)return fail("Combined line tool unavailable");
    window->actionLine->trigger();lineButton->click();
    QMouseEvent doubleClick(QEvent::MouseButtonDblClick,QPointF(lineButton->rect().center()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(lineButton,&doubleClick);
    if(!UBSettings::settings()->value("Teaching/DashedLines").toBool()||lineButton->text()!=QObject::tr("Dashed"))return fail("Line double-click did not immediately select dashed mode");
    QMouseEvent release(QEvent::MouseButtonRelease,QPointF(lineButton->rect().center()),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(lineButton,&release);
    lineButton->click();if(!UBSettings::settings()->value("Teaching/DashedLines").toBool())return fail("Selecting combined Line forgot the dashed style");
    QApplication::sendEvent(lineButton,&doubleClick);QApplication::sendEvent(lineButton,&release);
    if(UBSettings::settings()->value("Teaching/DashedLines").toBool())return fail("Line double-click could not return to solid mode");
    window->actionSelector->trigger();pen->click();
    auto dc=UBDrawingController::drawingController();const int old=dc->currentToolColorIndex();
    auto colour=panel->findChild<QToolButton*>("paperColour8");if(!colour)return fail("Paper colour swatches unavailable");colour->click();
    if(dc->currentToolColorIndex()!=8)return fail("Paper swatch did not select the native pen colour");dc->setColorIndex(old);
    panel->findChild<QToolButton*>("paperWidth2")->click();if(dc->currentToolWidthIndex()!=2)return fail("Paper stroke width did not update");
    shape->click();QApplication::processEvents();paper->sync();QApplication::processEvents();auto shapeBar=window->findChild<QToolBar*>("lessonShapeToolbar");
    if(shapeBar->parentWidget()!=board->controlView()||!shapeBar->isVisible()||!panel->isHidden()||shapeBar->geometry().intersects(dock->geometry()))return fail("Shape controls are not positioned above the paper dock");
    window->findChild<QAction*>("actionTeachingShapes")->trigger();cover->click();QApplication::processEvents();paper->sync();QApplication::processEvents();
    auto coverBar=window->findChild<QToolBar*>("lessonCoverToolbar");
    if(coverBar->parentWidget()!=board->controlView()||!coverBar->isVisible()||coverBar->geometry().intersects(dock->geometry()))return fail("Cover controls are not positioned above the paper dock");
    cover->click();pen->click();QApplication::processEvents();
    if(!board->selectedDocument()||pages->count()!=board->selectedDocument()->pageCount())return fail("Paper page list does not match the lesson");
    const int beforePages=board->selectedDocument()->pageCount();
    auto add=window->findChild<QToolButton*>("paper-actionNewPage");if(!add)return fail("Paper add-page control unavailable");add->click();QApplication::processEvents();
    if(board->selectedDocument()->pageCount()!=beforePages+1)return fail("Paper add-page did not create a lesson page");
    const auto lastScene=board->activeScene();
    pages->itemClicked(pages->item(0));QApplication::processEvents();
    if(board->activeSceneIndex()!=0||board->activeScene()==lastScene)return fail("Paper page thumbnail did not load the selected page");
    pen->click();QApplication::processEvents();
    const auto path=QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("paper-ink-window.png");window->grab().save(path);
    window->setProperty("paperInkChecked",true);return true;
}

bool checkCircleErase(QString *error)
{
    auto fail=[error](const QString &message){*error=message;return false;};
    auto window=UBApplication::mainWindow;auto board=UBApplication::boardController;auto dc=UBDrawingController::drawingController();
    auto manager=UBPersistenceManager::persistenceManager();
    auto doc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Circle and Tap regression",true);
    board->setActiveDocumentScene(doc,0);auto scene=board->activeScene();scene->setNominalSize(800,600);scene->setBackground(false,plain);
    window->actionLine->trigger();dc->setPenColor(false,Qt::black,0);dc->setColorIndex(0);dc->setLineWidthIndex(2);
    scene->inputDevicePress(QPointF(-170,0));scene->inputDeviceMove(QPointF(170,0));scene->inputDeviceRelease();
    const QByteArray svg="<svg xmlns='http://www.w3.org/2000/svg' width='20' height='20'><rect width='20' height='20' fill='#ffdd00'/></svg>";
    auto shape=scene->addSvg(QUrl(),QPointF(0,20),svg);scene->clearSelection();
    auto render=[scene]{QImage image(800,600,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter p(&image);const auto context=scene->renderingContext();scene->setRenderingContext(UBGraphicsScene::NonScreen);scene->render(&p,QRectF(0,0,800,600),QRectF(-400,-300,800,600));scene->setRenderingContext(context);return image;};
    const QImage before=render();const auto shapeData=shape->fileData();
    window->actionEraser->trigger();
    auto loop=[&]{
        for(int i=0;i<=36;++i){const qreal angle=i*6.283185307179586/36;const QPointF pt(70*qCos(angle),70*qSin(angle));if(i==0)scene->inputDevicePress(pt);else scene->inputDeviceMove(pt);}
        scene->inputDeviceRelease();
    };
    const int undoBefore=UBApplication::undoStack->index();loop();
    if(!scene->circleEraseReady()||render()!=before||UBApplication::undoStack->index()!=undoBefore)return fail("Drawing an erase loop changed lesson ink before confirmation");
    scene->inputDevicePress(QPointF(0,0));scene->inputDeviceRelease();const auto after=render();
    if(scene->circleEraseReady()||after.pixelColor(400,300).lightness()<230||after.pixelColor(260,300).lightness()>80||after.pixelColor(540,300).lightness()>80)return fail("Circle erase did not clip only the inside of crossing ink");
    if(shape->scene()!=scene||shape->fileData()!=shapeData||UBApplication::undoStack->index()!=undoBefore+1)return fail("Circle erase changed a shape or created multiple undo steps");
    UBApplication::undoStack->undo();if(render()!=before)return fail("Circle erase Undo did not restore all ink");
    UBApplication::undoStack->redo();if(render()!=after)return fail("Circle erase Redo did not restore the clipped result");
    loop();scene->inputDevicePress(QPointF(160,160));scene->inputDeviceRelease();
    if(scene->circleEraseReady()||render()!=after)return fail("Outside tap did not cancel circle erasing");
    loop();window->actionPen->trigger();if(scene->circleEraseReady())return fail("Switching tools did not cancel circle erase");
    window->actionEraser->trigger();scene->inputDevicePress(QPointF(140,-20));scene->inputDeviceMove(QPointF(140,20));scene->inputDeviceRelease();
    if(render().pixelColor(540,300).lightness()<230)return fail("Ordinary open-stroke erasing stopped working");
    window->grab().save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("circle-erase-window.png"));
    auto next=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Native drawing checks",true);board->setActiveDocumentScene(next,0);
    window->setProperty("circleEraseChecked",true);return true;
}

bool checkShapeToolbar(QString *error)
{
    auto fail=[error](const QString &s){*error=s;return false;};
    auto window=UBApplication::mainWindow;auto board=UBApplication::boardController;
    auto manager=UBPersistenceManager::persistenceManager();
    auto palette=window->findChild<UBToolbarButtonGroup*>("lessonColourPalette");
    // Compare actual painted colour areas, including selected versus unselected.
    for(int i=0;i<10;++i) {
        auto b=palette->findChild<QToolButton*>(QString("lessonColour%1").arg(i+1));
        b->click(); b->clearFocus();
        const auto colour=b->property("swatchColour").value<QColor>();
        auto image=b->grab().toImage();int count=0;
        for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.pixelColor(x,y)==colour)++count;
        const int expected=qRound(18*image.devicePixelRatio())*qRound(18*image.devicePixelRatio());
        if(count!=expected)return fail(QString("Colour %1 painted %2 pixels, expected %3").arg(i).arg(count).arg(expected));
    }
    auto customize=window->findChild<QAction*>("actionTeachingCustomize");bool clean=true;
    QTimer::singleShot(0,window,[&]{auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!d){clean=false;return;}for(auto list:d->findChildren<QListWidget*>())for(int i=0;i<list->count();++i)if(list->item(i)->text().startsWith("teachingGroup")||list->item(i)->data(Qt::UserRole).toString()=="teachingGroup3")clean=false;d->reject();});
    customize->trigger();if(!clean)return fail("Unnamed spacer remains in toolbar customization");
    auto doc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Shape toolbar regression",true);
    board->setActiveDocumentScene(doc,0);auto scene=board->activeScene();scene->setNominalSize(800,600);scene->setBackground(false,plain);
    auto action=window->findChild<QAction*>("actionTeachingShapes");if(!action)return fail("Shapes action unavailable");action->trigger();QApplication::processEvents();
    auto bar=window->findChild<QToolBar*>("lessonShapeToolbar");if(!bar||!bar->isVisible())return fail("Shapes did not reveal its toolbar");
    auto width=bar->findChild<QComboBox*>("lessonShapeWidth");auto style=bar->findChild<QComboBox*>("lessonShapeBorderStyle");auto opacity=bar->findChild<QSlider*>("lessonShapeOpacity");
    if(!width||!style||!opacity)return fail("Shape appearance controls unavailable");
    auto view=board->controlView();auto viewport=view->viewport();
    auto draw=[&](int type,bool tablet=false) -> UBGraphicsSvgItem* {
        auto choice=bar->findChild<QAction*>(QString("lessonShape%1").arg(type));if(!choice)return nullptr;choice->trigger();
        const QPointF start=view->mapFromScene(QPointF(-150,-100)),end=view->mapFromScene(QPointF(150,60));
        auto send=[&](QEvent::Type t,QPointF p,Qt::MouseButton button,Qt::MouseButtons buttons){
            if(tablet){QTabletEvent e(t,QPointingDevice::primaryPointingDevice(),p,viewport->mapToGlobal(p.toPoint()),.8,0,0,0,0,0,Qt::NoModifier,button,buttons);QApplication::sendEvent(viewport,&e);}
            else{QMouseEvent e(t,p,viewport->mapToGlobal(p.toPoint()),button,buttons,Qt::NoModifier);QApplication::sendEvent(viewport,&e);}
        };
        send(tablet?QEvent::TabletPress:QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
        send(tablet?QEvent::TabletMove:QEvent::MouseMove,end,Qt::NoButton,Qt::LeftButton);
        send(tablet?QEvent::TabletRelease:QEvent::MouseButtonRelease,end,Qt::LeftButton,Qt::NoButton);
        for(auto item:scene->selectedItems())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item))if(svg->isTeachingShape())return svg;
        return nullptr;
    };
    for(int type=0;type<7;++type){auto shape=draw(type,type==6);if(!shape)return fail(QString("Drag drawing failed for shape %1").arg(type));QDomDocument d;d.setContent(shape->fileData());if(d.elementsByTagName("metadata").at(0).toElement().text().toInt()!=type)return fail("Wrong shape geometry inserted");if(type==3||type==5){int handles=0;for(auto h:shape->childItems())if(dynamic_cast<QGraphicsEllipseItem*>(h))++handles;if(handles!=3)return fail("Triangle must have three vertex handles");}}
    auto stylePage=manager->createDocumentSceneAt(doc,1,false);board->setActiveDocumentScene(doc,1);scene=board->activeScene();scene->setNominalSize(800,600);scene->setBackground(false,plain);
    auto target=draw(0);if(!target)return fail("Cannot draw styled rectangle");
    auto before=target->fileData();width->setCurrentIndex(width->findData(8.));style->setCurrentIndex(1);opacity->setValue(35);
    window->findChild<QAction*>("lessonShapeNoBorder")->trigger();
    QDomDocument edited;edited.setContent(target->fileData());auto g=edited.elementsByTagName("g").at(0).toElement();
    if(g.attribute("stroke")!="none"||g.attribute("stroke-width").toDouble()!=8||g.attribute("fill-opacity").toDouble()!=.35||g.attribute("stroke-dasharray")=="none")return fail("Selected shape appearance did not update");
    const auto after=target->fileData();UBApplication::undoStack->undo();UBApplication::undoStack->redo();if(target->fileData()!=after)return fail("Shape styling redo failed");
    // Restore border/fill through the same actual palette button used by teachers.
    auto border=bar->findChild<QToolButton*>("lessonShapeBorder");auto fill=bar->findChild<QToolButton*>("lessonShapeFill");
    auto borderSwatches=border->menu()->findChildren<QToolButton*>();auto fillSwatches=fill->menu()->findChildren<QToolButton*>();
    if(borderSwatches.size()!=10||fillSwatches.size()!=10)return fail("Border/fill colour palettes unavailable");
    borderSwatches[0]->click();fillSwatches[4]->click();style->setCurrentIndex(0);width->setCurrentIndex(width->findData(3.));opacity->setValue(50);
    edited.setContent(target->fileData());g=edited.elementsByTagName("g").at(0).toElement();
    if(g.attribute("stroke")=="none"||g.attribute("fill")=="none"||g.attribute("fill-opacity").toDouble()!=.5)return fail("Shape colour palettes did not edit selection");
    QImage fillImage(400,240,QImage::Format_ARGB32_Premultiplied);fillImage.fill(Qt::white);
    QPainter fillPainter(&fillImage);QSvgRenderer filledSvg(target->fileData());filledSvg.render(&fillPainter,QRectF(0,0,400,240));fillPainter.end();
    const auto fillColour=QColor(g.attribute("fill"));const auto actualFill=fillImage.pixelColor(200,120);
    const QColor expectedFill(qRound((255+fillColour.red())/2.),qRound((255+fillColour.green())/2.),qRound((255+fillColour.blue())/2.));
    if(qAbs(actualFill.red()-expectedFill.red())>2||qAbs(actualFill.green()-expectedFill.green())>2||qAbs(actualFill.blue()-expectedFill.blue())>2)return fail("Fill opacity did not render independently of the border");
    manager->persistDocumentScene(doc,scene,1,true,true);
    auto loaded=UBSvgSubsetAdaptor::loadScene(doc,1);bool found=false;
    for(auto item:loaded->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item))if(svg->isTeachingShape()&&svg->fileData()==target->fileData())found=true;
    if(!found)return fail("Styled shape did not survive fresh reload");
    QApplication::processEvents();
    if(auto paper=window->findChild<UBPaperInk*>()){paper->sync();QEventLoop settle;QTimer::singleShot(60,&settle,&QEventLoop::quit);settle.exec();}
    window->grab().save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("shape-toolbar-window.png"));
    if(auto dock=window->findChild<QFrame*>("paperDock")){
        if(bar->parentWidget()!=board->controlView()||bar->geometry().intersects(dock->geometry())||bar->height()>95)return fail(QString("Shape panel layout: parent=%1 height=%2 panel=%3,%4,%5,%6 dock=%7,%8,%9,%10")
            .arg(bar->parentWidget()==board->controlView()).arg(bar->height()).arg(bar->x()).arg(bar->y()).arg(bar->width()).arg(bar->height()).arg(dock->x()).arg(dock->y()).arg(dock->width()).arg(dock->height()));
    }else if(bar->geometry().top()<window->boardToolBar->geometry().bottom()||bar->height()>65)return fail("Shape controls must occupy a compact row below the main toolbar");
    window->grab().save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("shape-toolbar-window.png"));
    window->actionPen->trigger();if(bar->isVisible())return fail("Pen did not dismiss shape controls");
    action->trigger();if(UBDrawingController::drawingController()->stylusTool()!=UBStylusTool::Selector)return fail("Shapes did not activate selection for an existing shape");auto shapeChoice=bar->findChild<QAction*>("lessonShape0");shapeChoice->trigger();
    // Escape cancels a preview without inserting a shape or an undo command.
    const int count=scene->items().size(),undo=UBApplication::undoStack->index();
    const QPointF pos=view->mapFromScene(QPointF(20,20));QMouseEvent press(QEvent::MouseButtonPress,pos,viewport->mapToGlobal(pos.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(viewport,&press);
    QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(viewport,&escape);
    if(scene->items().size()!=count||UBApplication::undoStack->index()!=undo)return fail("Cancelled shape left content or undo state");
    window->actionPen->trigger();
    auto next=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Native drawing checks",true);board->setActiveDocumentScene(next,0);
    window->setProperty("shapeToolbarChecked",true);return true;
}

bool checkLessonControlsAndShapes(QString *error)
{
    auto window = UBApplication::mainWindow;
    auto board = UBApplication::boardController;
    auto dc = UBDrawingController::drawingController();
    auto manager = UBPersistenceManager::persistenceManager();
    auto fail = [error](const QString &message) { *error = message; return false; };
    const QDir output = QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir();
    auto palette = window->findChild<UBToolbarButtonGroup*>("lessonColourPalette");
    if (!palette || !palette->property("colourGrid").toBool()) return fail("Two-row colour palette unavailable");
    palette->displayText(true); palette->layout()->activate();
    for (int i = 0; i < 10; ++i) {
        auto button = palette->findChild<QToolButton*>(QString("lessonColour%1").arg(i+1));
        auto first = palette->findChild<QToolButton*>(i<5 ? "lessonColour1" : "lessonColour6");
        if (!button || !first || button->height()>32 || button->y()!=first->y()) return fail("Colour row has incorrect geometry");
        button->click();
        if (dc->currentToolColorIndex()!=i) return fail("Colour swatch selected the wrong palette index");
    }
    if (palette->width()>220 || palette->height()>75 || window->boardToolBar->height()>85 || palette->findChild<QToolButton*>("lessonColour6")->y()<=palette->findChild<QToolButton*>("lessonColour1")->y()) return fail("Colours must occupy two rows of five");
    auto dash = window->findChild<QAction*>("actionTeachingDash");
    auto customize = window->findChild<QAction*>("actionTeachingCustomize");
    auto penPalettes = window->findChildren<UBStylusPalette*>();
    if (!dash || !customize || penPalettes.isEmpty()) return fail("Line tool controls unavailable");
    for (auto pen : penPalettes) if (!pen->actions().contains(dash)) return fail("Dashed Line is absent from a pen palette");
    dc->setStylusTool(UBStylusTool::Pen);dash->trigger();
    if(dc->stylusTool()!=UBStylusTool::Line||!UBSettings::settings()->value("Teaching/DashedLines").toBool()||!dash->isChecked()||window->actionLine->isChecked())return fail("Dashed Line did not immediately select the dashed drawing tool");
    dash->trigger();if(!dash->isChecked())return fail("Clicking active Dashed Line switched it off");
    auto doc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Toolbar and uniform outline regression",true);
    board->setActiveDocumentScene(doc,0);auto scene=board->activeScene();scene->setNominalSize(800,600);scene->setBackground(false,plain);
    dc->setPenColor(false,Qt::black,0);dc->setColorIndex(0);dc->setLineWidthIndex(1);
    auto line=[&](double y){scene->inputDevicePress(QPointF(-150,y));scene->inputDeviceMove(QPointF(150,y));scene->inputDeviceRelease();};
    line(100);window->actionLine->trigger();
    if(UBSettings::settings()->value("Teaching/DashedLines").toBool()||dash->isChecked()||!window->actionLine->isChecked())return fail("Solid Line did not select a solid drawing tool");
    dc->setLineWidthIndex(1);line(120);dc->setStylusTool(UBStylusTool::Selector);scene->clearSelection();
    auto render=[](std::shared_ptr<UBGraphicsScene> page){QImage image(800,600,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter p(&image);page->render(&p,QRectF(0,0,800,600),QRectF(-400,-300,800,600));return image;};
    auto lines=render(scene);int ink=0,gaps=0,solid=0;
    for(int x=260;x<540;++x){if(lines.pixelColor(x,400).lightness()<80)++ink;else ++gaps;if(lines.pixelColor(x,420).lightness()<80)++solid;}
    if(ink<20||gaps<20||solid<275)return fail("Dashed/solid line rendering does not match the selected tool");
    const QByteArray data="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"120\" height=\"100\" viewBox=\"0 0 120 100\"><metadata id=\"teaching-shape\">0</metadata><g stroke=\"black\" fill=\"#ffdd00\" stroke-width=\"3\"><rect x=\"10\" y=\"10\" width=\"100\" height=\"80\"/></g></svg>";
    auto shape=scene->addSvg(QUrl(),QPointF(),data);shape->setPos(-180,-50);shape->setScale(1);shape->setTransform(QTransform());scene->clearSelection();
    auto uniform=[&](const QImage &image,UBGraphicsSvgItem *item){
        auto widthAt=[&](QPointF a,QPointF b){auto direction=b-a;QPointF n(-direction.y(),direction.x());n/=qSqrt(n.x()*n.x()+n.y()*n.y());const auto mid=(a+b)/2+QPointF(400,300);int count=0;for(int i=-8;i<=8;++i){auto p=mid+n*i;if(image.pixelColor(qRound(p.x()),qRound(p.y())).lightness()<80)++count;}return count;};
        int a=widthAt(item->mapToScene(QPointF(10,10)),item->mapToScene(QPointF(110,10)));
        int b=widthAt(item->mapToScene(QPointF(10,10)),item->mapToScene(QPointF(10,90)));
        qWarning()<<"Uniform outline measured"<<a<<b;
        return a>=2&&a<=5&&b>=2&&b<=5&&qAbs(a-b)<=1;
    };
    manager->persistDocumentScene(doc,scene,0,true,true);
    const auto sharedId=shape->uuid();const QString oldPath=doc->persistencePath()+"/images/"+sharedId.toString()+".svg";QFile old(oldPath);old.open(QIODevice::ReadOnly);const auto originalBytes=old.readAll();old.close();
    auto second=manager->createDocumentSceneAt(doc,1,false);auto twin=second->addSvg(QUrl(),QPointF(),shape->fileData());twin->setUuid(sharedId);twin->setPos(-180,-50);twin->setScale(1);twin->setTransform(QTransform());manager->persistDocumentScene(doc,second,1,true,true);
    for(const auto &matrix:{QTransform().scale(3,1),QTransform().scale(.5,2),QTransform().rotate(90).scale(2,.7)}){
        shape->setTransform(matrix);scene->clearSelection();auto image=render(scene);
        if(!uniform(image,shape))return fail("Shape outline stretched unevenly");
        QImage svgImage(800,600,QImage::Format_ARGB32_Premultiplied);svgImage.fill(Qt::white);QPainter p(&svgImage);p.translate(400,300);p.setTransform(shape->sceneTransform(),true);QSvgRenderer svg(shape->fileData());svg.render(&p,QRectF(0,0,120,100));p.end();
        if(!uniform(svgImage,shape))return fail("Persisted ordinary SVG outline is uneven");
    }
    const auto previous=shape->transform();shape->setTransform(QTransform().scale(3,1));UBApplication::undoStack->push(new UBGraphicsItemTransformUndoCommand(shape,shape->pos(),previous,shape->zValue()));UBApplication::undoStack->undo();
    if(shape->transform()!=previous||!uniform(render(scene),shape))return fail("Shape resize undo lost its outline");UBApplication::undoStack->redo();if(!uniform(render(scene),shape))return fail("Shape resize redo lost its outline");
    auto stretched=render(scene);stretched.save(output.filePath("shape-outline-window.png"));manager->persistDocumentScene(doc,scene,0,true,true);
    old.open(QIODevice::ReadOnly);if(old.readAll()!=originalBytes||shape->uuid()==sharedId)return fail("Resizing changed a shared shape asset");old.close();
    auto fresh=UBSvgSubsetAdaptor::loadScene(doc,0);UBGraphicsSvgItem *reopened=nullptr;for(auto item:fresh->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item);svg&&svg->isTeachingShape())reopened=svg;
    if(!reopened||!uniform(render(fresh),reopened))return fail("Reopening lost uniform shape outlines");
    if(qEnvironmentVariable("TEACHING_TEST_SKIP_PDF")!="1")for(bool full:{false,true}){
        const QString file=output.filePath(full?"pdf-uniform-shape-full.pdf":"pdf-uniform-shape-simple.pdf");UBExportPDF simple;UBExportFullPDF merged;UBExportAdaptor *exporter=full?static_cast<UBExportAdaptor*>(&merged):static_cast<UBExportAdaptor*>(&simple);exporter->setPageSelection({0});if(!exporter->persistsDocument(doc,file))return fail("Uniform shape PDF export failed");
        auto pdf=PDFRenderer::rendererForUuid(QUuid::createUuid(),file,true);pdf->attach();if(!pdf->isValid()){pdf->detach();return fail("Uniform shape PDF is unreadable");}pdf->setDPI(qRound(72*800/pdf->pointSizeF(1).width()));QImage image(800,600,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter p(&image);pdf->render(&p,1,false);p.end();pdf->detach();if(!uniform(image,shape))return fail("PDF export stretched the shape outline");
    }
    std::unique_ptr<UBItem> copied(shape->deepCopy());
    auto copiedShape=dynamic_cast<UBGraphicsSvgItem*>(copied.get());
    if(!copiedShape||copiedShape->sceneTransform()!=shape->sceneTransform()||copiedShape->fileData()!=shape->fileData())return fail("Copying a shape lost its transformation or outline");
    board->setActiveDocumentScene(manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Native drawing checks",true),0);
    window->setProperty("lessonControlsAndShapesChecked",true);
    return true;
}

bool checkPdfSelection(QString *error)
{
    const QDir output = QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir();
    const QString fixture = output.filePath("pdf-original.pdf");
    {
        QPdfWriter writer(fixture);
        writer.setPdfVersion(QPagedPaintDevice::PdfVersion_1_4);
        writer.setResolution(72); writer.setPageMargins(QMarginsF());
        QPainter painter;
        for (int i = 0; i < 8; ++i) {
            writer.setPageSize(QPageSize(QSizeF(400 + i * 20, 300), QPageSize::Point));
            if (i == 0) painter.begin(&writer); else writer.newPage();
            painter.fillRect(QRect(10,10,20,20), Qt::blue);
            painter.drawText(QPoint(20,70), QString("PDF ORIGINAL PAGE %1").arg(i+1));
        }
        painter.end();
    }
    auto manager = UBPersistenceManager::persistenceManager();
    auto doc = manager->createNewDocument(UBPersistenceManager::myDocumentsName, "PDF selection regression", false);
    if (!doc) { *error = "Cannot create PDF test document"; return false; }
    const QUuid uuid = QUuid::createUuid();
    QDir().mkpath(doc->persistencePath() + "/objects");
    if (!QFile::copy(fixture, doc->persistencePath() + "/objects/" + uuid.toString() + ".pdf")) {
        *error = "Cannot copy PDF fixture"; return false;
    }
    UBImportPDF importer;
    const auto imported = importer.import(uuid, fixture);
    if (imported.size() != 8) { *error = "PDF fixture import failed"; return false; }
    for (int i = 0; i < 8; ++i) {
        auto page = manager->createDocumentSceneAt(doc, i, false);
        importer.placeImportedItemToScene(page, imported[i]);
        auto mark = new UBGraphicsSvgItem(QByteArray("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"20\" height=\"20\"><rect width=\"20\" height=\"20\" fill=\"red\"/></svg>"));
        const qreal scale = page->nominalSize().width() / qreal(400 + i * 20);
        mark->setScale(scale);
        mark->setPos(page->normalizedSceneRect().topLeft() + QPointF(60*scale,10*scale));
        page->addItem(mark);
        manager->persistDocumentScene(doc, page, i, false, true);
    }
    UBApplication::applicationController->showDocument();
    UBApplication::documentController->selectDocument(doc);
    QCoreApplication::processEvents();
    auto thumbnails = UBApplication::mainWindow->findChild<UBDocumentThumbnailWidget*>("thumbnailWidget");
    UBExportFullPDF *exporter = nullptr;
    for (auto adaptor : UBDocumentManager::documentManager()->supportedExportAdaptors())
        if (auto pdf = qobject_cast<UBExportFullPDF*>(adaptor)) exporter = pdf;
    if (!thumbnails || !exporter || !exporter->associatedAction()) { *error = "Documents export UI unavailable"; return false; }
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
    UBSettings::settings()->pdfResolution->setInt(96);
    auto verify = [error](const QString &file, const QList<int> &expected) {
        auto renderer = PDFRenderer::rendererForUuid(QUuid::createUuid(), file, true);
        renderer->attach();
        bool good = renderer->isValid() && renderer->pageCount() == expected.size();
        qWarning() << "Teaching PDF verification" << file << renderer->isValid() << renderer->pageCount() << expected;
        renderer->setDPI(72);
        for (int i = 0; good && i < expected.size(); ++i) {
            good = qAbs(renderer->pointSizeF(i+1).width() - (400 + expected[i]*20)) < 3;
            QImage image(600,400,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::white);
            QPainter painter(&image); renderer->render(&painter, i+1, false); painter.end();
            const auto blue = image.pixelColor(20,20), red = image.pixelColor(70,20);
            good = good && blue.blue() > 200 && blue.red() < 60 && red.red() > 200 && red.blue() < 60;
            if (!good) qWarning() << "Teaching PDF mismatch page" << i << renderer->pointSizeF(i+1) << blue << red;
        }
        renderer->detach();
        if (!good) *error = "PDF page count/order/background/annotation mismatch: " + file;
        return good;
    };
    const QList<QList<int>> selections = {{5,2,4,3}, {}, {4}};
    const QList<QList<int>> expected = {{2,3,4,5}, {0,1,2,3,4,5,6,7}, {4}};
    for (bool merge : {true, false}) {
        UBSettings::settings()->pdfUsePDFMerger->setBool(merge);
        for (int test = 0; test < selections.size(); ++test) {
            thumbnails->clearSelection();
            for (int index : selections[test]) thumbnails->selectItemAt(index, true);
            UBApplication::documentController->pageSelectionChanged();
            if (thumbnails->document()->proxy() != doc || !exporter->associatedAction()->isEnabled()) {
                *error = QString("PDF test selection unavailable: correct document=%1, enabled=%2, pages=%3, selected=%4")
                    .arg(thumbnails->document()->proxy() == doc)
                    .arg(exporter->associatedAction()->isEnabled()).arg(doc->pageCount()).arg(thumbnails->selectedItems().size());
                return false;
            }
            qWarning() << "Teaching PDF export" << merge << test << doc->pageCount() << thumbnails->selectedItems().size();
            const QString file = output.filePath(QString("pdf-%1-%2.pdf").arg(merge ? "merge" : "render").arg(test));
            QFile::remove(file);
            QTimer dialogTimer;
            QObject::connect(&dialogTimer, &QTimer::timeout, [file] {
                for (auto widget : QApplication::topLevelWidgets())
                    if (auto dialog = qobject_cast<QFileDialog*>(widget); dialog && dialog->isVisible()) {
                        const QFileInfo target(file);
                        dialog->setDirectory(target.absolutePath());
                        dialog->selectFile(target.fileName());
                        if (auto edit = dialog->findChild<QLineEdit*>("fileNameEdit")) edit->setText(target.fileName());
                        qWarning() << "Teaching save dialog files" << dialog->selectedFiles();
                        QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                    }
            });
            dialogTimer.start(50);
            bool triggered = false;
            auto connection = QObject::connect(exporter->associatedAction(), &QAction::triggered, [&triggered] { triggered = true; });
            exporter->associatedAction()->trigger();
            QObject::disconnect(connection);
            qWarning() << "Teaching export action triggered" << triggered;
            dialogTimer.stop();
            if (!verify(file, expected[test])) return false;
        }
    }
    QString invalidFolder;
    QTemporaryDir occupied;
    QFile existing(occupied.filePath("keep.txt")); existing.open(QIODevice::WriteOnly); existing.write("keep"); existing.close();
    if (UBSettings::validateDocumentDirectory(UBSettings::userDocumentDirectory(), occupied.path(), true, &invalidFolder)
        || UBSettings::validateDocumentDirectory(UBSettings::userDocumentDirectory(), UBSettings::userDocumentDirectory(), true, &invalidFolder)
        || !QFile::exists(existing.fileName())) {
        *error = "Unsafe document folder was accepted"; return false;
    }
    bool folderUi = false;
    QTimer preferencesTimer;
    QObject::connect(&preferencesTimer, &QTimer::timeout, [&folderUi] {
        for (auto window : QApplication::topLevelWidgets()) {
            auto dialog = qobject_cast<QDialog*>(window);
            if (!dialog || !dialog->isVisible()) continue;
            auto path = dialog->findChild<QLineEdit*>("documentStoragePath");
            auto tab = dialog->findChild<QWidget*>("documentsStorageTab");
            if (!path || !tab) continue;
            folderUi = path->isReadOnly() && !path->text().isEmpty()
                && dialog->findChild<QPushButton*>("browseDocumentStorage");
            for (auto tabs : dialog->findChildren<QTabWidget*>())
                if (tabs->indexOf(tab) >= 0) tabs->setCurrentWidget(tab);
            dialog->grab().save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("preferences-window.png"));
            dialog->accept();
        }
    });
    preferencesTimer.start(50);
    UBApplication::mainWindow->actionPreferences->trigger();
    preferencesTimer.stop();
    if (!folderUi) { *error = "Documents folder preference unavailable"; return false; }
    UBExportPDF simple;
    simple.setPageSelection({6,1,6});
    const QString file = output.filePath("pdf-noncontiguous.pdf");
    if (!simple.persistsDocument(doc, file) || !verify(file, {1,6})) return false;
    return true;
}

void finish(bool passed, const QString &detail) {
    const QString path = qEnvironmentVariable("TEACHING_SMOKE_REPORT");
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::WriteOnly)) { qApp->exit(19); return; }
    QString storageError;
    if (passed && !qEnvironmentVariable("TEACHING_TEST_RELOCATE_TO").isEmpty()) {
        passed = UBSettings::settings()->scheduleDocumentDirectoryChange(qEnvironmentVariable("TEACHING_TEST_RELOCATE_TO"),
            qEnvironmentVariable("TEACHING_TEST_RELOCATE_MODE") != "use", &storageError);
    }
    QJsonObject report;
    report["documentDirectory"] = UBSettings::userDocumentDirectory();
    report["pendingDocumentDirectory"] = UBSettings::settings()->pendingDocumentDirectory();
    report["storageError"] = storageError;
    report["passed"] = passed;
    report["detail"] = storageError.isEmpty() ? detail : storageError;
    report["qt"] = qVersion();
    report["pdfSelectionChecked"] = qEnvironmentVariable("TEACHING_TEST_SKIP_PDF") != "1";
    report["shapeToolbarChecked"] = UBApplication::mainWindow->property("shapeToolbarChecked").toBool();
    report["lessonControlsAndShapesChecked"] = UBApplication::mainWindow->property("lessonControlsAndShapesChecked").toBool();
    report["windowChromeChecked"] = UBApplication::mainWindow->property("windowChromeChecked").toBool();
    report["toolbarLayoutChecked"] = UBApplication::mainWindow->property("toolbarLayoutChecked").toBool();
    file.write(QJsonDocument(report).toJson());
    file.close();
    qApp->exit(passed ? 0 : 18);
}
}
void scheduleTeachingSmokeTest(QObject *context)
{
    QTimer::singleShot(120000, context, [] { finish(false, "Native test timed out"); });
    QTimer::singleShot(7000, context, [] {
        qWarning("Teaching check: board initialized");
        auto board = UBApplication::boardController;
        if (!board || !board->activeScene() || !UBApplication::mainWindow) {
            finish(false, "Board failed to initialize"); return;
        }
        QString chromeError;
        if (!checkWindowAndToolbar(&chromeError) || !checkLessonControlsAndShapes(&chromeError) || !checkShapeToolbar(&chromeError) || !checkPaperInk(&chromeError) || !checkCircleErase(&chromeError)) { finish(false, chromeError); return; }
        if (!UBApplication::mainWindow->findChild<QAction*>("actionTeachingQuadratic")) { finish(false,"Quadratic formula action missing"); return; }
        UBQuadraticFormula::State example{1,6,7,0}, restored;
        auto first=UBQuadraticFormula::render(example);
        QDomDocument formulaDoc;formulaDoc.setContent(first);
        const auto formulaBox=formulaDoc.documentElement().attribute("viewBox").split(' ');
        const double formulaWidth=formulaBox.value(2).toDouble(),formulaHeight=formulaBox.value(3).toDouble();
        if (!UBQuadraticFormula::read(first,&restored) || restored.a!=1 || restored.b!=6 || restored.c!=7 || restored.step!=0
            || UBQuadraticFormula::stepAt(first,formulaWidth-80,formulaHeight-36)!=1) { finish(false,"Quadratic initial state or Next button invalid"); return; }
        const auto formulaOutput=QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir();
        for(int stage:{1,5}){
            example.step=stage;const auto data=UBQuadraticFormula::render(example);QSvgRenderer renderer(data);
            if(!renderer.isValid()||!data.contains("4(1)(7)")||!data.contains("denominator 2(1)")){finish(false,"Quadratic substitution notation or SVG invalid");return;}
            QFile svgFile(formulaOutput.filePath(QString("quadratic-stage-%1.svg").arg(stage)));svgFile.open(QIODevice::WriteOnly);svgFile.write(data);svgFile.close();
            QImage image(renderer.defaultSize()*2,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter painter(&image);renderer.render(&painter);painter.end();image.save(formulaOutput.filePath(QString("quadratic-stage-%1.png").arg(stage)));
        }
        example.step=5;
        auto finalFormula=UBQuadraticFormula::render(example);
        if (!finalFormula.contains("-3 ± √2") || !finalFormula.contains("-1.586") || !finalFormula.contains("-4.414")
            || !UBQuadraticFormula::read(finalFormula,&restored) || restored.step!=5) { finish(false,"Quadratic exact or approximate roots invalid"); return; }
        if (!UBQuadraticFormula::render({1,0,1,5}).contains("No real roots") ||
            !UBQuadraticFormula::render({1,-2,1,5}).contains("One repeated root")) { finish(false,"Quadratic discriminant cases invalid"); return; }
        auto menuWrapper = UBApplication::mainWindow->findChild<QWidgetAction*>("teaching-presentation-actionMenu");
        auto menuButton = menuWrapper ? qobject_cast<QToolButton*>(menuWrapper->defaultWidget()) : nullptr;
        auto pageButton = UBApplication::mainWindow->findChild<QToolButton*>("teaching-actionPages");
        if (!menuButton || !menuButton->menu() || !menuButton->menu()->actions().contains(UBApplication::mainWindow->actionPreferences) || !pageButton) {
            finish(false, "Native toolbar menu/page controls were lost"); return;
        }
        const int oldPageCount = board->selectedDocument()->pageCount();
        auto pageWrapper = UBApplication::mainWindow->findChild<QWidgetAction*>("teaching-presentation-actionPages");
        if (!pageWrapper || pageWrapper->text().isEmpty() || menuWrapper->menu() != menuButton->menu()) {
            finish(false, "Toolbar overflow controls are unavailable"); return;
        }
        pageWrapper->trigger();
        if (board->selectedDocument()->pageCount() != oldPageCount + 1) {
            finish(false, "New Page toolbar behaviour was lost"); return;
        }
        const QString reportPath = qEnvironmentVariable("TEACHING_SMOKE_REPORT");
        UBApplication::mainWindow->grab().save(QFileInfo(reportPath).dir().filePath("lesson-window.png"));
        auto scene = board->activeScene();
        scene->setBackground(false, plain);
        auto dc = UBDrawingController::drawingController();
        dc->setPenColor(false, QColor(Qt::black), 0);
        dc->setMarkerColor(false, QColor(Qt::yellow), 0);
        auto stroke = [dc, scene](int tool, qreal y) {
            dc->setStylusTool(tool); dc->setColorIndex(0);
            scene->inputDevicePress(QPointF(-40,y));
            scene->inputDeviceMove(QPointF(0,y));
            scene->inputDeviceMove(QPointF(40,y));
            scene->inputDeviceRelease();
        };
        qWarning("Teaching check: draw strokes");
        stroke(UBStylusTool::Pen,0); stroke(UBStylusTool::Marker,0);
        stroke(UBStylusTool::Marker,40); stroke(UBStylusTool::Pen,40);
        qreal inkBottom = 1000000, markerTop = -1000000;
        int count = 0;
        for (auto item : scene->items()) if (auto group = dynamic_cast<UBGraphicsStrokesGroup*>(item)) {
            ++count;
            if (group->color().alpha() < 255) markerTop = qMax(markerTop,group->zValue());
            else inkBottom = qMin(inkBottom,group->zValue());
        }
        if (count < 4 || markerTop >= inkBottom) { finish(false,"Highlights must remain below ink"); return; }
        dc->setStylusTool(UBStylusTool::Selector);
        scene->clearSelection();
        QImage image(200,160,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::white);
        QPainter painter(&image);
        scene->render(&painter,QRectF(0,0,200,160),QRectF(-100,-60,200,160));
        painter.end();
        if (image.pixelColor(100,60).lightness() > 50 || image.pixelColor(100,100).lightness() > 50) {
            finish(false,"Highlighting obscured black ink"); return;
        }
        qWarning("Teaching check: shapes");
        const QByteArray shapeData = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"300\" height=\"300\" viewBox=\"0 0 300 300\"><metadata id=\"teaching-shape\">3</metadata><g stroke=\"black\" fill=\"none\" stroke-width=\"3\"><polygon points=\"150,12 288,288 12,288\"/></g></svg>";
        auto shape = scene->addSvg(QUrl(),QPointF(400,0),shapeData);
        if (!shape || !shape->isTeachingShape() || shape->childItems().size() < 3) {
            finish(false,"Triangle editing handles were not created"); return;
        }
        const auto before = shape->fileData();
        auto edited = before; edited.replace("150,12","180,22");
        shape->setTeachingSvg(edited); const auto after = shape->fileData(); shape->commitTeachingSvg(before);
        UBApplication::undoStack->undo();
        if (shape->fileData() != before) { finish(false,"Shape undo failed"); return; }
        UBApplication::undoStack->redo();
        if (shape->fileData() != after) { finish(false,"Shape redo failed"); return; }
        board->persistCurrentScene(false, true);
        board->setActiveDocumentScene(board->activeSceneIndex());
        if (UBSettings::settings()->penColors(false).size() < 10) { finish(false,"Ten colour palette unavailable"); return; }
        auto window = UBApplication::mainWindow;
        auto graphAction = window->findChild<QAction*>("actionTeachingGraphDesk");
        if (!graphAction) { finish(false,"GraphDesk toolbar action unavailable"); return; }
        qWarning("Teaching check: open GraphDesk");
        graphAction->trigger();
        auto tabs = window->teachingTabs();
        auto view = tabs->currentWidget()->findChild<QWebEngineView*>();
        if (!view) { finish(false,"GraphDesk tab was not created"); return; }
        QObject::connect(view,&QWebEngineView::loadFinished,view,[view](bool ok) {
            if (!ok) { finish(false,"GraphDesk resource failed to load"); return; }
            const QString script = R"JS((()=>{try{const p=project();p.plots=[
                {expr:'x^2',color:'#ff0000',show:true,dash:'solid',width:'2.5'},
                {expr:'2x+1',color:'#0000ff',show:true,dash:'dashed',width:'2.5'},
                {expr:'sin(x)',color:'#008000',show:true,dash:'solid',width:'2.5'},
                {expr:'-x+3',color:'#800080',show:true,dash:'solid',width:'2.5'}];
                importProject(p);validForExport();const svg=exportScene(scene(),'key').svg;
                importProject(JSON.parse(JSON.stringify(project())));
                return JSON.stringify({ok:project().plots.length===4&&svg===exportScene(scene(),'key').svg&&svg.includes('stroke-dasharray')});
                }catch(e){return JSON.stringify({ok:false,error:String(e)})}})())JS";
            view->page()->runJavaScript(script,[](const QVariant &result) {
                UBApplication::mainWindow->grab().save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("graphdesk-window.png"));
                const auto report = QJsonDocument::fromJson(result.toString().toUtf8()).object();
                QTimer::singleShot(0, qApp, [report, result] {
                QString exportError;
                if (report.value("ok").toBool() && qEnvironmentVariable("TEACHING_TEST_SKIP_PDF") != "1" && !checkPdfSelection(&exportError)) {
                    finish(false, exportError); return;
                }
                finish(report.value("ok").toBool(), report.value("ok").toBool()
                    ? "Startup, ink/highlight layering and pixels, triangle undo/redo, ten swatches, GraphDesk four-function export/restore, Documents PDF selection/all/single pages with both exporters, page order and original PDF/annotation pixels passed"
                    : "GraphDesk check failed: " + result.toString());
                });
            });
        });
    });
}
