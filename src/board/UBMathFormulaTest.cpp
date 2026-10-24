// SPDX-License-Identifier: GPL-3.0-only
#include "UBMathFormulaTest.h"
#include "UBPaperInk.h"
#include "core/UBSettings.h"
#include <QtWidgets>
#include <algorithm>
#include <QSvgRenderer>
#include <QDomDocument>
#include "domain/UBMathFormula.h"
#include "domain/UBGraphicsSvgItem.h"
#include "domain/UBGraphicsScene.h"
#include "core/UBApplication.h"
#include "core/UBPersistenceManager.h"
#include "gui/UBMainWindow.h"
#include "board/UBBoardController.h"
#include "board/UBBoardView.h"
#include "board/UBDrawingController.h"
#include "document/UBDocumentProxy.h"
namespace {
QString description(const QByteArray &data){QDomDocument doc;doc.setContent(data);return doc.elementsByTagName("desc").at(0).toElement().text();}
void type(QLineEdit *field,const QString &value){field->setFocus();field->selectAll();QKeyEvent press(QEvent::KeyPress,0,Qt::NoModifier,value);QApplication::sendEvent(field,&press);QKeyEvent release(QEvent::KeyRelease,0,Qt::NoModifier,value);QApplication::sendEvent(field,&release);}
}
bool checkMathFormulae(QString *error){
    auto fail=[error](const QString &message){*error=message;return false;};
    struct Example{QString kind;QStringList values;int mode;QString expected;};
    const QList<Example> examples={
        {"quadratic",{"1","6","7"},0,"-1.586"},
        {"discriminant",{"1","2","1"},0,"one repeated real root"},
        {"pythagoras",{"3","4"},0,"5 = c"},
        {"pythagoras",{"5","4"},1,"a = 3"},
        {"distance",{"1","2","4","6"},0,"d = 5"},
        {"slope",{"1","2","3","6"},0,"m ≈ 2"},
        {"slope",{"1","2","1","6"},0,"line is vertical"},
        {"slope",{"0","0","0.0000000000001","1"},0,"m ≈ 10000000000000"},
        {"slope",{"1","2","1","2"},0,"two points coincide"},
        {"sine",{"10","30","60"},0,"b ≈ 17.321"},
        {"sine",{"7","30","10"},1,"both triangles are valid"},
        {"cosine-side",{"3","4","90"},0,"a ≈ 5"},
        {"cosine-side",{"1000000","1000000","0.000001"},0,"a ≈ 0.017"},
        {"cosine-angle",{"5","3","4"},0,"A ≈ 90°"},
        {"cosine-angle",{"1e-200","1e-200","1e-200"},0,"A ≈ 60°"},
        {"quotient",{"x^2"},0,"Q = 2x + h"},
        {"derivative",{"x^3-2x"},0,"f′(x) = 3x^2 − 2"},
        {"derivative",{"0.00000000001x^2"},0,"f′(x) = 2e-11x"},
        {"derivative",{"0.99999999999x"},0,"f′(x) = 0.99999999999"},
        {"derivative",{"7"},0,"f′(x) = 0"}
    };
    const QDir output=QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir();
    for(const auto &e:examples){UBMathFormula::State state;state.kind=e.kind;state.values=e.values;state.mode=e.mode;state.step=UBMathFormula::lastStep(state);const auto svg=UBMathFormula::render(state);UBMathFormula::State reopened;QSvgRenderer renderer(svg);const auto result=description(svg);
        if(!renderer.isValid()||!UBMathFormula::read(svg,&reopened)||reopened.kind!=state.kind||reopened.values!=state.values||!result.contains(e.expected)&&!result.contains(QString(e.expected).replace(" ≈ "," = ")))return fail("Formula regression "+e.kind+": "+result);
        QImage image(renderer.defaultSize(),QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter painter(&image);renderer.render(&painter);painter.end();image.save(output.filePath("formula-"+e.kind+QString::number(e.mode)+"-window.png"));
    }
    UBMathFormula::State detailed;detailed.kind="pythagoras";detailed.values={"1","3"};detailed.step=UBMathFormula::lastStep(detailed);
    const auto worked=description(UBMathFormula::render(detailed));int last=-1;
    for(const auto &line:QStringList{"1² + 3² = c²","1 + 9 = c²","10 = c²","sqrt(10) = c"}){
        const int position=worked.indexOf(line,last+1);if(position<0)return fail("Pythagorean working skips or reorders "+line);last=position;
    }
    detailed.step=1;detailed.tapToReveal=true;
    const auto revealSvg=UBMathFormula::render(detailed);UBMathFormula::State revealRestored;
    if(!UBMathFormula::read(revealSvg,&revealRestored)||!revealRestored.tapToReveal||revealRestored.step!=1)return fail("Formula reveal mode or step did not survive SVG round-trip");
    QDomDocument revealDoc;revealDoc.setContent(revealSvg);const auto box=revealDoc.documentElement().attribute("viewBox").split(QRegularExpression("[ ,]+"),Qt::SkipEmptyParts);
    if(box.size()!=4)return fail("Formula reveal SVG has no viewBox");
    const qreal cardWidth=box[2].toDouble(),cardHeight=box[3].toDouble();
    for(int i=0;i<4;++i)if(UBMathFormula::stepAt(revealSvg,cardWidth-548+i*132+61,cardHeight-36)!=i)return fail("Formula reveal control hit area is misplaced");
    if(UBMathFormula::stepAt(revealSvg,cardWidth-125,33)!=4||UBMathFormula::stepAt(revealSvg,cardWidth/2,cardHeight-95)!=5)return fail("Tap-to-reveal control or card hit area is misplaced");
    detailed.tapToReveal=false;
    if(UBMathFormula::stepAt(UBMathFormula::render(detailed),cardWidth/2,cardHeight-95)!=-1)return fail("Ordinary formula selection advances a reveal step");
    UBMathFormula::State unsupported;unsupported.kind="derivative";unsupported.values={"sin(x)"};if(UBMathFormula::error(unsupported).isEmpty())return fail("Unsupported function was accepted");
    unsupported.values={"2*"};if(UBMathFormula::error(unsupported).isEmpty())return fail("Incomplete multiplication was accepted as a constant");
    UBMathFormula::State overflow;overflow.kind="slope";overflow.values={"0","0","1e-320","1"};if(UBMathFormula::error(overflow).isEmpty())return fail("Unrepresentable slope was accepted");
    overflow.kind="sine";overflow.values={"1000000","1e-310","60"};if(UBMathFormula::error(overflow).isEmpty())return fail("Unrepresentable sine-law side was accepted");
    UBMathFormula::State impossible;impossible.kind="cosine-angle";impossible.values={"1","2","4"};if(UBMathFormula::error(impossible).isEmpty())return fail("Impossible triangle was accepted");
    auto window=UBApplication::mainWindow;auto board=UBApplication::boardController;auto manager=UBPersistenceManager::persistenceManager();
    auto functions=window->findChild<QAction*>("actionTeachingFunctions");auto insert=window->findChild<QAction*>("insertFormula-quadratic");
    if(!functions||!functions->menu()||!insert||window->windowTitle()!="MathScrap")return fail("MathScrap Functions action or branding unavailable");
    auto doc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"MathScrap formula input regression",true);board->setActiveDocumentScene(doc,0);auto scene=board->activeScene();
    insert->trigger();if(QApplication::activeModalWidget())return fail("Formula insertion opened a dialog");
    UBGraphicsSvgItem *item=nullptr;for(auto candidate:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(candidate)){UBMathFormula::State s;if(UBMathFormula::read(svg->fileData(),&s)&&s.kind=="quadratic"){item=svg;break;}}
    if(!item||!item->formulaInputWidget())return fail("On-page formula inputs missing");
    auto fitsCanvas=[board,item]{
        auto view=board->controlView();
        const auto bounds=view->viewportTransform().mapRect(item->sceneBoundingRect());
        QRectF available=view->viewport()->rect();
        if(auto dock=view->findChild<QWidget*>("paperDock"))if(dock->isVisible())available.setBottom(dock->mapTo(view->viewport(),QPoint(0,0)).y());
        return available.contains(bounds);
    };
    if(!fitsCanvas())return fail("Inserted formula is outside the visible canvas or behind the dock");
    auto fields=item->formulaInputWidget()->findChildren<QLineEdit*>();std::sort(fields.begin(),fields.end(),[](QLineEdit*a,QLineEdit*b){return a->objectName()<b->objectName();});if(fields.size()!=3)return fail("Expected three coefficient inputs");
    // Inputs must accept taps while the pen is active without laying down ink.
    UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Pen);
    auto view=board->controlView();const auto tap=view->mapFromScene(item->mapToScene(QPointF(118,76)));
    QMouseEvent down(QEvent::MouseButtonPress,QPointF(tap),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view->viewport(),&down);
    QMouseEvent up(QEvent::MouseButtonRelease,QPointF(tap),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(view->viewport(),&up);
    if(!fields[0]->hasFocus()){
        window->grab().save(output.filePath("formula-focus-failure-window.png"));
        auto focus=QApplication::focusWidget();
        return fail(QString("Pen tap did not focus the coefficient box (active=%1, focus=%2, tap=%3,%4)").arg(window->isActiveWindow()).arg(focus?focus->objectName():"none").arg(tap.x()).arg(tap.y()));
    }
    window->actionSelector->trigger();
    type(fields[0],"1");type(fields[1],"6");type(fields[2],"7");QMetaObject::invokeMethod(fields[2],"editingFinished",Qt::DirectConnection);
    UBMathFormula::State state;if(!UBMathFormula::read(item->fileData(),&state)||state.values!=QStringList{"1","6","7"}||state.step!=1)return fail("Typing coefficients did not update substitution");
    const qreal beforeHeight=item->boundingRect().height();state.step=UBMathFormula::lastStep(state);item->setTeachingSvg(UBMathFormula::render(state));if(item->boundingRect().height()<=beforeHeight)return fail("Formula object did not grow with steps");
    if(!fitsCanvas())return fail("Expanded formula is outside the visible canvas or behind the dock");
    type(fields[2],"-");if(!UBMathFormula::read(item->fileData(),&state)||state.step!=0||description(item->fileData()).contains("-1.586"))return fail("Incomplete new value retained old answers");
    type(fields[2],"6");QMetaObject::invokeMethod(fields[2],"editingFinished",Qt::DirectConnection);if(!UBMathFormula::read(item->fileData(),&state)||state.step!=1)return fail("Reused formula did not restart at substitution");
    const auto changed=item->fileData();UBApplication::undoStack->undo();if(!UBMathFormula::read(item->fileData(),&state)||state.values!=QStringList{"1","6","7"}||state.step!=5)return fail("Input undo did not restore the prior solution");UBApplication::undoStack->redo();if(item->fileData()!=changed||!UBMathFormula::read(item->fileData(),&state))return fail("Input redo failed");
    board->persistCurrentScene(false,true);auto loaded=manager->loadDocumentScene(doc,0,false);bool found=false;for(auto candidate:loaded->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(candidate)){UBMathFormula::State restored;if(UBMathFormula::read(svg->fileData(),&restored)&&restored.values==QStringList{"1","6","6"})found=svg->formulaInputWidget()!=nullptr;}if(!found)return fail("Formula inputs or edited state were lost on save/reopen");
    state.step=0;state.tapToReveal=true;item->setTeachingSvg(UBMathFormula::render(state));
    const auto tapStep=view->mapFromScene(item->mapToScene(QPointF(100,UBMathFormula::inputRect(state).bottom()+45)));
    QMouseEvent revealDown(QEvent::MouseButtonPress,QPointF(tapStep),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view->viewport(),&revealDown);
    QMouseEvent revealUp(QEvent::MouseButtonRelease,QPointF(tapStep),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(view->viewport(),&revealUp);
    if(!UBMathFormula::read(item->fileData(),&state)||!state.tapToReveal||state.step!=1){
        window->grab().save(output.filePath("formula-tap-failure-window.png"));
        return fail(QString("Tapping formula card did not reveal exactly one step (step=%1, hit=%2, point=%3,%4, active=%5)")
                    .arg(state.step).arg(UBMathFormula::stepAt(item->fileData(),100,UBMathFormula::inputRect(state).bottom()+45))
                    .arg(tapStep.x()).arg(tapStep.y()).arg(window->isActiveWindow()));
    }
    UBApplication::undoStack->undo();if(!UBMathFormula::read(item->fileData(),&state)||state.step!=0||!state.tapToReveal)return fail("Tap reveal did not undo");
    UBApplication::undoStack->redo();if(!UBMathFormula::read(item->fileData(),&state)||state.step!=1)return fail("Tap reveal did not redo");
    board->persistCurrentScene(false,true);loaded=manager->loadDocumentScene(doc,0,false);found=false;
    for(auto candidate:loaded->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(candidate)){UBMathFormula::State restored;if(UBMathFormula::read(svg->fileData(),&restored)&&restored.values==QStringList{"1","6","6"})found=restored.step==1&&restored.tapToReveal;}
    if(!found)return fail("Formula reveal mode or current step was lost on save/reopen");
    // Exercise the actual card controls through the Windows view, with Pages
    // both open and closed and after changing zoom. Merely testing SVG hit
    // coordinates misses the board view's selection-event routing.
    auto revealState=[item]{UBMathFormula::State s;UBMathFormula::read(item->fileData(),&s);return s;};
    auto cardSize=[item]{QDomDocument d;d.setContent(item->fileData());const auto v=d.documentElement().attribute("viewBox").split(QRegularExpression("[ ,]+"),Qt::SkipEmptyParts);return QSizeF(v[2].toDouble(),v[3].toDouble());};
    auto clickCard=[&](const QPointF &local){const auto pos=view->mapFromScene(item->mapToScene(local));QMouseEvent down(QEvent::MouseButtonPress,QPointF(pos),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view->viewport(),&down);QMouseEvent up(QEvent::MouseButtonRelease,QPointF(pos),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(view->viewport(),&up);QApplication::processEvents();};
    auto control=[&](int index){const auto size=cardSize();clickCard(QPointF(size.width()-548+index*132+61,size.height()-36));};
    auto body=[&]{clickCard(QPointF(100,UBMathFormula::inputRect(revealState()).bottom()+45));};
    auto expectStep=[&](int step,const QString &name){return revealState().step==step?true:fail(QString("%1: expected step %2, got %3").arg(name).arg(step).arg(revealState().step));};
    control(1);if(!expectStep(2,"Next step"))return false;
    control(0);if(!expectStep(1,"Previous"))return false;
    control(2);if(!expectStep(0,"Start over"))return false;
    const qreal startingScale=item->scale();
    control(3);if(!expectStep(UBMathFormula::lastStep(revealState()),"Reveal all"))return false;
    control(2);if(!expectStep(0,"Start over after Reveal all"))return false;
    if(item->scale()+0.001<startingScale)return fail("Restart left the formula card shrunk after Reveal all");
    body();if(!expectStep(1,"Tap to reveal"))return false;
    clickCard(QPointF(cardSize().width()-125,33));if(revealState().tapToReveal)return fail("Tap-to-reveal toggle did not turn off");
    body();if(!expectStep(1,"Tap mode off"))return false;
    clickCard(QPointF(cardSize().width()-125,33));if(!revealState().tapToReveal)return fail("Tap-to-reveal toggle did not turn back on");
    QToolButton *pageToggle=nullptr;
    for(auto candidate:window->findChildren<QToolButton*>())if(candidate->toolTip()==QObject::tr("Show or hide pages")){pageToggle=candidate;break;}
    if(!pageToggle)return fail("Pages toggle missing for reveal test");
    const bool pagesBefore=pageToggle->isChecked();pageToggle->click();QApplication::processEvents();
    board->zoomIn();board->zoomIn();const qreal zoomed=board->currentZoom();
    body();if(!expectStep(2,"Tap after Pages and zoom change")||!qFuzzyCompare(board->currentZoom(),zoomed))return false;
    control(0);if(!expectStep(1,"Previous after Pages and zoom change"))return false;
    board->zoomRestore();pageToggle->click();QApplication::processEvents();if(pageToggle->isChecked()!=pagesBefore)return fail("Pages toggle did not restore after reveal test");
    window->grab().save(output.filePath("formula-reveal-controls-window.png"));
    window->grab().save(output.filePath("mathscrap-inputs-window.png"));
    window->findChild<QAction*>("insertFormula-distance")->trigger();
    UBGraphicsSvgItem *pointItem=nullptr;for(auto candidate:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(candidate)){UBMathFormula::State saved;if(UBMathFormula::read(svg->fileData(),&saved)&&saved.kind=="distance")pointItem=svg;}
    if(!pointItem||!description(pointItem->fileData()).contains("A(x₁, y₁); B(x₂, y₂)"))return fail("Coordinate-pair labels are missing");
    auto pairFields=pointItem->formulaInputWidget()->findChildren<QLineEdit*>();
    std::sort(pairFields.begin(),pairFields.end(),[](QLineEdit*a,QLineEdit*b){return a->objectName()<b->objectName();});
    if(pairFields.size()!=4||pairFields[0]->parentWidget()->y()!=pairFields[1]->parentWidget()->y()||pairFields[2]->parentWidget()->y()<=pairFields[0]->parentWidget()->y())return fail("Point coordinates are not grouped into two rows");
    scene->clearSelection();QApplication::processEvents();window->grab().save(output.filePath("mathscrap-points-window.png"));

    // Repeatedly leave Zoom and select both function cards. Check the view and
    // the object's scene geometry independently to distinguish the two symptoms.
    auto paper=window->findChild<UBPaperInk*>();auto settings=UBSettings::settings();
    const auto previousZoomVisibility=settings->value("Teaching/Bottom/actionZoomIn",false);
    const auto previousDirection=settings->value("Teaching/ZoomOutTool",false);
    settings->setValue("Teaching/Bottom/actionZoomIn",true);paper->rebuild();QApplication::processEvents();
    auto zoomButton=window->findChild<QToolButton*>("paper-actionZoomIn");
    if(!zoomButton)return fail("Zoom button missing from formula regression");
    for(const auto &kind:QStringList{"derivative","quotient"}){
        auto regressionDoc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Formula zoom regression "+kind,true);
        board->setActiveDocumentScene(regressionDoc,0);board->zoomRestore();
        window->findChild<QAction*>("insertFormula-"+kind)->trigger();auto regressionScene=board->activeScene();
        UBGraphicsSvgItem *formula=nullptr;for(auto candidate:regressionScene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(candidate)){UBMathFormula::State saved;if(UBMathFormula::read(svg->fileData(),&saved)&&saved.kind==kind)formula=svg;}
        if(!formula)return fail("Function card missing for "+kind);
        auto field=formula->formulaInputWidget()->findChild<QLineEdit*>("formulaValue0");
        type(field,"x^2+6x+7");QMetaObject::invokeMethod(field,"editingFinished",Qt::DirectConnection);field->clearFocus();
        const auto bounds=formula->sceneBoundingRect();const qreal initialZoom=board->currentZoom();
        for(int cycle=0;cycle<6;++cycle){
            settings->setValue("Teaching/ZoomOutTool",cycle%2==0);window->actionSelector->trigger();zoomButton->click();
            if(cycle%2==0)zoomButton->click();else window->actionSelector->trigger();
            if(zoomButton->isChecked()||UBDrawingController::drawingController()->stylusTool()!=UBStylusTool::Selector)return fail("Zoom stayed active after deselection for "+kind);
            regressionScene->clearSelection();const auto pos=view->mapFromScene(formula->mapToScene(QPointF(70,30)));
            QMouseEvent press(QEvent::MouseButtonPress,QPointF(pos),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view->viewport(),&press);
            QMouseEvent release(QEvent::MouseButtonRelease,QPointF(pos),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(view->viewport(),&release);QApplication::processEvents();
            const auto size=formula->sceneBoundingRect().size();
            if(!qFuzzyCompare(board->currentZoom(),initialZoom)||qAbs(size.width()-bounds.width())>0.001||qAbs(size.height()-bounds.height())>0.001)return fail("Selecting "+kind+" changed page zoom or formula size");
        }
        // Focus changes and an undo command's initial redo can reapply the same
        // SVG. It must remain unchanged even when the user has zoomed in.
        board->zoomIn();board->zoomIn();const auto zoomedBounds=formula->sceneBoundingRect();const qreal zoomedPage=board->currentZoom();
        const auto unchanged=formula->fileData();for(int i=0;i<6;++i)formula->setTeachingSvg(unchanged);
        if(formula->sceneBoundingRect()!=zoomedBounds||!qFuzzyCompare(board->currentZoom(),zoomedPage))return fail("Reapplying unchanged "+kind+" resized the object or zoomed the page");
        board->zoomRestore();
    }
    window->setProperty("formulaZoomRegressionChecked",true);
    window->grab().save(output.filePath("mathscrap-zoom-regression-window.png"));
    settings->setValue("Teaching/Bottom/actionZoomIn",previousZoomVisibility);settings->setValue("Teaching/ZoomOutTool",previousDirection);paper->rebuild();window->actionPen->trigger();
    auto clean=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Native drawing checks",true);board->setActiveDocumentScene(clean,0);
    return true;
}
