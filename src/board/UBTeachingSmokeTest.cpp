// SPDX-License-Identifier: GPL-3.0-only
#include "UBTeachingSmokeTest.h"
#include "UBPaperInk.h"
#include "UBMathFormulaTest.h"
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
#include "domain/UBGraphicsTextItem.h"
#include "gui/UBZoomPalette.h"
#include "domain/UBMathFormula.h"
#include "domain/UBQuadraticFormula.h"
#include "domain/UBGraphicsStrokesGroup.h"
#include "document/UBDocumentProxy.h"
#include "document/UBDocument.h"
#include "document/UBDocumentController.h"
#include "gui/UBDocumentThumbnailWidget.h"
#include "gui/UBThumbnailScene.h"
#include "gui/UBThumbnail.h"
#include "core/UBDocumentManager.h"
#include "core/UBPersistenceManager.h"
#include "core/UBApplicationController.h"
#include "core/UBSetting.h"
#include "core/UBShortcutManager.h"
#include "adaptors/UBExportPDF.h"
#include "adaptors/UBExportFullPDF.h"
#include "adaptors/UBImportPDF.h"
#include "adaptors/UBThumbnailAdaptor.h"
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
bool checkPreferencesRedesign(QString *error)
{
    auto settings=UBSettings::settings();auto window=UBApplication::mainWindow;
    const auto touch=settings->value("Teaching/TouchFriendly",false),labels=settings->value("Teaching/ToolLabels",true),pages=settings->value("Teaching/PaperInkPages",true);
    const auto palette=settings->boardPenLightBackgroundSelectedColors->get(),order=settings->value("Teaching/BottomOrder"),home=settings->webHomePage->get();
    const auto penWidth=settings->boardPenFineWidth->get();const auto folder=UBSettings::userDocumentDirectory(),pending=settings->pendingDocumentDirectory();
    auto cleared=new QAction("Preferences clear-binding probe",window);cleared->setObjectName("actionPreferencesClearProbe");cleared->setShortcut(QKeySequence("Ctrl+Shift+F11"));cleared->setProperty("builtIn",true);
    settings->setValue("Shortcut/actionPreferencesClearProbe",QStringList{QString(),QString(),QString()});
    UBShortcutManager::shortcutManager()->addActions("Preferences regression",{cleared});
    if(!cleared->shortcut().isEmpty()){*error="An intentionally empty shortcut was replaced by its default";return false;}
    bool checked=false,good=true,shortcutChanged=false;QTimer timer;
    QObject::connect(&timer,&QTimer::timeout,[&]{
        auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
        auto nav=dialog?dialog->findChild<QListWidget*>("preferencesNavigation"):nullptr;
        if(!nav||checked)return;checked=true;timer.stop();
        auto search=dialog->findChild<QLineEdit*>("preferencesSearch");auto tabs=dialog->findChild<QTabWidget*>("mainTabWidget");
        good=search&&tabs&&nav->count()==8&&tabs->count()==8&&tabs->tabBar()->isHidden();
        auto touchBox=dialog->findChild<QCheckBox*>("preferencesTouchControls");auto labelsBox=dialog->findChild<QCheckBox*>("preferencesToolLabels");auto pagesBox=dialog->findChild<QCheckBox*>("preferencesShowPages");
        good=good&&touchBox&&labelsBox&&pagesBox&&dialog->findChild<QPushButton*>("preferencesCustomizeWorkspace");
        if(good){
            const bool previous=touchBox->isChecked();touchBox->click();good=good&&settings->value("Teaching/TouchFriendly").toBool()!=previous;touchBox->click();
            labelsBox->click();auto pen=window->findChild<QToolButton*>("paper-actionPen");good=good&&pen&&pen->toolButtonStyle()==(labelsBox->isChecked()?Qt::ToolButtonTextUnderIcon:Qt::ToolButtonIconOnly);labelsBox->click();
            pagesBox->click();good=good&&settings->value("Teaching/PaperInkPages").toBool()==pagesBox->isChecked();pagesBox->click();
            search->setText("projector");good=good&&nav->currentItem()->data(Qt::UserRole).toString()=="projector";search->clear();
            search->setText("no-such-setting-987");good=good&&!tabs->isVisible();search->clear();
            // Exercise the real shortcut editor, then restore the saved binding.
            nav->setCurrentRow(4);auto table=dialog->findChild<QTableView*>("preferencesShortcuts");auto edit=dialog->findChild<QPushButton*>("preferencesEditShortcut");
            auto model=UBShortcutManager::shortcutManager();QModelIndex target;
            for(int row=0;row<model->rowCount();++row)if(model->index(row,0).data().toString().remove('&')==window->actionPen->text().remove('&')){target=model->index(row,2);break;}
            const auto oldKey=target.data(UBShortcutManager::PrimaryShortcutRole);auto proxy=table?qobject_cast<QSortFilterProxyModel*>(table->model()):nullptr;
            if(target.isValid()&&proxy&&edit){
                table->selectRow(proxy->mapFromSource(target).row());
                QTimer::singleShot(0,dialog,[&]{auto editor=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!editor||editor->objectName()!="preferencesShortcutEditor")return;
                    auto key=editor->findChild<QKeySequenceEdit*>("preferencesShortcutKey");auto buttons=editor->findChild<QDialogButtonBox*>();if(key&&buttons){key->setKeySequence(QKeySequence("Ctrl+Alt+Shift+F12, Ctrl+Alt+Shift+F11"));buttons->button(QDialogButtonBox::Save)->click();shortcutChanged=target.data(UBShortcutManager::PrimaryShortcutRole).toString()==QKeySequence("Ctrl+Alt+Shift+F12, Ctrl+Alt+Shift+F11").toString();}if(editor->isVisible())editor->reject();});
                edit->click();model->setData(target,oldKey);
            }
            good=good&&shortcutChanged&&target.data(UBShortcutManager::ShortcutsRole).toStringList().contains(oldKey.toString());
            const QDir output(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir());
            for(int row=0;row<nav->count();++row){nav->setCurrentRow(row);QApplication::processEvents();dialog->grab().save(output.filePath("preferences-"+nav->item(row)->data(Qt::UserRole).toString()+"-window.png"));}
            for(const auto &name : {"preferencesWritingTabs","preferencesAboutTabs"}) {
                auto inner=dialog->findChild<QTabWidget*>(name);good=good&&inner;
                if(!inner)continue;
                nav->setCurrentRow(QString(name)=="preferencesWritingTabs"?1:7);
                for(int selected=0;selected<inner->count();++selected) {
                    inner->setCurrentIndex(selected);QApplication::processEvents();
                    for(int page=0;page<inner->count();++page)good=good&&(inner->widget(page)->isVisible()==(page==selected));
                    if(QString(name)=="preferencesWritingTabs"&&selected==1)dialog->grab().save(output.filePath("preferences-highlighter-window.png"));
                }
                inner->setCurrentIndex(0);
            }
            nav->setCurrentRow(0);
            auto reset=dialog->findChild<QPushButton*>("defaultSettingsButton");
            QTimer::singleShot(0,dialog,[]{if(auto question=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))question->button(QMessageBox::Yes)->click();});
            reset->click();good=good&&!touchBox->isChecked()&&labelsBox->isChecked()&&pagesBox->isChecked();
            good=good&&settings->value("Teaching/BottomOrder")==order&&settings->boardPenLightBackgroundSelectedColors->get()==palette&&settings->boardPenFineWidth->get()==penWidth;
            good=good&&UBSettings::userDocumentDirectory()==folder&&settings->pendingDocumentDirectory()==pending;
            nav->setCurrentRow(6);dialog->findChild<QLineEdit*>("webHomePage")->setText("https://example.org/mathscrap-preferences-test");
        }
        dialog->close(); // Window-close must save the same fields as Done.
    });
    timer.start(50);window->actionPreferences->trigger();timer.stop();
    // Let native window activation return from the modal dialog before the next
    // test sends pointer events to the board. A real user cannot click this early.
    QEventLoop activation;QTimer::singleShot(100,&activation,&QEventLoop::quit);activation.exec();
    good=good&&settings->webHomePage->get().toString()=="https://example.org/mathscrap-preferences-test";
    settings->webHomePage->set(home);settings->setValue("Teaching/TouchFriendly",touch);settings->setValue("Teaching/ToolLabels",labels);settings->setValue("Teaching/PaperInkPages",pages);settings->save();
    if(auto paper=window->findChild<UBPaperInk*>())paper->applyPreferences();
    if(!checked||!good){*error="Preferences navigation, live workspace controls, shortcut editing, scoped reset or window-close persistence failed";return false;}
    window->setProperty("preferencesRedesignChecked",true);return true;
}

bool checkSelectionOrigin(QString *error)
{
    auto window=UBApplication::mainWindow;auto board=UBApplication::boardController;
    auto view=board->controlView();auto settings=UBSettings::settings();
    const auto savedPages=settings->value("Teaching/PaperInkPages",true);
    const int savedTool=UBDrawingController::drawingController()->stylusTool();
    const qreal savedZoom=board->currentZoom();
    auto manager=UBPersistenceManager::persistenceManager();
    auto doc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Selection origin regression",true);
    board->setActiveDocumentScene(doc,0);auto scene=board->activeScene();
    const bool undoEnabled=scene->isURStackIsEnabled();scene->setURStackEnable(false);
    bool good=true;QString failure;
    for(bool pages : {false,true}) {
        settings->setValue("Teaching/PaperInkPages",pages);
        if(auto paper=window->findChild<UBPaperInk*>())paper->applyPreferences();
        QApplication::processEvents();
        for(qreal zoom : {0.75,1.0,1.6}) {
            board->zoom(zoom/board->currentZoom(),view->mapToScene(view->viewport()->rect().center()));
            QApplication::processEvents();
            const QPoint first(200,140),last(360,260);
            const QByteArray svg="<svg xmlns='http://www.w3.org/2000/svg' width='20' height='20'><rect width='20' height='20' fill='#147d92'/></svg>";
            auto inside=scene->addSvg(QUrl(),view->mapToScene(QPoint(280,200)),svg);
            auto outside=scene->addSvg(QUrl(),view->mapToScene(QPoint(80,200)),svg);
            for(bool reverse : {false,true}) {
                window->actionSelector->trigger();scene->deselectAllItems();
                const QPoint start=reverse?last:first,end=reverse?first:last;
                QMouseEvent press(QEvent::MouseButtonPress,QPointF(start),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                QApplication::sendEvent(view->viewport(),&press);
                QMouseEvent move(QEvent::MouseMove,QPointF(end),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
                QApplication::sendEvent(view->viewport(),&move);
                QRubberBand *band=nullptr;for(auto candidate:view->findChildren<QRubberBand*>())if(candidate->isVisible())band=candidate;
                const QRect expected=QRect(start,end).normalized();
                if(!band||band->mapToGlobal(QPoint())!=view->viewport()->mapToGlobal(expected.topLeft())||band->size()!=expected.size()||!inside->isSelected()||outside->isSelected()) {
                    good=false;failure=QString("Selection rectangle origin or hit test mismatch (Pages=%1, zoom=%2, reverse=%3)").arg(pages).arg(zoom).arg(reverse);
                }
                if(pages&&zoom==1.0&&!reverse)window->grab().save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("selection-origin-window.png"));
                QMouseEvent release(QEvent::MouseButtonRelease,QPointF(end),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QApplication::sendEvent(view->viewport(),&release);
                if(view->findChild<QRubberBand*>()) {good=false;failure="Selection rectangle remained after pointer release";}
            }
            // UBCoreGraphicsScene owns detached items until scene teardown.
            scene->removeItem(inside);scene->removeItem(outside);
        }
    }
    scene->setURStackEnable(undoEnabled);
    settings->setValue("Teaching/PaperInkPages",savedPages);settings->save();
    if(auto paper=window->findChild<UBPaperInk*>())paper->applyPreferences();
    board->zoom(savedZoom/board->currentZoom(),view->mapToScene(view->viewport()->rect().center()));
    UBDrawingController::drawingController()->setStylusTool(static_cast<UBStylusTool::Enum>(savedTool));
    QApplication::processEvents();
    if(!good){*error=failure;return false;}
    window->setProperty("selectionOriginChecked",true);return true;
}

bool checkExploratoryInteractions(QString *error)
{
    auto fail = [error](const QString &message) { *error = message; return false; };
    auto window = UBApplication::mainWindow;
    auto board = UBApplication::boardController;
    auto view = board->controlView();
    auto settings = UBSettings::settings();
    auto manager = UBPersistenceManager::persistenceManager();
    auto doc = manager->createNewDocument(UBPersistenceManager::myDocumentsName,
                                           "Exploratory pointer and tool checks", true);
    qWarning("Exploration: document created");
    const auto previousPages = settings->value("Teaching/PaperInkPages", true);
    const QDir output(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir());
    auto send = [view](QEvent::Type type, QPointF p, Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent event(type, p, view->viewport()->mapToGlobal(p.toPoint()), button, buttons, Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &event);
    };
    auto pointsClose = [](QPointF actual, QPointF expected, qreal tolerance = 2.5) {
        return QLineF(actual, expected).length() <= tolerance;
    };
    auto shapeAt = [view](UBGraphicsSvgItem *item, QPointF local) {
        return view->mapFromScene(item->mapToScene(local));
    };
    const QByteArray square = "<svg xmlns='http://www.w3.org/2000/svg' width='36' height='36'><rect width='36' height='36' fill='#147d92'/></svg>";
    int page = 0;
    for (bool pages : {false, true}) {
        settings->setValue("Teaching/PaperInkPages", pages);
        if (auto paper = window->findChild<UBPaperInk*>()) paper->applyPreferences();
        QApplication::processEvents();
        for (qreal zoom : {0.7, 1.0, 1.7}) {
            qWarning().noquote() << "Exploration: page" << page << "Pages" << pages << "zoom" << zoom;
            if (page > 0) manager->createDocumentSceneAt(doc, page, true);
            board->setActiveDocumentScene(doc, page);
            qWarning("Exploration: active scene set");
            auto scene = board->activeScene();
            board->zoom(zoom / board->currentZoom(), view->mapToScene(view->viewport()->rect().center()));
            QApplication::processEvents();
            const QString context = QString("Pages=%1, zoom=%2, page=%3").arg(pages).arg(zoom).arg(page);
            if (auto palette=board->controlContainer()->findChild<UBZoomPalette*>();
                palette && palette->isVisible()) {
                auto dock=view->findChild<QWidget*>("paperDock");
                if (!dock || dock->geometry().intersects(QRect(palette->mapTo(view,QPoint()),palette->size())))
                    return fail("Zoom control covers the lesson tool tray: " + context);
            }
            window->actionSelector->trigger();
            auto item = scene->addSvg(QUrl(), view->mapToScene(QPoint(230, 170)), square);
            if (!item) return fail("Could not add draggable object: " + context);
            scene->clearSelection();
            const QPointF start = shapeAt(item, item->boundingRect().center());
            const QPointF end = start + QPointF(60, 35);
            const QPointF initial = item->pos();
            const int undoBeforeMove = UBApplication::undoStack->index();
            qWarning("Exploration: dragging object");
            send(QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
            qWarning("Exploration: object pressed");
            send(QEvent::MouseMove, start + QPointF(30, 17), Qt::NoButton, Qt::LeftButton);
            qWarning("Exploration: object moved halfway");
            send(QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
            qWarning("Exploration: object moved to end");
            send(QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
            qWarning("Exploration: object released");
            const QPointF moved = item->pos();
            const QPointF expected = initial + view->mapToScene(end.toPoint()) - view->mapToScene(start.toPoint());
            if (!pointsClose(moved, expected) || !pointsClose(shapeAt(item, item->boundingRect().center()), end)
                || UBApplication::undoStack->index() <= undoBeforeMove)
                return fail("Selected object did not follow the drag or record Undo: " + context);
            UBApplication::undoStack->undo();
            if (!pointsClose(item->pos(), initial)) return fail("Object move Undo missed its origin: " + context);
            UBApplication::undoStack->redo();
            qWarning("Exploration: object move and history checked");
            if (!pointsClose(item->pos(), moved)) return fail("Object move Redo missed its destination: " + context);

            auto shapes = window->findChild<QAction*>("actionTeachingShapes");
            if (!shapes) return fail("Shape tool missing");
            shapes->trigger();
            auto bar = window->findChild<QToolBar*>("lessonShapeToolbar");
            auto choice = bar ? bar->findChild<QAction*>("lessonShape0") : nullptr;
            if (!choice) return fail("Rectangle choice missing");
            choice->trigger();
            qWarning("Exploration: drawing rectangle");
            const QPointF topLeft(375, 120), bottomRight(475, 205);
            const QPointF sceneSize = view->mapToScene(bottomRight.toPoint())
                                      - view->mapToScene(topLeft.toPoint());
            send(QEvent::MouseButtonPress, topLeft, Qt::LeftButton, Qt::LeftButton);
            send(QEvent::MouseMove, bottomRight, Qt::NoButton, Qt::LeftButton);
            QGraphicsPathItem *preview = nullptr;
            for (auto candidate : scene->items())
                if (auto path = dynamic_cast<QGraphicsPathItem*>(candidate); path && path->zValue() == 1900000)
                    preview = path;
            if (!preview || !pointsClose(view->mapFromScene(preview->path().boundingRect().topLeft()), topLeft)
                         || !pointsClose(view->mapFromScene(preview->path().boundingRect().bottomRight()), bottomRight))
                return fail("Shape preview is offset from the drag: " + context);
            if (pages && zoom == 1.0)
                window->grab().save(output.filePath("explore-shape-preview-window.png"));
            const int undoBeforeShape = UBApplication::undoStack->index();
            send(QEvent::MouseButtonRelease, bottomRight, Qt::LeftButton, Qt::NoButton);
            UBGraphicsSvgItem *shape = nullptr;
            for (auto candidate : scene->selectedItems())
                if (auto svg = dynamic_cast<UBGraphicsSvgItem*>(candidate); svg && svg->isTeachingShape()) shape = svg;
            if (!shape || !pointsClose(shapeAt(shape, QPointF(12,12)), topLeft)
                       || !pointsClose(shapeAt(shape, QPointF(12,12) + sceneSize), bottomRight)
                       || UBApplication::undoStack->index() <= undoBeforeShape) {
                window->grab().save(output.filePath("explore-shape-failure-window.png"));
                return fail(QString("Drawn rectangle placement or Undo is wrong: %1, shape=%2, top=%3, bottom=%4, undo=%5/%6")
                    .arg(context).arg(shape != nullptr)
                    .arg(shape ? QString("%1,%2").arg(shapeAt(shape,QPointF(12,12)).x()).arg(shapeAt(shape,QPointF(12,12)).y()) : "missing")
                    .arg(shape ? QString("%1,%2").arg(shapeAt(shape,QPointF(12,12) + sceneSize).x()).arg(shapeAt(shape,QPointF(12,12) + sceneSize).y()) : "missing")
                    .arg(UBApplication::undoStack->index()).arg(undoBeforeShape));
            }
            UBApplication::undoStack->undo();
            if (shape->scene() == scene) return fail("Shape Undo left the object on the page: " + context);
            UBApplication::undoStack->redo();
            qWarning("Exploration: rectangle and history checked");
            if (shape->scene() != scene || !pointsClose(shapeAt(shape, QPointF(12,12)), topLeft))
                return fail("Shape Redo moved the object: " + context);

            window->actionText->trigger();
            qWarning("Exploration: drawing text");
            const QPointF textStart(375, 275), textEnd(490, 345);
            send(QEvent::MouseButtonPress, textStart, Qt::LeftButton, Qt::LeftButton);
            QRubberBand *pendingTextBand = nullptr;
            for (auto band : view->findChildren<QRubberBand*>())
                if (band->isVisible()) pendingTextBand = band;
            if (!pendingTextBand) return fail("Text drag preview did not open: " + context);
            const QRect initialTextBand = pendingTextBand->geometry();
            send(QEvent::MouseMove, textStart + QPointF(2, 0), Qt::NoButton, Qt::LeftButton);
            if (pendingTextBand->geometry() != initialTextBand)
                return fail("Text drag began below the pixel threshold: " + context);
            send(QEvent::MouseMove, textStart + QPointF(QApplication::startDragDistance() + 3, 0),
                 Qt::NoButton, Qt::LeftButton);
            if (pendingTextBand->geometry().width() <= initialTextBand.width())
                return fail("Text drag missed the pixel threshold: " + context);
            send(QEvent::MouseMove, textEnd, Qt::NoButton, Qt::LeftButton);
            QRubberBand *textBand = nullptr;
            for (auto band : view->findChildren<QRubberBand*>()) if (band->isVisible()) textBand = band;
            if (!textBand || !pointsClose(textBand->mapToGlobal(QPoint()),
                                    view->viewport()->mapToGlobal(textStart.toPoint())))
                return fail("Text preview starts away from the pointer: " + context);
            if (pages && zoom == 1.0)
                window->grab().save(output.filePath("explore-text-preview-window.png"));
            send(QEvent::MouseButtonRelease, textEnd, Qt::LeftButton, Qt::NoButton);
            UBGraphicsTextItem *textItem = nullptr;
            for (auto candidate : scene->items())
                if (auto t = dynamic_cast<UBGraphicsTextItem*>(candidate)) textItem = t;
            if (!textItem || !pointsClose(view->mapFromScene(textItem->scenePos()), textStart))
                return fail("Text box starts away from the pointer: " + context);
            textItem->setPlainText("Classroom note");
            scene->clearFocus(); window->actionSelector->trigger();
            qWarning("Exploration: text checked");

            const int beforeCancel = UBApplication::undoStack->index();
            choice->trigger();
            send(QEvent::MouseButtonPress, QPointF(525,275), Qt::LeftButton, Qt::LeftButton);
            send(QEvent::MouseMove, QPointF(625,345), Qt::NoButton, Qt::LeftButton);
            window->actionPen->trigger();
            qWarning("Exploration: tool switched");
            send(QEvent::MouseButtonRelease, QPointF(625,345), Qt::LeftButton, Qt::NoButton);
            bool previewLeft = false;
            for (auto candidate : scene->items())
                if (auto path = dynamic_cast<QGraphicsPathItem*>(candidate); path && path->zValue() == 1900000)
                    previewLeft = true;
            if (previewLeft || UBApplication::undoStack->index() != beforeCancel)
                return fail("Switching tools left a shape preview or changed Undo: " + context);

            manager->persistDocumentScene(doc, scene, page, true, true);
            qWarning("Exploration: scene saved");
            auto reopened = UBSvgSubsetAdaptor::loadScene(doc, page);
            qWarning("Exploration: scene reloaded");
            bool savedMoved = false, savedShape = false;
            QStringList reopenedSvgs;
            for (auto candidate : reopened->items()) if (auto svg = dynamic_cast<UBGraphicsSvgItem*>(candidate)) {
                const QPointF origin = svg->mapToScene(QPointF(0,0));
                reopenedSvgs << QString("uuid=%1 origin=%2,%3 shape=%4")
                    .arg(svg->uuid().toString()).arg(origin.x()).arg(origin.y()).arg(svg->isTeachingShape());
                if (svg->uuid() == item->uuid()) savedMoved = pointsClose(origin, item->mapToScene(QPointF(0,0)));
                if (svg->uuid() == shape->uuid()) savedShape = pointsClose(origin, shape->mapToScene(QPointF(0,0)));
            }
            if (!savedMoved || !savedShape)
                return fail(QString("Saved objects reopened at different positions: %1, moved=%2 (%3, %4,%5), shape=%6 (%7, %8,%9), reopened=[%10]")
                    .arg(context).arg(savedMoved).arg(item->uuid().toString()).arg(moved.x()).arg(moved.y())
                    .arg(savedShape).arg(shape->uuid().toString()).arg(shape->pos().x()).arg(shape->pos().y())
                    .arg(reopenedSvgs.join("; ")));
            if (pages && zoom == 1.0)
                window->grab().save(output.filePath("explore-saved-objects-window.png"));
            if (pages && zoom == 1.7) {
                const qreal beforeZoomClick = board->currentZoom();
                window->actionZoomIn->trigger();
                send(QEvent::MouseButtonPress, QPointF(690,180), Qt::LeftButton, Qt::LeftButton);
                send(QEvent::MouseButtonRelease, QPointF(690,180), Qt::LeftButton, Qt::NoButton);
                const qreal afterZoomClick = board->currentZoom();
                if (afterZoomClick <= beforeZoomClick + 0.01)
                    return fail("Zoom tool click did not change zoom");
                window->actionSelector->trigger();
                for (const auto &kind : {"derivative", "quotient"}) {
                    auto insert = window->findChild<QAction*>(QString("insertFormula-%1").arg(kind));
                    if (!insert) return fail(QString("Missing %1 function tool").arg(kind));
                    insert->trigger();
                    UBGraphicsSvgItem *formula = nullptr;
                    for (auto candidate : scene->selectedItems())
                        if (auto svg = dynamic_cast<UBGraphicsSvgItem*>(candidate)) {
                            UBMathFormula::State state;
                            if (UBMathFormula::read(svg->fileData(), &state) && state.kind == kind) formula = svg;
                        }
                    if (!formula) return fail(QString("%1 formula was not inserted").arg(kind));
                    QRectF safe = view->viewport()->rect().adjusted(10,10,-10,-10);
                    if (auto dock = view->findChild<QWidget*>("paperDock"); dock && dock->isVisible())
                        safe.setBottom(dock->mapTo(view->viewport(), QPoint()).y() - 10);
                    const QRectF bounds = view->viewportTransform().mapRect(formula->sceneBoundingRect());
                    if (!safe.contains(bounds))
                        return fail(QString("%1 formula extends outside the visible lesson").arg(kind));
                    scene->clearSelection();
                    send(QEvent::MouseButtonPress, QPointF(690,300), Qt::LeftButton, Qt::LeftButton);
                    send(QEvent::MouseButtonRelease, QPointF(690,300), Qt::LeftButton, Qt::NoButton);
                    if (qAbs(board->currentZoom() - afterZoomClick) > 0.001)
                        return fail(QString("Selecting after %1 insertion unexpectedly changed zoom").arg(kind));
                    window->grab().save(output.filePath(QString("explore-%1-after-zoom-window.png").arg(kind)));
                }
            }
            ++page;
        }
    }
    settings->setValue("Teaching/PaperInkPages", previousPages); settings->save();
    if (auto paper = window->findChild<UBPaperInk*>()) paper->applyPreferences();
    window->setProperty("exploratoryInteractionsChecked", true);
    return true;
}

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
            auto list = dialog ? dialog->findChild<QListWidget*>("workspaceToolbarPreview") : nullptr;
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
    if(!pen||!shape||!cover||dock->findChild<QToolButton*>("paperMore"))return fail("Pen toolbar is missing tools or still contains More");
    auto settingsButton=header->findChild<QToolButton*>("paperMenu");
    auto settingsMenu=settingsButton?settingsButton->menu():nullptr;
    auto pageMenu=settingsMenu?settingsMenu->findChild<QMenu*>("paperPageMenu"):nullptr;
    auto workspaceMenu=settingsMenu?settingsMenu->findChild<QMenu*>("paperWorkspaceMenu"):nullptr;
    auto viewMenu=settingsMenu?settingsMenu->findChild<QMenu*>("paperViewMenu"):nullptr;
    auto versionsMenu=settingsMenu?settingsMenu->findChild<QMenu*>("paperDocumentsMenu"):nullptr;
    if(!settingsMenu||!pageMenu||!workspaceMenu||!viewMenu||!versionsMenu||settingsButton->icon().isNull()||settingsButton->toolTip()!=QObject::tr("Settings & more"))return fail("Categorized settings menu or cog is missing");
    if(!workspaceMenu->actions().contains(window->actionPreferences)||!workspaceMenu->actions().contains(window->findChild<QAction*>("actionTeachingCustomize"))||!versionsMenu->actions().contains(window->findChild<QAction*>("actionLessonSaveVersion")))return fail("Moved workspace and version commands are missing");
    for(auto submenu:settingsMenu->findChildren<QMenu*>())for(auto action:submenu->actions())if(QList<QAction*>{window->actionPlay,window->actionCapture,window->actionVirtualKeyboard,window->actionSnap}.contains(action))return fail("Optional tools remain in Settings instead of Customize Workspace");
    if(pageMenu->actions().size()<2||!pageMenu->actions().at(pageMenu->actions().size()-2)->isSeparator()||pageMenu->actions().last()!=window->actionClearPage)return fail("Clear Page is not separated in Page & paper");
    for(const auto &id:{"paperLesson","paperGraphDesk"}){auto action=header->findChild<QAction*>(id);if(!action||action->icon().isNull())return fail("Lesson or GraphDesk navigation icon is missing");}
    if(header->findChildren<QLabel*>().size()!=1)return fail("Document title remains in the top toolbar");
    auto library=settingsMenu->findChild<QAction*>("paperLibrary");
    if(!library)return fail("Library command was lost while moving menus");
    const bool libraryShown=library->isChecked();library->trigger();paper->sync();
    if(library->isChecked()==libraryShown)return fail("Moved Library command does not toggle");
    library->trigger();paper->sync();
    auto dockOpacity=qobject_cast<QGraphicsOpacityEffect*>(dock->graphicsEffect());
    if(!dockOpacity||qAbs(dockOpacity->opacity()-.8)>.001)return fail("Pen toolbar opacity is not 80 percent");
    if(window->findChild<QAction*>("actionLessonSaveVersion")->text()!=QObject::tr("Create saved version…"))return fail("Saved version label is unclear");
    window->actionSelector->trigger();pen->click();QApplication::processEvents();auto panel=window->findChild<QFrame*>("paperPenPanel");
    if(!panel||panel->isHidden()||!dock->isVisible()||header->height()>62||header->width()<window->width()-12)return fail("Paper pen controls or compact header are not visible");
    auto docs=window->findChild<QToolButton*>("paperDocuments");
    if(!docs||!docs->isVisible()||docs->text()!=QObject::tr("Documents"))return fail("Documents is not visible in the header");
    auto panelOpacity=qobject_cast<QGraphicsOpacityEffect*>(panel->graphicsEffect());
    if(!panelOpacity||qAbs(panelOpacity->opacity()-.8)>.001)return fail("Pen options opacity is not 80 percent");
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
    auto settings=UBSettings::settings();const auto previousZoom=settings->value("Teaching/Bottom/actionZoomIn",false);settings->setValue("Teaching/Bottom/actionZoomIn",true);paper->rebuild();QApplication::processEvents();
    auto zoomButton=dock->findChild<QToolButton*>("paper-actionZoomIn");if(!zoomButton||dock->findChild<QToolButton*>("paper-actionZoomOut"))return fail("Zoom is not a single configurable button");
    settings->setValue("Teaching/ZoomOutTool",false);window->actionZoomIn->trigger();
    QMouseEvent zoomDouble(QEvent::MouseButtonDblClick,QPointF(zoomButton->rect().center()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QMouseEvent zoomRelease(QEvent::MouseButtonRelease,QPointF(zoomButton->rect().center()),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(zoomButton,&zoomDouble);QApplication::sendEvent(zoomButton,&zoomRelease);
    if(!settings->value("Teaching/ZoomOutTool").toBool()||!window->actionZoomOut->isChecked()||zoomButton->text()!=QObject::tr("Zoom −"))return fail("Zoom double-click did not select zoom out");
    zoomButton->click();if(zoomButton->isChecked()||UBDrawingController::drawingController()->stylusTool()!=UBStylusTool::Selector)return fail("Deselecting Zoom left its native tool active");
    zoomButton->click();if(!window->actionZoomOut->isChecked()||!zoomButton->isChecked())return fail("Reselecting Zoom forgot the chosen direction");
    QApplication::sendEvent(zoomButton,&zoomDouble);QApplication::sendEvent(zoomButton,&zoomRelease);
    if(settings->value("Teaching/ZoomOutTool").toBool()||!window->actionZoomIn->isChecked())return fail("Zoom double-click did not return to zoom in");
    settings->setValue("Teaching/Bottom/actionZoomIn",previousZoom);paper->rebuild();window->actionPen->trigger();
    const auto handVisibility=settings->value("Teaching/Bottom/actionHand"),handOrder=settings->value("Teaching/BottomOrder");
    auto orderWithHand=handOrder.toStringList();if(orderWithHand.isEmpty())orderWithHand=UBPaperInk::defaultTools();
    if(!orderWithHand.contains("actionHand"))orderWithHand<<"actionHand";
    settings->setValue("Teaching/BottomOrder",orderWithHand);settings->setValue("Teaching/Bottom/actionHand",true);paper->rebuild();
    if(!dock->findChild<QToolButton*>("paper-actionHand")||dock->findChild<QToolButton*>("paperMore"))return fail("Optional Hand cannot be added without restoring More");
    settings->setValue("Teaching/Bottom/actionHand",handVisibility);settings->setValue("Teaching/BottomOrder",handOrder);
    paper->rebuild();
    const auto savedOrder=settings->value("Teaching/BottomOrder");bool cancelled=false;
    QTimer::singleShot(0,window,[&]{auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());auto preview=dialog?dialog->findChild<QListWidget*>("workspaceToolbarPreview"):nullptr;
        if(preview&&preview->count()){
            QSet<QString> availableIds;for(auto list:dialog->findChildren<QListWidget*>())for(int i=0;i<list->count();++i)availableIds.insert(list->item(i)->data(Qt::UserRole).toString());
            for(const auto &id:{"actionPlay","actionCapture","actionVirtualKeyboard","actionSnap"})if(!availableIds.contains(id)){dialog->reject();return;}
            if(dialog->findChild<QPushButton*>("workspaceRemove")->text()!=QObject::tr("Remove from toolbar")){dialog->reject();return;}
            preview->item(0)->setSelected(true);dialog->findChild<QPushButton*>("workspaceRemove")->click();cancelled=true;dialog->grab().save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("workspace-customize-window.png"));}if(dialog)dialog->reject();});
    window->findChild<QAction*>("actionTeachingCustomize")->trigger();if(!cancelled||settings->value("Teaching/BottomOrder")!=savedOrder)return fail("Cancel changed the saved workspace arrangement");
    bool contextOpened=false;
    QTimer::singleShot(0,window,[&]{if(auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){contextOpened=dialog->objectName()=="customizeWorkspace";dialog->reject();}});
    dock->customContextMenuRequested(QPoint(5,5));if(!contextOpened)return fail("Right-click pen toolbar customization is unavailable");
    // A densely customized tray must remain usable when it exceeds the view.
    const QStringList optional={"actionHand","actionPlay","actionCapture","actionVirtualKeyboard","actionSnap","actionZoomIn"};
    QMap<QString,QVariant> visibility;
    auto crowdedOrder=UBPaperInk::defaultTools();
    for(const auto &id:optional){visibility[id]=settings->value("Teaching/Bottom/"+id);settings->setValue("Teaching/Bottom/"+id,true);if(!crowdedOrder.contains(id))crowdedOrder<<id;}
    settings->setValue("Teaching/BottomOrder",crowdedOrder);paper->rebuild();paper->sync();QApplication::processEvents();
    auto overflow=settingsMenu->findChild<QMenu*>("paperToolbarOverflow");bool overflowWorks=overflow&&overflow->menuAction()->isVisible()&&!overflow->actions().isEmpty();
    for(auto tool:dock->findChildren<QToolButton*>(QString(),Qt::FindDirectChildrenOnly))if(tool->defaultAction())overflowWorks=overflowWorks&&(overflow->actions().contains(tool->defaultAction())==tool->isHidden());
    for(const auto &id:optional)settings->setValue("Teaching/Bottom/"+id,visibility[id]);
    settings->setValue("Teaching/BottomOrder",savedOrder);paper->rebuild();paper->sync();QApplication::processEvents();
    if(!overflowWorks)return fail("Crowded pen toolbar lost tools instead of exposing them in Settings overflow");
    settingsMenu->popup(settingsButton->mapToGlobal(QPoint(0,settingsButton->height())));QApplication::processEvents();
    window->grab().save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("settings-toolbar-window.png"));
    settingsMenu->grab().save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("settings-menu-window.png"));settingsMenu->hide();
    window->setProperty("paperInkChecked",true);return true;
}

bool checkPenTrayTablet(QString *error)
{
    auto window=UBApplication::mainWindow;auto board=UBApplication::boardController;
    auto dc=UBDrawingController::drawingController();auto settings=UBSettings::settings();
    const auto highRes=settings->boardUseHighResTabletEvent->get(),pressure=settings->boardPenPressureSensitive->get();
    settings->boardUseHighResTabletEvent->set(true);settings->boardPenPressureSensitive->set(true);
    auto restore=[&]{settings->boardUseHighResTabletEvent->set(highRes);settings->boardPenPressureSensitive->set(pressure);};
    auto fail=[&](const QString &message){restore();*error=message;return false;};
    auto doc=UBPersistenceManager::persistenceManager()->createNewDocument(UBPersistenceManager::myDocumentsName,"Stylus toolbar regression",true);
    board->setActiveDocumentScene(doc,0);auto scene=board->activeScene();
    auto view=board->controlView();auto viewport=view->viewport();
    auto paper=window->findChild<UBPaperInk*>();auto dock=window->findChild<QFrame*>("paperDock");
    auto panel=window->findChild<QFrame*>("paperPenPanel");auto pen=dock->findChild<QToolButton*>("paper-actionPen");
    window->actionSelector->trigger();pen->click();paper->sync();QApplication::processEvents();
    auto tablet=[&](QObject *receiver,QEvent::Type type,const QPoint &global,Qt::MouseButton button,Qt::MouseButtons buttons){
        auto widget=qobject_cast<QWidget*>(receiver);
        QTabletEvent event(type,QPointingDevice::primaryPointingDevice(),widget->mapFromGlobal(global),global,type==QEvent::TabletRelease?0:.8,0,0,0,0,0,Qt::NoModifier,button,buttons);
        QApplication::sendEvent(receiver,&event);
    };
    auto tap=[&](QWidget *target,bool deliveredToCanvas){const auto global=target->mapToGlobal(target->rect().center());QObject *receiver=deliveredToCanvas?viewport:target;
        tablet(receiver,QEvent::TabletPress,global,Qt::LeftButton,Qt::LeftButton);
        tablet(receiver,QEvent::TabletRelease,global,Qt::LeftButton,Qt::NoButton);QApplication::processEvents();};
    const int undo=UBApplication::undoStack->index(),items=scene->items().size();
    dc->setColorIndex(0);tap(panel->findChild<QToolButton*>("paperColour8"),true);
    if(dc->currentToolColorIndex()!=8||UBApplication::undoStack->index()!=undo||scene->items().size()!=items)return fail(QString("Stylus colour tap failed: colour=%1, undo=%2/%3, items=%4/%5, panelVisible=%6").arg(dc->currentToolColorIndex()).arg(UBApplication::undoStack->index()).arg(undo).arg(scene->items().size()).arg(items).arg(panel->isVisible()));
    tap(panel->findChild<QToolButton*>("paperColour3"),false);
    tap(panel->findChild<QToolButton*>("paperWidth2"),true);
    if(dc->currentToolColorIndex()!=3||dc->currentToolWidthIndex()!=2)return fail("Stylus cannot change colour or thickness on the pen panel");
    const QPoint colourPress=panel->findChild<QToolButton*>("paperColour5")->mapToGlobal(panel->findChild<QToolButton*>("paperColour5")->rect().center());
    const QPoint outside=viewport->mapToGlobal(QPoint(viewport->width()/2,viewport->height()/3));
    tablet(viewport,QEvent::TabletPress,colourPress,Qt::LeftButton,Qt::LeftButton);tablet(viewport,QEvent::TabletMove,outside,Qt::NoButton,Qt::LeftButton);tablet(viewport,QEvent::TabletRelease,outside,Qt::LeftButton,Qt::NoButton);
    if(dc->currentToolColorIndex()!=3||UBApplication::undoStack->index()!=undo)return fail("Dragging the stylus off a colour applied it or leaked ink");
    tap(dock->findChild<QToolButton*>("paper-actionMarker"),true);
    if(dc->stylusTool()!=UBStylusTool::Marker)return fail("Stylus cannot select Highlighter from the pen tray");
    tap(dock->findChild<QToolButton*>("paper-actionEraser"),false);
    if(dc->stylusTool()!=UBStylusTool::Eraser)return fail("Stylus cannot select Eraser from the pen tray");
    tap(pen,true);
    if(dc->stylusTool()!=UBStylusTool::Pen||UBApplication::undoStack->index()!=undo||scene->items().size()!=items)return fail("Stylus tool selection created lesson ink");
    auto line=dock->findChild<QToolButton*>("paper-actionLine");const bool dashed=settings->value("Teaching/DashedLines",false).toBool();
    tap(line,true);tap(line,true);
    if(settings->value("Teaching/DashedLines").toBool()==dashed)return fail("Stylus double-tap no longer changes the combined Line style");
    dc->setLineTool(dashed);tap(pen,true);
    const QPoint margin=dock->mapToGlobal(QPoint(3,3));tablet(viewport,QEvent::TabletPress,margin,Qt::LeftButton,Qt::LeftButton);tablet(viewport,QEvent::TabletRelease,margin,Qt::LeftButton,Qt::NoButton);
    if(UBApplication::undoStack->index()!=undo||scene->items().size()!=items)return fail("Stylus tapping the toolbar background creates ink");
    const QPoint start=viewport->mapToGlobal(QPoint(viewport->width()/2,viewport->height()/3)),end=start+QPoint(90,30);
    tablet(viewport,QEvent::TabletPress,start,Qt::LeftButton,Qt::LeftButton);tablet(viewport,QEvent::TabletMove,end,Qt::NoButton,Qt::LeftButton);tablet(viewport,QEvent::TabletRelease,end,Qt::LeftButton,Qt::NoButton);
    if(UBApplication::undoStack->index()<=undo)return fail("Routing tablet controls stopped pressure-sensitive canvas drawing");
    UBApplication::undoStack->undo();dc->setColorIndex(0);
    // Propagated tablet packets may have parent-relative local coordinates.
    // Their global position must still land at the same viewport point.
    auto shiftedTablet=[&](QEvent::Type type,const QPoint &global,Qt::MouseButton button,Qt::MouseButtons buttons){
        QTabletEvent event(type,QPointingDevice::primaryPointingDevice(),viewport->mapFromGlobal(global)+QPoint(0,48),global,type==QEvent::TabletRelease?0:.8,0,0,0,0,0,Qt::NoModifier,button,buttons);
        QApplication::sendEvent(viewport,&event);
    };
    shiftedTablet(QEvent::TabletPress,start,Qt::LeftButton,Qt::LeftButton);shiftedTablet(QEvent::TabletMove,end,Qt::NoButton,Qt::LeftButton);shiftedTablet(QEvent::TabletRelease,end,Qt::LeftButton,Qt::NoButton);
    QApplication::processEvents();const auto image=viewport->grab().toImage();const auto expected=viewport->mapFromGlobal((start+end)/2);bool inkAtCursor=false;
    for(int dy=-3;dy<=3;++dy)for(int dx=-3;dx<=3;++dx){const QPoint pixel=(expected+QPoint(dx,dy))*image.devicePixelRatio();if(image.rect().contains(pixel)&&image.pixelColor(pixel).lightness()<180)inkAtCursor=true;}
    if(!inkAtCursor)return fail("Tablet ink is displaced from the global cursor position by parent-relative coordinates");
    restore();window->setProperty("penTrayTabletChecked",true);return true;
}

bool checkShapeThenFunctions(QString *error)
{
    auto window=UBApplication::mainWindow;auto board=UBApplication::boardController;auto view=board->controlView();auto viewport=view->viewport();
    auto settle=[](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
    class InputProbe : public QObject {
    public:
        int tablets=0,mouse=0;
        bool eventFilter(QObject *o,QEvent *e) override {
            if(e->spontaneous()){if(e->type()==QEvent::TabletPress)++tablets;if(e->type()==QEvent::MouseButtonPress)++mouse;}
            if(e->type()==QEvent::TabletPress||e->type()==QEvent::TabletRelease){auto t=static_cast<QTabletEvent*>(e);qWarning()<<"PEN SEQUENCE"<<o->metaObject()->className()<<o->objectName()<<e->type()<<e->spontaneous()<<t->globalPosition()<<t->button()<<t->buttons()<<"popup"<<QApplication::activePopupWidget();}
            return false;
        }
    } probe;
    qApp->installEventFilter(&probe);
    struct RemoveProbe {QObject *probe;~RemoveProbe(){qApp->removeEventFilter(probe);}} removeProbe{&probe};
#ifdef Q_OS_WIN
    const auto user=GetModuleHandleW(L"user32.dll");
    auto create=reinterpret_cast<HANDLE(WINAPI *)(POINTER_INPUT_TYPE,ULONG,POINTER_FEEDBACK_MODE)>(GetProcAddress(user,"CreateSyntheticPointerDevice"));
    auto inject=reinterpret_cast<BOOL(WINAPI *)(HANDLE,const POINTER_TYPE_INFO*,UINT32)>(GetProcAddress(user,"InjectSyntheticPointerInput"));
    auto destroy=reinterpret_cast<void(WINAPI *)(HANDLE)>(GetProcAddress(user,"DestroySyntheticPointerDevice"));
    HANDLE device=create&&inject&&destroy?create(PT_PEN,1,POINTER_FEEDBACK_NONE):nullptr;
    struct ReleaseDevice {HANDLE device;decltype(destroy) release;~ReleaseDevice(){if(device)release(device);}} releaseDevice{device,destroy};
    if(!device){*error=QString("Windows pen injection unavailable: %1").arg(GetLastError());return false;}
#endif
    for(bool native:{false,true}) {
#ifndef Q_OS_WIN
        if(native)continue;
#endif
        QString injectionError;
        auto packet=[&](QEvent::Type type,const QPoint &global){
#ifdef Q_OS_WIN
            if(native){
                POINTER_TYPE_INFO input={};input.type=PT_PEN;auto &pen=input.penInfo;pen.pointerInfo.pointerType=PT_PEN;pen.pointerInfo.pointerId=1;
                const auto scale=window->devicePixelRatioF();pen.pointerInfo.ptPixelLocation={qRound(global.x()*scale),qRound(global.y()*scale)};
                pen.pointerInfo.pointerFlags=POINTER_FLAG_INRANGE|(type==QEvent::TabletPress?POINTER_FLAG_DOWN|POINTER_FLAG_INCONTACT|POINTER_FLAG_FIRSTBUTTON:type==QEvent::TabletRelease?POINTER_FLAG_UP:POINTER_FLAG_UPDATE|POINTER_FLAG_INCONTACT|POINTER_FLAG_FIRSTBUTTON);
                pen.penMask=PEN_MASK_PRESSURE;pen.pressure=type==QEvent::TabletRelease?0:800;
                if(!inject(device,&input,1))injectionError=QString("InjectSyntheticPointerInput: %1").arg(GetLastError());return;
            }
#endif
            QTabletEvent e(type,QPointingDevice::primaryPointingDevice(),viewport->mapFromGlobal(global),global,type==QEvent::TabletRelease?0:.8,0,0,0,0,0,Qt::NoModifier,type==QEvent::TabletMove?Qt::NoButton:Qt::LeftButton,type==QEvent::TabletRelease?Qt::NoButton:Qt::LeftButton);QApplication::sendEvent(viewport,&e);
        };
        auto tap=[&](QPoint global){
            if(native){packet(QEvent::TabletPress,global);QTimer::singleShot(35,window,[&,global]{packet(QEvent::TabletRelease,global);});settle(100);}
            else {packet(QEvent::TabletPress,global);packet(QEvent::TabletRelease,global);QApplication::processEvents();}
        };
        const auto mode=native?QString("Windows pen"):QString("Qt tablet");
        auto fail=[&](QString message){window->grab().save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("shape-functions-failure-window.png"));*error=mode+": "+message+QString("; spontaneous tablets=%1 mouse=%2 injection=%3").arg(probe.tablets).arg(probe.mouse).arg(injectionError);return false;};
        window->actionPen->trigger();
        auto doc=UBPersistenceManager::persistenceManager()->createNewDocument(UBPersistenceManager::myDocumentsName,"Triangle then Functions "+mode,true);board->setActiveDocumentScene(doc,0);auto scene=board->activeScene();scene->setNominalSize(800,600);scene->setBackground(false,plain);
        window->raise();window->activateWindow();settle(100);
        auto shapes=window->findChild<QToolButton*>("paper-actionTeachingShapes");auto functions=window->findChild<QToolButton*>("paper-actionTeachingFunctions");auto menu=window->findChild<QMenu*>("mathscrapFunctionsMenu");
        if(!shapes||!functions||!menu)return fail("Required buttons missing");
        tap(shapes->mapToGlobal(shapes->rect().center()));
        auto bar=window->findChild<QToolBar*>("lessonShapeToolbar");QToolButton *triangle=nullptr;
        for(auto b:bar->findChildren<QToolButton*>())if(b->defaultAction()&&b->defaultAction()->objectName()=="lessonShape3")triangle=b;
        if(!bar->isVisible()||!triangle)return fail("Shapes tap did not reveal triangle controls");
        tap(triangle->mapToGlobal(triangle->rect().center()));
        const auto a=viewport->mapToGlobal(view->mapFromScene(QPointF(-130,-80))),b=viewport->mapToGlobal(view->mapFromScene(QPointF(130,70)));
        packet(QEvent::TabletPress,a);
        if(native){QTimer::singleShot(35,window,[&]{packet(QEvent::TabletMove,(a+b)/2);});QTimer::singleShot(70,window,[&]{packet(QEvent::TabletMove,b);});QTimer::singleShot(105,window,[&]{packet(QEvent::TabletRelease,b);});settle(180);}
        else{packet(QEvent::TabletMove,b);packet(QEvent::TabletRelease,b);QApplication::processEvents();}
        int triangles=0;for(auto item:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item))if(svg->isTeachingShape())++triangles;
        if(triangles!=1)return fail(QString("Triangle was not inserted (count %1)").arg(triangles));
        // No intervening tools, colour menus, or programmatic popup reset.
        auto choice=menu->findChild<QAction*>("insertFormula-pythagoras");if(!choice)return fail("Pythagorean action missing");bool selected=false,visible=false;
        auto connection=QObject::connect(choice,&QAction::triggered,window,[&]{selected=true;});
        QTimer guard;guard.setSingleShot(true);QObject::connect(&guard,&QTimer::timeout,window,[]{if(auto p=QApplication::activePopupWidget())p->close();});guard.start(2500);
        QTimer::singleShot(300,&guard,[&]{visible=menu->isVisible();if(visible)tap(menu->mapToGlobal(menu->actionGeometry(choice).center()));});
        tap(functions->mapToGlobal(functions->rect().center()));
        if(menu->isVisible())settle(500);
        guard.stop();QObject::disconnect(connection);
        int formulae=0;for(auto item:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item))if(svg->fileData().contains("mathscrap-formula"))++formulae;
        if(!selected||formulae!=1||!injectionError.isEmpty())return fail(QString("Functions after triangle: menu=%1 chosen=%2 formulae=%3 Select=%4").arg(visible).arg(selected).arg(formulae).arg(window->actionSelector->isChecked()));
        if(native&&probe.tablets==0)return fail("Injected input did not reach Qt as pen events");
    }
    window->setProperty("shapeThenFunctionsChecked",true);return true;
}

bool checkTabletMenus(QString *error)
{
    auto window=UBApplication::mainWindow;auto view=UBApplication::boardController->controlView();auto viewport=view->viewport();
    auto packet=[&](QEvent::Type type,const QPoint &global,Qt::MouseButton button,Qt::MouseButtons buttons){QTabletEvent e(type,QPointingDevice::primaryPointingDevice(),viewport->mapFromGlobal(global),global,type==QEvent::TabletRelease?0:(buttons?.8:0),0,0,0,0,0,Qt::NoModifier,button,buttons);QApplication::sendEvent(viewport,&e);};
    auto hover=[&](const QPoint &global){packet(QEvent::TabletMove,global,Qt::NoButton,Qt::NoButton);};
    auto tap=[&](const QPoint &global){hover(global);packet(QEvent::TabletPress,global,Qt::LeftButton,Qt::LeftButton);packet(QEvent::TabletRelease,global,Qt::LeftButton,Qt::NoButton);hover(global);QApplication::processEvents();};
    auto functions=window->findChild<QToolButton*>("paper-actionTeachingFunctions");auto menu=window->findChild<QMenu*>("mathscrapFunctionsMenu");
    if(!functions||!menu){*error="Functions popup unavailable";return false;}
    bool selected=false,callback=false,menuAtCallback=false;QString popupName;auto choice=menu->actions().first();auto connection=QObject::connect(choice,&QAction::triggered,window,[&]{selected=true;});
    QTimer guard;guard.setSingleShot(true);QObject::connect(&guard,&QTimer::timeout,window,[]{if(auto popup=QApplication::activePopupWidget())popup->close();});guard.start(2000);
    QTimer::singleShot(250,&guard,[&]{callback=true;menuAtCallback=menu->isVisible();if(auto p=QApplication::activePopupWidget())popupName=p->objectName()+p->metaObject()->className();if(menu->isVisible())tap(menu->mapToGlobal(menu->actionGeometry(choice).center()));});
    tap(functions->mapToGlobal(functions->rect().center()));guard.stop();QObject::disconnect(connection);
    if(!selected||menu->isVisible()){window->grab().save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("popup-failure-window.png"));*error=QString("Functions menu: selected=%1 visible=%2 buttonVisible=%3 timer=%4 menuAtTimer=%5 popup=%6 button=%7,%8").arg(selected).arg(menu->isVisible()).arg(functions->isVisible()).arg(callback).arg(menuAtCallback).arg(popupName).arg(functions->mapToGlobal(QPoint()).x()).arg(functions->mapToGlobal(QPoint()).y());return false;}
    menu->popup(functions->mapToGlobal(QPoint(0,-menu->sizeHint().height())));QApplication::processEvents();
    const int before=UBApplication::undoStack->index();tap(viewport->mapToGlobal(QPoint(viewport->width()/2,viewport->height()/4)));
    if(menu->isVisible()||UBApplication::undoStack->index()!=before){*error="Stylus outside tap did not dismiss Functions safely";return false;}
    auto board=UBApplication::boardController;
    auto doc=UBPersistenceManager::persistenceManager()->createNewDocument(UBPersistenceManager::myDocumentsName,"Stylus shape menus",true);
    board->setActiveDocumentScene(doc,0);auto scene=board->activeScene();scene->setNominalSize(800,600);scene->setBackground(false,plain);
    auto shapes=window->findChild<QAction*>("actionTeachingShapes");shapes->trigger();QApplication::processEvents();
    auto bar=window->findChild<QToolBar*>("lessonShapeToolbar");auto panel=bar->findChild<QWidget*>("compactShapePanel");
    QToolButton *triangle=nullptr;for(auto b:panel->findChildren<QToolButton*>())if(b->defaultAction()&&b->defaultAction()->objectName()=="lessonShape3")triangle=b;
    if(!triangle||bar->height()>200||bar->width()>560){*error="Compact three-row shape controls are missing or oversized";return false;}
    tap(triangle->mapToGlobal(triangle->rect().center()));if(!triangle->isChecked()){*error="Stylus cannot select a shape directly";return false;}
    auto drawShape=[&]() -> UBGraphicsSvgItem* {
        tap(triangle->mapToGlobal(triangle->rect().center()));
        const auto a=viewport->mapToGlobal(view->mapFromScene(QPointF(-130,-80))),b=viewport->mapToGlobal(view->mapFromScene(QPointF(130,70)));
        packet(QEvent::TabletPress,a,Qt::LeftButton,Qt::LeftButton);packet(QEvent::TabletMove,b,Qt::NoButton,Qt::LeftButton);packet(QEvent::TabletRelease,b,Qt::LeftButton,Qt::NoButton);hover(b);QApplication::processEvents();
        for(auto item:scene->selectedItems())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item))if(svg->isTeachingShape())return svg;
        return nullptr;
    };
    auto drawn=drawShape();if(!drawn){*error="Stylus cannot draw the first shape before changing its appearance";return false;}
    auto width=bar->findChild<QToolButton*>("compactShapeWidth1");auto style=bar->findChild<QToolButton*>("compactShapeStyle1");
    tap(width->mapToGlobal(width->rect().center()));tap(width->mapToGlobal(width->rect().center()));
    tap(style->mapToGlobal(style->rect().center()));tap(style->mapToGlobal(style->rect().center()));
    if(!width->isChecked()||!style->isChecked()){*error="Compact shape preset selection toggled off";return false;}
    auto border=bar->findChild<QToolButton*>("lessonShapeBorder");auto colours=border->menu();bool changed=false;auto none=colours->actions().first();auto c=QObject::connect(none,&QAction::triggered,window,[&]{changed=true;});
    guard.start(2000);QTimer::singleShot(250,&guard,[&]{if(colours->isVisible())tap(colours->mapToGlobal(colours->actionGeometry(none).center()));});tap(border->mapToGlobal(border->rect().center()));guard.stop();QObject::disconnect(c);
    if(!changed||colours->isVisible()){*error="Stylus cannot select a shape colour-menu item";return false;}
    // Exercise actual QWidgetAction colour swatches, not just the plain No border action.
    for(const auto &name:{QString("lessonShapeBorder"),QString("lessonShapeFill")})for(int index:{2,5}){
        auto button=bar->findChild<QToolButton*>(name);auto palette=button->menu();
        auto swatches=palette->findChildren<QToolButton*>();if(swatches.size()<=index){*error="Shape swatches missing";return false;}
        auto swatch=swatches[index];bool clicked=false;auto c=QObject::connect(swatch,&QToolButton::clicked,window,[&]{clicked=true;});
        const int undo=UBApplication::undoStack->index();
        guard.start(2000);QTimer::singleShot(250,&guard,[&]{if(palette->isVisible())tap(swatch->mapToGlobal(swatch->rect().center()));});
        tap(button->mapToGlobal(button->rect().center()));guard.stop();QObject::disconnect(c);
        if(!clicked||palette->isVisible()){*error="Stylus cannot choose an actual swatch: "+name;return false;}
        if(UBApplication::undoStack->index()>undo+1){*error="Shape swatch tap created extra lesson ink";return false;}
        QDomDocument styled;styled.setContent(drawn->fileData());const auto g=styled.elementsByTagName("g").at(0).toElement();
        const QString attribute=name.endsWith("Border")?"stroke":"fill";
        if(QColor(g.attribute(attribute))!=QColor(swatch->toolTip())){*error="Stylus colour did not apply to the selected shape: "+name;return false;}
        drawn=drawShape();if(!drawn){*error="Cannot draw another shape after changing its appearance";return false;}
        QDomDocument next;next.setContent(drawn->fileData());if(QColor(next.elementsByTagName("g").at(0).toElement().attribute(attribute))!=QColor(swatch->toolTip())){*error="Next shape lost the colour selected by stylus";return false;}

    }
    // Hover over a popup and dismiss it with the mouse: the next stylus press must
    // still start a new gesture (hover is never tip contact).
    menu->popup(functions->mapToGlobal(QPoint(0,-menu->sizeHint().height())));QApplication::processEvents();
    hover(menu->mapToGlobal(menu->rect().center()));menu->close();QApplication::processEvents();
    selected=false;connection=QObject::connect(choice,&QAction::triggered,window,[&]{selected=true;});
    guard.start(2000);QTimer::singleShot(250,&guard,[&]{if(menu->isVisible())tap(menu->mapToGlobal(menu->actionGeometry(choice).center()));});
    tap(functions->mapToGlobal(functions->rect().center()));guard.stop();QObject::disconnect(connection);
    if(!selected||menu->isVisible()){*error="Functions stylus tap stopped working after popup hover and dismissal";return false;}
    // A menu action may enter a modal loop before trigger() returns. Its
    // dialog must receive tablet packets rather than the old toolbar gesture.
    class TabletProbeDialog : public QDialog {
    public:
        using QDialog::QDialog;
        int packets=0;
        bool event(QEvent *e) override {
            if(e->type()==QEvent::TabletMove||e->type()==QEvent::TabletPress||e->type()==QEvent::TabletRelease){++packets;e->ignore();return true;}
            return QDialog::event(e);
        }
    };
    auto modalAction=menu->addAction("Tablet modal regression");bool modalReached=false,modalBlocked=false;
    auto modalConnection=QObject::connect(modalAction,&QAction::triggered,window,[&]{
        TabletProbeDialog dialog(window);dialog.setTabletTracking(true);dialog.resize(180,80);dialog.move(viewport->mapToGlobal(QPoint(viewport->width()/2,viewport->height()/4)));
        QTimer::singleShot(0,&dialog,[&]{
            const auto local=dialog.rect().center(),global=dialog.mapToGlobal(local);
            for(auto type:{QEvent::TabletMove,QEvent::TabletPress,QEvent::TabletRelease}){
                QTabletEvent e(type,QPointingDevice::primaryPointingDevice(),local,global,type==QEvent::TabletPress?.8:0,0,0,0,0,0,Qt::NoModifier,type==QEvent::TabletMove?Qt::NoButton:Qt::LeftButton,type==QEvent::TabletPress?Qt::LeftButton:Qt::NoButton);
                QApplication::sendEvent(&dialog,&e);
            }
            modalBlocked=dialog.packets!=3;modalReached=true;dialog.accept();
        });dialog.exec();
    });
    guard.start(2000);QTimer::singleShot(250,&guard,[&]{if(menu->isVisible())tap(menu->mapToGlobal(menu->actionGeometry(modalAction).center()));});
    tap(functions->mapToGlobal(functions->rect().center()));guard.stop();QObject::disconnect(modalConnection);delete modalAction;
    if(!modalReached||modalBlocked){*error="A menu action's modal dialog inherited the old stylus gesture";return false;}
    window->grab().save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("compact-shapes-window.png"));
    window->actionPen->trigger();window->setProperty("tabletMenusChecked",true);return true;
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
    QTimer::singleShot(0,window,[&]{auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!d){clean=false;return;}for(auto list:d->findChildren<QListWidget*>())for(int i=0;i<list->count();++i)if(list->item(i)->text().startsWith("teachingGroup")||list->item(i)->data(Qt::UserRole).toString()=="teachingGroup3"||list->item(i)->data(Qt::UserRole).toString()=="actionPointer")clean=false;d->reject();});
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
        if(bar->parentWidget()!=board->controlView()||bar->geometry().intersects(dock->geometry())||bar->height()>200)return fail(QString("Shape panel layout: parent=%1 height=%2 panel=%3,%4,%5,%6 dock=%7,%8,%9,%10")
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

bool checkDocumentThumbnails(QString *error)
{
    auto manager=UBPersistenceManager::persistenceManager();
    auto doc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Thumbnail loading regression",false);
    if(!doc){*error="Cannot create thumbnail regression lesson";return false;}
    for(int i=0;i<3;++i){
        auto page=manager->createDocumentSceneAt(doc,i,false);
        manager->persistDocumentScene(doc,page,i,false,true);
        QFile::remove(UBThumbnailAdaptor::thumbnailUrl(doc,i).toLocalFile());
    }
    UBApplication::applicationController->showDocument();
    // Force the loader's in-flight window: all paths can be dequeued while
    // none of their results is ready, even for a small three-page lesson.
    qputenv("TEACHING_THUMBNAIL_TEST_DELAY_MS","120");
    UBApplication::documentController->selectDocument(doc);
    auto widget=UBApplication::mainWindow->findChild<UBDocumentThumbnailWidget*>("thumbnailWidget");
    if(!widget||!widget->document()||widget->document()->proxy()!=doc){qunsetenv("TEACHING_THUMBNAIL_TEST_DELAY_MS");*error="Documents did not select the three-page lesson";return false;}
    auto thumbnails=widget->document()->thumbnailScene();
    QElapsedTimer timer;timer.start();
    int visible=0;
    while(timer.elapsed()<3500){
        QApplication::processEvents();visible=0;
        for(auto candidate:thumbnails->items())if(auto thumb=dynamic_cast<UBThumbnail*>(candidate);thumb&&!thumb->pixmap().isNull())++visible;
        if(visible==3)break;
        QThread::msleep(15);
    }
    qunsetenv("TEACHING_THUMBNAIL_TEST_DELAY_MS");
    const auto output=QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir();
    UBApplication::mainWindow->grab().save(output.filePath("documents-thumbnails-window.png"));
    if(visible!=3||thumbnails->thumbnailCount()!=3){*error=QString("Documents loaded %1 of 3 thumbnails after %2 ms (pages=%3, slots=%4)").arg(visible).arg(timer.elapsed()).arg(doc->pageCount()).arg(thumbnails->thumbnailCount());return false;}
    for(int i=0;i<3;++i)if(!QFileInfo::exists(UBThumbnailAdaptor::thumbnailUrl(doc,i).toLocalFile())){*error=QString("Page %1 thumbnail was not generated").arg(i+1);return false;}
    const auto thumb=thumbnails->thumbnailAt(0);
    const auto savedPixmap=thumb->pixmap();
    QPixmap darkPixmap(savedPixmap.size());darkPixmap.fill(Qt::black);
    thumb->setPixmap(darkPixmap);
    thumbnails->arrangeThumbnails(0,1);
    widget->viewport()->update();QApplication::processEvents();
    const auto screenPoint=widget->mapFromScene(thumb->mapToScene(thumb->boundingRect().center()));
    const auto screenImage=widget->viewport()->grab().toImage();
    if(!screenImage.rect().contains(screenPoint)){*error="Thumbnail preview is outside the Documents viewport";return false;}
    const auto screenPixel=screenImage.pixelColor(screenPoint);
    if(screenPixel.red()>150||screenPixel.green()>150||screenPixel.blue()>150){*error=QString("Dark thumbnail preview is hidden in Documents (%1,%2,%3)").arg(screenPixel.red()).arg(screenPixel.green()).arg(screenPixel.blue());return false;}
    thumb->setPixmap(savedPixmap);
    // Re-enter Documents and select the lesson again; cached pages must remain.
    UBApplication::applicationController->showBoard();
    UBApplication::applicationController->showDocument();
    UBApplication::documentController->selectDocument(doc);
    QElapsedTimer reopenTimer;reopenTimer.start();int reopened=0;
    while(reopenTimer.elapsed()<3500){
        QApplication::processEvents();reopened=0;
        if(widget->document())for(auto candidate:widget->document()->thumbnailScene()->items())if(auto thumb=dynamic_cast<UBThumbnail*>(candidate);thumb&&!thumb->pixmap().isNull())++reopened;
        if(reopened==3)break;
        QThread::msleep(15);
    }
    if(reopened!=3){*error=QString("Documents reopened with %1 of 3 thumbnails").arg(reopened);return false;}
    UBApplication::mainWindow->setProperty("documentsThumbnailsChecked",true);
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
    UBApplication::mainWindow->grab().save(QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir().filePath("mathscrap-documents-window.png"));
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
    report["formulaZoomRegressionChecked"] = UBApplication::mainWindow->property("formulaZoomRegressionChecked").toBool();
    report["preferencesRedesignChecked"] = UBApplication::mainWindow->property("preferencesRedesignChecked").toBool();
    report["selectionOriginChecked"] = UBApplication::mainWindow->property("selectionOriginChecked").toBool();
    report["exploratoryInteractionsChecked"] = UBApplication::mainWindow->property("exploratoryInteractionsChecked").toBool();
    report["pdfSelectionChecked"] = qEnvironmentVariable("TEACHING_TEST_SKIP_PDF") != "1";
    report["shapeToolbarChecked"] = UBApplication::mainWindow->property("shapeToolbarChecked").toBool();
    report["lessonControlsAndShapesChecked"] = UBApplication::mainWindow->property("lessonControlsAndShapesChecked").toBool();
    report["windowChromeChecked"] = UBApplication::mainWindow->property("windowChromeChecked").toBool();
    report["toolbarLayoutChecked"] = UBApplication::mainWindow->property("toolbarLayoutChecked").toBool();
    report["shapeThenFunctionsChecked"] = UBApplication::mainWindow->property("shapeThenFunctionsChecked").toBool();
    report["tabletMenusChecked"] = UBApplication::mainWindow->property("tabletMenusChecked").toBool();
    report["penTrayTabletChecked"] = UBApplication::mainWindow->property("penTrayTabletChecked").toBool();
    report["settingsToolbarChecked"] = UBApplication::mainWindow->property("paperInkChecked").toBool();
    report["documentsThumbnailsChecked"] = UBApplication::mainWindow->property("documentsThumbnailsChecked").toBool();
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
        if (qEnvironmentVariable("TEACHING_EXPLORE_ONLY") == "1") {
            QString detail;
            if (!checkExploratoryInteractions(&detail)) { finish(false, detail); return; }
            finish(true, "Exploratory pointer, object, tool, Undo/Redo and save/reopen checks passed");
            return;
        }
        QString chromeError;
        if (!checkWindowAndToolbar(&chromeError) || !checkLessonControlsAndShapes(&chromeError) || !checkShapeToolbar(&chromeError) || !checkPaperInk(&chromeError) || !checkShapeThenFunctions(&chromeError) || !checkPenTrayTablet(&chromeError) || !checkTabletMenus(&chromeError) || !checkCircleErase(&chromeError) || !checkPreferencesRedesign(&chromeError) || !checkSelectionOrigin(&chromeError)) { finish(false, chromeError); return; }
        if(!checkMathFormulae(&chromeError)){finish(false,chromeError);return;}
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
            QImage image(renderer.defaultSize()*2,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter painter(&image);renderer.render(&painter);painter.end();image.save(formulaOutput.filePath(QString("quadratic-stage-%1-window.png").arg(stage)));
        }
        example.step=5;
        auto finalFormula=UBQuadraticFormula::render(example);
        UBGraphicsSvgItem growingFormula(first);
        const qreal initialFormulaHeight=growingFormula.boundingRect().height();
        growingFormula.setTeachingSvg(finalFormula);
        if(growingFormula.boundingRect().height()<=initialFormulaHeight || qAbs(growingFormula.boundingRect().height()-growingFormula.renderer()->viewBoxF().height())>1){finish(false,"Quadratic page object squeezed growing working into old bounds");return;}
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
                if (report.value("ok").toBool() && !checkDocumentThumbnails(&exportError)) {
                    finish(false, exportError); return;
                }
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
