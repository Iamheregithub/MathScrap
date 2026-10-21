// SPDX-License-Identifier: GPL-3.0-only
#include "UBTeachingSmokeTest.h"
#include <QtWidgets>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QJsonDocument>
#include <QJsonObject>
#include "core/UBApplication.h"
#include "core/UBSettings.h"
#include "gui/UBMainWindow.h"
#include "UBBoardController.h"
#include "UBDrawingController.h"
#include "domain/UBGraphicsScene.h"
#include "domain/UBGraphicsSvgItem.h"
#include "domain/UBGraphicsStrokesGroup.h"
#include "document/UBDocumentProxy.h"
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
        renderer->setDPI(72);
        for (int i = 0; good && i < expected.size(); ++i) {
            good = qAbs(renderer->pointSizeF(i+1).width() - (400 + expected[i]*20)) < 3;
            QImage image(600,400,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::white);
            QPainter painter(&image); renderer->render(&painter, i+1, false); painter.end();
            const auto blue = image.pixelColor(20,20), red = image.pixelColor(70,20);
            good = good && blue.blue() > 200 && blue.red() < 60 && red.red() > 200 && red.blue() < 60;
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
            if (UBApplication::documentController->selectedDocumentProxy() != doc || !exporter->associatedAction()->isEnabled()) {
                *error = QString("PDF test selection unavailable: correct document=%1, enabled=%2, pages=%3, selected=%4")
                    .arg(UBApplication::documentController->selectedDocumentProxy() == doc)
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
                        dialog->selectFile(file);
                        QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                    }
            });
            dialogTimer.start(50);
            exporter->associatedAction()->trigger();
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
        shape->setTeachingSvg(edited); shape->commitTeachingSvg(before);
        UBApplication::undoStack->undo();
        if (shape->fileData() != before) { finish(false,"Shape undo failed"); return; }
        UBApplication::undoStack->redo();
        if (shape->fileData() != edited) { finish(false,"Shape redo failed"); return; }
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
}
