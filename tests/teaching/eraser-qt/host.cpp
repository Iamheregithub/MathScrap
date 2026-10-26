// Qt input host for the production pen-tray methods. Canvas/model are stubs.
#include <QtWidgets>
#include <memory>
#define private public
#include "UBPaperInk.h"
#undef private
struct UBStylusTool {enum {Pen,Marker,Eraser,Line,Selector,ZoomIn,ZoomOut};};
class UBSettings {public:QMap<QString,QVariant> data;static UBSettings *settings(){static UBSettings s;return &s;}QVariant value(QString k,QVariant d={}){return data.value(k,d);}void setValue(QString k,QVariant v){data[k]=v;}void save(){}};
class UBDrawingController {public:int tool=UBStylusTool::Pen;static UBDrawingController *drawingController(){static UBDrawingController d;return &d;}int stylusTool(){return tool;}void setLineTool(bool){tool=UBStylusTool::Line;}};
class UBGraphicsScene {public:void cancelCircleErase(){}};
struct Palettes {QWidget left,right;QWidget *leftPalette(){return &left;}QWidget *rightPalette(){return &right;}};
struct Board {QWidget view;Palettes palettes;std::shared_ptr<UBGraphicsScene> scene=std::make_shared<UBGraphicsScene>();int erased=0;QWidget *controlView(){return &view;}Palettes *paletteManager(){return &palettes;}auto activeScene(){return scene;}void clearSceneAnnotation(){++erased;}};
struct UBApplication {static Board *boardController;};Board *UBApplication::boardController=nullptr;
class UBStylusPalette:public QWidget {Q_OBJECT};
class UBZoomPalette:public QWidget {Q_OBJECT};
class UBMainWindow:public QMainWindow {public:QAction *actionSelector,*actionEraser,*actionZoomIn,*actionZoomOut;QToolBar *boardToolBar;
 UBMainWindow(){boardToolBar=new QToolBar(this);auto make=[this](QString name,int mode){auto a=new QAction(name,this);a->setObjectName(name);a->setCheckable(true);connect(a,&QAction::triggered,this,[mode]{UBDrawingController::drawingController()->tool=mode;});return a;};actionSelector=make("actionSelector",UBStylusTool::Selector);actionEraser=make("actionEraser",UBStylusTool::Eraser);actionZoomIn=make("actionZoomIn",UBStylusTool::ZoomIn);actionZoomOut=make("actionZoomOut",UBStylusTool::ZoomOut);}
};
QIcon inkIcon(const QString &){return QIcon();}
UBPaperInk::UBPaperInk(UBMainWindow *w):QObject(w),mWindow(w){mDock=new QFrame(w);mDock->setGeometry(10,10,300,100);mPenPanel=new QFrame(w);mPenPanel->setGeometry(10,200,200,100);mHeader=new QToolBar(w);mPages=new QFrame(w);mHoverTimer=new QTimer(this);mCloseTimer=new QTimer(this);mEraseAllTimer=new QTimer(this);mEraseAllTimer->setSingleShot(true);qApp->installEventFilter(this);}
void UBPaperInk::openTray(QToolButton *b){if(mEraseAllMode&&b->property("paperSource")=="actionEraser")return;mTrayOpen=true;mPenPanel->show();}
void UBPaperInk::closeTray(){mTrayOpen=false;mHoverTimer->stop();mCloseTimer->stop();mPenPanel->hide();}
void UBPaperInk::scheduleLayout(){}
int UBPaperInk::trayTool()const{return UBStylusTool::Eraser;}
#include "production-methods.inc"
int main(int argc,char **argv){QApplication app(argc,argv);app.setDoubleClickInterval(80);Board board;UBApplication::boardController=&board;UBMainWindow w;w.resize(600,400);UBPaperInk p(&w);auto b=p.button(w.actionEraser,"Eraser",p.mDock);b->setProperty("paperTray",true);b->setProperty("paperSource","actionEraser");b->setGeometry(10,10,120,60);w.show();p.mDock->show();b->show();p.mPenPanel->hide();app.processEvents();int checks=0;
 auto check=[&](bool good,const char *message){++checks;if(!good){qCritical()<<"FAIL:"<<message;exit(1);}};
 auto wait=[](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
 auto mouse=[&](QEvent::Type type){QMouseEvent e(type,b->rect().center(),b->mapToGlobal(b->rect().center()),Qt::LeftButton,type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(b,&e);};
 auto click=[&]{mouse(QEvent::MouseButtonPress);mouse(QEvent::MouseButtonRelease);};
 auto doubleClick=[&]{click();mouse(QEvent::MouseButtonDblClick);mouse(QEvent::MouseButtonRelease);};
 auto tap=[&](bool canvas){const QPoint global=b->mapToGlobal(b->rect().center());QWidget *receiver=canvas?&board.view:b;
 for(auto type:{QEvent::TabletPress,QEvent::TabletRelease}){QTabletEvent e(type,receiver->mapFromGlobal(global),global,QTabletEvent::Stylus,QTabletEvent::Pen,type==QEvent::TabletRelease?0:.8,0,0,0,0,0,Qt::NoModifier,1,Qt::LeftButton,type==QEvent::TabletRelease?Qt::NoButton:Qt::LeftButton);QApplication::sendEvent(receiver,&e);}app.processEvents();};
 check(!p.mEraseAllMode&&b->text()=="Eraser","starts as regular eraser");
 doubleClick();check(p.mEraseAllMode&&b->text()=="Erase All","mouse double-click changes mode and label");wait(130);check(board.erased==0,"entering Erase All does not clear");check(!p.mTrayOpen,"Erase All has no size tray");
 click();wait(130);check(board.erased==1,"single mouse click invokes clear once");
 doubleClick();wait(130);check(!p.mEraseAllMode&&board.erased==1,"mouse double-click back cancels clear");
 tap(false);tap(false);wait(130);check(p.mEraseAllMode&&board.erased==1,"direct stylus double-tap changes mode without clearing");
 tap(false);wait(130);check(board.erased==2,"direct stylus single tap clears exactly once");
 tap(false);tap(false);wait(130);check(!p.mEraseAllMode&&board.erased==2,"direct stylus double-tap back cancels clear");
 tap(true);tap(true);wait(130);check(p.mEraseAllMode&&board.erased==2,"canvas-addressed stylus double-tap enters mode");
 tap(true);wait(130);check(board.erased==3,"canvas-addressed stylus single tap clears once");
 tap(true);tap(true);wait(130);check(!p.mEraseAllMode&&board.erased==3,"canvas-addressed stylus double-tap back cancels clear");
 doubleClick();click();board.scene=std::make_shared<UBGraphicsScene>();wait(130);check(board.erased==3,"pending clear does not erase another page");
 click();QKeyEvent esc(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(b,&esc);wait(130);check(board.erased==3,"Escape cancels pending clear");
 click();QMouseEvent outside(QEvent::MouseButtonPress,QPointF(1,1),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&w,&outside);wait(130);check(board.erased==3,"another UI press cancels pending clear");
 doubleClick();p.mLastTabletTap=nullptr;tap(true);check(UBDrawingController::drawingController()->stylusTool()==UBStylusTool::Eraser&&p.mTrayOpen,"regular stylus eraser and size tray still work");
 qInfo()<<"PASS:"<<checks<<"production mouse/tablet control checks (stub canvas)";return 0;}
#include "host.moc"
