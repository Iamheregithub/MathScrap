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

namespace {
void finish(bool passed, const QString &detail) {
    const QString path = qEnvironmentVariable("TEACHING_SMOKE_REPORT");
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::WriteOnly)) { qApp->exit(19); return; }
    QJsonObject report;
    report["passed"] = passed;
    report["detail"] = detail;
    report["qt"] = qVersion();
    file.write(QJsonDocument(report).toJson());
    file.close();
    qApp->exit(passed ? 0 : 18);
}
}
void scheduleTeachingSmokeTest(QObject *context)
{
    QTimer::singleShot(60000, context, [] { finish(false, "Native test timed out"); });
    QTimer::singleShot(7000, context, [] {
        auto board = UBApplication::boardController;
        if (!board || !board->activeScene() || !UBApplication::mainWindow) {
            finish(false, "Board failed to initialize"); return;
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
        if (UBSettings::settings()->penColors(false).size() < 10) { finish(false,"Ten colour palette unavailable"); return; }
        auto window = UBApplication::mainWindow;
        auto graphAction = window->findChild<QAction*>("actionTeachingGraphDesk");
        if (!graphAction) { finish(false,"GraphDesk toolbar action unavailable"); return; }
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
                finish(report.value("ok").toBool(), report.value("ok").toBool()
                    ? "Startup, ink/highlight layering and pixels, triangle undo/redo, ten swatches, GraphDesk four-function export/restore passed"
                    : "GraphDesk check failed: " + result.toString());
            });
        });
    });
}
