// SPDX-License-Identifier: GPL-3.0-only
#include "UBMathFormulaTest.h"
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
        {"pythagoras",{"3","4"},0,"c ≈ 5"},
        {"pythagoras",{"5","4"},1,"a ≈ 3"},
        {"distance",{"1","2","4","6"},0,"d ≈ 5"},
        {"slope",{"1","2","3","6"},0,"m ≈ 2"},
        {"slope",{"1","2","1","6"},0,"line is vertical"},
        {"sine",{"10","30","60"},0,"b ≈ 17.321"},
        {"sine",{"7","30","10"},1,"both triangles are valid"},
        {"cosine-side",{"3","4","90"},0,"a ≈ 5"},
        {"cosine-angle",{"5","3","4"},0,"A ≈ 90°"},
        {"quotient",{"x^2"},0,"Q = 2x + h"},
        {"derivative",{"x^3-2x"},0,"f′(x) = 3x^2 − 2"}
    };
    const QDir output=QFileInfo(qEnvironmentVariable("TEACHING_SMOKE_REPORT")).dir();
    for(const auto &e:examples){UBMathFormula::State state;state.kind=e.kind;state.values=e.values;state.mode=e.mode;state.step=UBMathFormula::lastStep(state);const auto svg=UBMathFormula::render(state);UBMathFormula::State reopened;QSvgRenderer renderer(svg);const auto result=description(svg);
        if(!renderer.isValid()||!UBMathFormula::read(svg,&reopened)||reopened.kind!=state.kind||reopened.values!=state.values||!result.contains(e.expected))return fail("Formula regression "+e.kind+": "+result);
        QImage image(renderer.defaultSize(),QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter painter(&image);renderer.render(&painter);painter.end();image.save(output.filePath("formula-"+e.kind+QString::number(e.mode)+"-window.png"));
    }
    UBMathFormula::State unsupported;unsupported.kind="derivative";unsupported.values={"sin(x)"};if(UBMathFormula::error(unsupported).isEmpty())return fail("Unsupported function was accepted");
    UBMathFormula::State impossible;impossible.kind="cosine-angle";impossible.values={"1","2","4"};if(UBMathFormula::error(impossible).isEmpty())return fail("Impossible triangle was accepted");
    auto window=UBApplication::mainWindow;auto board=UBApplication::boardController;auto manager=UBPersistenceManager::persistenceManager();
    auto functions=window->findChild<QAction*>("actionTeachingFunctions");auto insert=window->findChild<QAction*>("insertFormula-quadratic");
    if(!functions||!functions->menu()||!insert||window->windowTitle()!="Mathscrap")return fail("Mathscrap Functions action or branding unavailable");
    auto doc=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Mathscrap formula input regression",true);board->setActiveDocumentScene(doc,0);auto scene=board->activeScene();
    insert->trigger();if(QApplication::activeModalWidget())return fail("Formula insertion opened a dialog");
    UBGraphicsSvgItem *item=nullptr;for(auto candidate:scene->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(candidate)){UBMathFormula::State s;if(UBMathFormula::read(svg->fileData(),&s)&&s.kind=="quadratic"){item=svg;break;}}
    if(!item||!item->formulaInputWidget())return fail("On-page formula inputs missing");
    auto fields=item->formulaInputWidget()->findChildren<QLineEdit*>();std::sort(fields.begin(),fields.end(),[](QLineEdit*a,QLineEdit*b){return a->objectName()<b->objectName();});if(fields.size()!=3)return fail("Expected three coefficient inputs");
    // Inputs must accept taps while the pen is active without laying down ink.
    UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Pen);
    auto view=board->controlView();const auto tap=view->mapFromScene(item->mapToScene(QPointF(118,76)));
    QMouseEvent down(QEvent::MouseButtonPress,QPointF(tap),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view->viewport(),&down);
    QMouseEvent up(QEvent::MouseButtonRelease,QPointF(tap),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(view->viewport(),&up);
    if(!fields[0]->hasFocus())return fail("Pen tap did not focus the coefficient box");
    window->actionSelector->trigger();
    type(fields[0],"1");type(fields[1],"6");type(fields[2],"7");QMetaObject::invokeMethod(fields[2],"editingFinished",Qt::DirectConnection);
    UBMathFormula::State state;if(!UBMathFormula::read(item->fileData(),&state)||state.values!=QStringList{"1","6","7"}||state.step!=1)return fail("Typing coefficients did not update substitution");
    const qreal beforeHeight=item->boundingRect().height();state.step=UBMathFormula::lastStep(state);item->setTeachingSvg(UBMathFormula::render(state));if(item->boundingRect().height()<=beforeHeight)return fail("Formula object did not grow with steps");
    type(fields[2],"-");if(!UBMathFormula::read(item->fileData(),&state)||state.step!=0||description(item->fileData()).contains("-1.586"))return fail("Incomplete new value retained old answers");
    type(fields[2],"6");QMetaObject::invokeMethod(fields[2],"editingFinished",Qt::DirectConnection);if(!UBMathFormula::read(item->fileData(),&state)||state.step!=1)return fail("Reused formula did not restart at substitution");
    const auto changed=item->fileData();UBApplication::undoStack->undo();if(!UBMathFormula::read(item->fileData(),&state)||state.values!=QStringList{"1","6","7"}||state.step!=5)return fail("Input undo did not restore the prior solution");UBApplication::undoStack->redo();if(item->fileData()!=changed)return fail("Input redo failed");
    board->persistCurrentScene(false,true);auto loaded=manager->loadDocumentScene(doc,0,false);bool found=false;for(auto candidate:loaded->items())if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(candidate)){UBMathFormula::State restored;if(UBMathFormula::read(svg->fileData(),&restored)&&restored.values==QStringList{"1","6","6"})found=svg->formulaInputWidget()!=nullptr;}if(!found)return fail("Formula inputs or edited state were lost on save/reopen");
    window->grab().save(output.filePath("mathscrap-inputs-window.png"));
    auto clean=manager->createNewDocument(UBPersistenceManager::myDocumentsName,"Native drawing checks",true);board->setActiveDocumentScene(clean,0);
    return true;
}
