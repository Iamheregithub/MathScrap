// SPDX-License-Identifier: GPL-3.0-only
#include <QtWidgets>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebChannel>
class Preferences:public QObject{
 Q_OBJECT
 public:QMap<QString,QString> saved;
 public slots:QString preference(const QString &key,const QString &def){return saved.value(key,def);}void setPreference(const QString &key,const QString &value){saved[key]=value;}
};
void settle(int ms){QEventLoop wait;QTimer::singleShot(ms,&wait,&QEventLoop::quit);wait.exec();}
QVariant js(QWebEngineView *web,QString script){QVariant result;QEventLoop wait;bool done=false;web->page()->runJavaScript(script,[&](const QVariant&v){result=v;done=true;wait.quit();});QTimer::singleShot(5000,&wait,&QEventLoop::quit);if(!done)wait.exec();return result;}
int main(int argc,char**argv){QApplication app(argc,argv);if(argc<3){qWarning()<<"Usage: calculator-qt-test PATH_TO_WIDGET_INDEX.HTML REPORT_DIRECTORY";return 20;}Preferences prefs;QGraphicsScene scene;QGraphicsView view(&scene);auto web=new QWebEngineView;auto proxy=scene.addWidget(web);web->setFixedSize(680,660);view.resize(740,720);view.show();auto channel=new QWebChannel(web);channel->registerObject("sankore",&prefs);web->page()->setWebChannel(channel);
 QFile source(":/qtwebchannel/qwebchannel.js");source.open(QFile::ReadOnly);QString boot=source.readAll();boot+="\nnew QWebChannel(qt.webChannelTransport,function(c){window.sankore=c.objects.sankore;window.sankore.async={preference:function(k,d){return new Promise(function(r){window.sankore.preference(k,d,r);});}};});";
 QWebEngineScript injection;injection.setInjectionPoint(QWebEngineScript::DocumentCreation);injection.setWorldId(QWebEngineScript::MainWorld);injection.setSourceCode(boot);web->page()->scripts().insert(injection);
 QUrl url=QUrl::fromLocalFile(argv[1]);web->load(url);int checks=0;
 auto expect=[&](QString code,QString expected){for(int i=0;i<100;i++){if(js(web,code).toString()==expected){checks++;return true;}settle(50);}qWarning()<<"FAIL"<<code<<js(web,code)<<"expected"<<expected;return false;};
 if(!expect("typeof mathscrapCalculator","object"))return 1;
 js(web,"mathscrapCalculator.expression.typedText('sqrt12')");if(!expect("document.getElementById('decimal').textContent","≈ 3.46410161514"))return 2;
 if(!expect("document.getElementById('exact').textContent.indexOf('3')>=0?'yes':'no'","yes"))return 3;
 js(web,"mathscrapCalculator.restore({latex:'f(x)=x^2+3x-1',x:'2',angle:'degrees'})");if(!expect("document.getElementById('decimal').textContent","≈ 9"))return 4;
 if(!prefs.saved.value("functionCalculatorState").contains("f(x)="))return 5;checks++;
 web->reload();if(!expect("document.getElementById('decimal')&&document.getElementById('decimal').textContent","≈ 9"))return 6;
 js(web,"mathscrapCalculator.restore({latex:'',x:'',angle:'degrees'})");settle(200);
 auto rect=js(web,"(function(){var r=document.querySelector('button[data-type=\"7\"]').getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()").toMap();
 QPoint local=view.mapFromScene(proxy->mapToScene(QPointF(rect["x"].toDouble(),rect["y"].toDouble())));QPoint global=view.viewport()->mapToGlobal(local);
 QMouseEvent press(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(view.viewport(),&press);QMouseEvent release(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(view.viewport(),&release);
 if(!expect("document.getElementById('decimal').textContent","≈ 7"))return 7;
 view.grab().save(QString(argv[2])+"/qt-calculator-window.png");qInfo()<<"Qt WebEngine calculator:"<<checks<<"checks passed";return 0;
}
#include "main.moc"
