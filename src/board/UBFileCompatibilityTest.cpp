// SPDX-License-Identifier: GPL-3.0-only
#include "UBFileCompatibilityTest.h"
#include <QtWidgets>
#include <QPdfWriter>
#include <QtXml>
#include "core/UBApplication.h"
#include "core/UBSettings.h"
#include "core/UBSetting.h"
#include "core/UBPersistenceManager.h"
#include "board/UBBoardController.h"
#include "board/UBDrawingController.h"
#include "domain/UBGraphicsScene.h"
#include "domain/UBGraphicsSvgItem.h"
#include "domain/UBGraphicsTextItem.h"
#include "domain/UBGraphicsPDFItem.h"
#include "domain/UBGraphicsStrokesGroup.h"
#include "domain/UBGraphicsPolygonItem.h"
#include "document/UBDocumentProxy.h"
#include "adaptors/UBExportDocument.h"
#include "adaptors/UBImportDocument.h"
#include "adaptors/UBImportPDF.h"
#include "adaptors/UBExportPDF.h"
#include "adaptors/UBMetadataDcSubsetAdaptor.h"
namespace {
QByteArray hashFile(const QString &path) {
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash h(QCryptographicHash::Sha256); if (!h.addData(&f)) return {};
    return h.result().toHex();
}
bool writeJson(const QString &path, const QJsonObject &object) {
    QSaveFile f(path); if (!f.open(QIODevice::WriteOnly)) return false;
    const auto bytes = QJsonDocument(object).toJson();
    return f.write(bytes) == bytes.size() && f.commit();
}
QJsonObject manifest(std::shared_ptr<UBDocumentProxy> doc, const QString &prefix) {
    QJsonArray pages;
    auto manager = UBPersistenceManager::persistenceManager();
    for (int i=0;i<doc->pageCount();++i) {
        auto scene=manager->loadDocumentScene(doc,i);
        QJsonArray svgs,texts,ink; int strokes=0, polygons=0;
        for (auto item:scene->items()) {
            if (auto svg=dynamic_cast<UBGraphicsSvgItem*>(item)) {
                QJsonObject o; o["uuid"]=svg->uuid().toString();
                o["sha256"]=QString(QCryptographicHash::hash(svg->fileData(),QCryptographicHash::Sha256).toHex());
                const auto t=svg->sceneTransform();o["dx"]=t.dx();o["dy"]=t.dy();o["m11"]=t.m11();o["m12"]=t.m12();o["m21"]=t.m21();o["m22"]=t.m22();
                svgs.append(o);
            }
            if (auto text=dynamic_cast<UBGraphicsTextItem*>(item)) texts.append(text->toPlainText());
            if (dynamic_cast<UBGraphicsStrokesGroup*>(item)) ++strokes;
            if (auto polygon=dynamic_cast<UBGraphicsPolygonItem*>(item)) {
                ++polygons; QJsonArray points;
                for(const auto &point:polygon->mapToScene(polygon->polygon())) {QJsonArray xy;xy.append(point.x());xy.append(point.y());points.append(xy);}
                QJsonObject stroke;stroke["points"]=points;stroke["color"]=polygon->color().name(QColor::HexArgb);ink.append(stroke);
            }
        }
        QJsonObject page;page["svg"]=svgs;page["texts"]=texts;page["strokeGroups"]=strokes;page["polygons"]=polygons;page["ink"]=ink;
        if (auto pdf=dynamic_cast<UBGraphicsPDFItem*>(scene->backgroundObject())) {
            page["pdfPage"]=pdf->pageNumber();
            page["pdfSha256"]=QString(hashFile(doc->persistencePath()+"/objects/"+pdf->fileUuid().toString()+".pdf"));
        }
        QImage image(1000,750,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);
        scene->clearSelection();QPainter painter(&image);scene->render(&painter,QRectF(0,0,1000,750),scene->normalizedSceneRect());painter.end();
        image.save(prefix+QString("-page-%1.png").arg(i+1));pages.append(page);
    }
    QJsonObject o;o["pageCount"]=doc->pageCount();o["pages"]=pages;o["documentPath"]=doc->persistencePath();return o;
}
bool saveAll(std::shared_ptr<UBDocumentProxy> doc) {
    auto manager=UBPersistenceManager::persistenceManager();
    // A unique queued name is a FIFO barrier for earlier asynchronous scene saves.
    manager->persistDocumentMetadata(doc);
    QElapsedTimer timer;timer.start();
    while(UBMetadataDcSubsetAdaptor::load(doc->persistencePath()).value(UBSettings::documentName)!=doc->metaData(UBSettings::documentName)) {
        if(timer.elapsed()>15000) return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents,20);QThread::msleep(10);
    }
    for(int i=0;i<doc->pageCount();++i) manager->persistDocumentScene(doc,manager->loadDocumentScene(doc,i),i,false,true);
    manager->persistDocumentMetadata(doc,true);return true;
}
void run() {
    const QDir root(qEnvironmentVariable("LESSONBOARD_COMPAT_DIR"));
    const QString mode=qEnvironmentVariable("LESSONBOARD_COMPAT_MODE");
    QString error;QJsonObject report;report["mode"]=mode;report["profile"]=UBSettings::userDataDirectory();
    auto fail=[&](const QString &message){report["passed"]=false;report["error"]=message;writeJson(root.filePath(mode+"-report.json"),report);qCritical()<<message;qApp->exit(24);};
    auto manager=UBPersistenceManager::persistenceManager();auto board=UBApplication::boardController;
    if(!board || !board->activeScene()){fail("Board unavailable");return;}
    std::shared_ptr<UBDocumentProxy> doc;
    if(mode=="original-create") {
        const QString pdfPath=root.filePath("original-background.pdf");
        { QPdfWriter pdf(pdfPath);pdf.setResolution(72);pdf.setPageSize(QPageSize(QSizeF(600,450),QPageSize::Point));pdf.setPageMargins(QMarginsF());
          QPainter painter(&pdf);for(int i=0;i<3;++i){if(i)pdf.newPage();painter.setPen(Qt::black);painter.drawText(QPoint(30,35),QString("ORIGINAL OPENBOARD PDF PAGE %1").arg(i+1));painter.fillRect(QRect(20,60,40,40),Qt::blue);}painter.end(); }
        doc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Compatibility original lesson",false);
        if(!doc){fail("Could not create original lesson");return;}
        const auto uuid=QUuid::createUuid();QDir().mkpath(doc->persistencePath()+"/objects");
        if(!QFile::copy(pdfPath,doc->persistencePath()+"/objects/"+uuid.toString()+".pdf")){fail("PDF copy failed");return;}
        UBImportPDF importer;const auto items=importer.import(uuid,pdfPath);
        if(items.size()!=3){fail("Original PDF import failed");return;}
        auto dc=UBDrawingController::drawingController();dc->setPenColor(false,Qt::black,0);dc->setMarkerColor(false,Qt::yellow,0);
        for(int i=0;i<3;++i){auto scene=manager->createDocumentSceneAt(doc,i,false);board->setActiveDocumentScene(doc,i);importer.placeImportedItemToScene(scene,items[i]);
            scene->addText(QString("Original editable text %1").arg(i+1),QPointF(-180,-100));
            for(int tool:{UBStylusTool::Pen,UBStylusTool::Marker}){dc->setStylusTool(tool);dc->setColorIndex(0);scene->inputDevicePress(QPointF(-100,0));scene->inputDeviceMove(QPointF(0,0));scene->inputDeviceMove(QPointF(100,0));scene->inputDeviceRelease();}
            dc->setStylusTool(UBStylusTool::Selector);
            scene->addSvg(QUrl(),QPointF(150,80),QByteArray("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"80\" height=\"60\"><ellipse cx=\"40\" cy=\"30\" rx=\"35\" ry=\"25\" fill=\"cyan\" stroke=\"blue\"/></svg>"));
        }
    } else {
        const QString input=root.filePath(mode=="lessonboard-edit"?"original.ubz":mode=="original-edit"?"lessonboard.ubz":"original-resaved.ubz");
        const auto before=hashFile(input);UBImportDocument importer;doc=importer.importFile(QFile(input),QString());
        if(!doc || before.isEmpty() || hashFile(input)!=before){fail("Import failed or changed source archive");return;}
        if(doc->pageCount()!=3){fail("Page count changed during import");return;}
        board->setActiveDocumentScene(doc,0);
        if(!writeJson(root.filePath(mode+"-import.json"),manifest(doc,root.filePath(mode+"-import")))){fail("Cannot save import manifest");return;}
        auto scene=board->activeScene();
        if(mode=="lessonboard-edit") {
#ifdef UB_TEACHING_FORK
            UBSettings::settings()->setValue("Teaching/DashedLines",true);
            auto dc=UBDrawingController::drawingController();dc->setStylusTool(UBStylusTool::Line);dc->setColorIndex(0);
            scene->inputDevicePress(QPointF(-180,70));scene->inputDeviceMove(QPointF(180,70));scene->inputDeviceRelease();
            dc->setStylusTool(UBStylusTool::Selector);UBSettings::settings()->setValue("Teaching/DashedLines",false);
#endif
            const QStringList geometry={"<rect x=\"10\" y=\"20\" width=\"160\" height=\"80\"/>","<rect x=\"20\" y=\"20\" width=\"100\" height=\"100\"/>","<circle cx=\"80\" cy=\"80\" r=\"60\"/>","<polygon points=\"10,130 75,5 150,125\"/>"};
            for(int i=0;i<4;++i){const auto data=QString("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"180\" height=\"160\"><metadata id=\"teaching-shape\">%1</metadata><g stroke=\"#800080\" fill=\"%2\" stroke-width=\"3\">%3</g></svg>").arg(i).arg(i%2?"none":"#33cc66").arg(geometry[i]).toUtf8();
                auto svg=scene->addSvg(QUrl(),QPointF(-210+i*140,130),data);if(!svg){fail("Cannot insert shape");return;}svg->setRotation(i*10);svg->setScale(0.7);
            }
            const QByteArray state=QByteArray("{\"plots\":[{\"expr\":\"x^2\"},{\"expr\":\"2x+1\"},{\"expr\":\"sin(x)\"},{\"expr\":\"-x+3\"}]}").toBase64();
            const QByteArray graph="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"200\" height=\"100\"><metadata id=\"graphdesk-project\">"+state+"</metadata><path d=\"M0 90L200 10 M0 50L200 50\" stroke=\"red\" stroke-dasharray=\"5 3\" fill=\"none\"/></svg>";
            if(!scene->addSvg(QUrl(),QPointF(150,-100),graph)){fail("Cannot insert graph");return;}
        } else if(mode=="original-edit") {
            scene->addText("Edited and saved in original OpenBoard",QPointF(-200,-160));
            bool moved=false;for(auto item:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item))if(svg->fileData().contains("graphdesk-project")){svg->moveBy(10,5);svg->setRotation(svg->rotation()+15);svg->setScale(svg->scale()*1.1);moved=true;}
            if(!moved){fail("Original OpenBoard did not load the graph as an editable SVG object");return;}
        } else if(mode=="lessonboard-reopen") {
#ifdef UB_TEACHING_FORK
            int shapes=0,graphs=0;for(auto item:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item)) {
                if(svg->isTeachingShape())++shapes;
                if(svg->fileData().contains("graphdesk-project")){QDomDocument xml;xml.setContent(svg->fileData());const auto nodes=xml.elementsByTagName("metadata");for(int j=0;j<nodes.size();++j)if(nodes.at(j).toElement().attribute("id")=="graphdesk-project")if(QJsonDocument::fromJson(QByteArray::fromBase64(nodes.at(j).toElement().text().toUtf8())).object().value("plots").toArray().size()==4)++graphs;}
            }
            if(shapes!=4 || graphs!=1){fail("Shape identity or GraphDesk project metadata did not survive original OpenBoard");return;}
#endif
        } else {fail("Unknown compatibility mode");return;}
    }
    doc->setMetaData(UBSettings::documentName, QString("Compatibility %1 lesson").arg(mode));
    if(!saveAll(doc)){fail("Pending document saves did not finish");return;}
    const QString name=mode=="original-create"?"original":mode=="lessonboard-edit"?"lessonboard":mode=="original-edit"?"original-resaved":"lessonboard-reopened";
    UBExportDocument exporter;if(!exporter.persistsDocument(doc,root.filePath(name+".ubz"))){fail("UBZ export failed");return;}
    if(!writeJson(root.filePath(mode+"-saved.json"),manifest(doc,root.filePath(mode+"-saved")))){fail("Cannot save output manifest");return;}
    UBExportPDF pdf;if(!pdf.persistsDocument(doc,root.filePath(name+".pdf"))){fail("PDF export failed after import");return;}
    report["passed"]=true;report["archive"]=name+".ubz";report["documentPath"]=doc->persistencePath();
    writeJson(root.filePath(mode+"-report.json"),report);qApp->exit(0);
}
}
void scheduleFileCompatibilityTest(QObject *context) {
    QTimer::singleShot(120000,context,[]{qCritical("File compatibility test timed out");qApp->exit(25);});
    QTimer::singleShot(7000,context,run);
}
