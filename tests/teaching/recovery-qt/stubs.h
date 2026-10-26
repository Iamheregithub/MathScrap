// Test boundaries only: production history, store and idle-clock code is compiled unchanged.
#pragma once
#include <QtWidgets>
#include <memory>
#include <atomic>
namespace Fixture {
inline QString root;
inline int pageWrites=0,metadataWrites=0;
inline bool failPage=false;
inline std::atomic<bool> holdDrain{false};
inline QSemaphore enteredDrain,releaseDrain;
}
class UBDocumentProxy {
public:
    explicit UBDocumentProxy(const QString &path):mPath(path){}
    QString persistencePath()const{return mPath;}
    QString name()const{return "Recovery fixture";}
    QUuid uuid()const{return QUuid("{11111111-1111-1111-1111-111111111111}");}
private:QString mPath;
};
class UBGraphicsScene {
public:
    bool pressed=false,modified=true;
    QString text="original writing";
    bool isInputDevicePressed()const{return pressed;}
    bool isModified()const{return modified;}
    void saveWidgetSnapshots(){}
};
class UBBoardController {
public:
    std::shared_ptr<UBDocumentProxy> doc;
    std::shared_ptr<UBGraphicsScene> scene=std::make_shared<UBGraphicsScene>();
    auto selectedDocument()const{return doc;}
    auto activeScene()const{return scene;}
    int activeSceneIndex()const{return 0;}
};
class UBApplication { public: inline static UBBoardController *boardController=nullptr; };
class UBSettings {
public:
    static QString userDataDirectory(){return Fixture::root;}
    static QString userDocumentDirectory(){return Fixture::root+"/documents";}
};
class UBPersistenceManager {
public:
    static UBPersistenceManager *persistenceManager(){static UBPersistenceManager m;return &m;}
    void flushPendingWrites(){if(Fixture::holdDrain){Fixture::enteredDrain.release();Fixture::releaseDrain.acquire();}}
    QString generateUniqueDocumentPath(){return Fixture::root+"/restored-"+QUuid::createUuid().toString(QUuid::WithoutBraces);}
    std::shared_ptr<UBDocumentProxy> createDocumentFromDir(const QString &p,const QString &,const QString &,bool,bool,bool){return std::make_shared<UBDocumentProxy>(p);}
    inline static QString myDocumentsName="MyDocuments";
};
class UBSvgSubsetAdaptor {
public:
    static bool persistScene(std::shared_ptr<UBDocumentProxy> d,std::shared_ptr<UBGraphicsScene> s,int){
        ++Fixture::pageWrites;if(Fixture::failPage)return false;
        QDir().mkpath(d->persistencePath());QSaveFile f(d->persistencePath()+"/page000.svg");
        const auto bytes=s->text.toUtf8();return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit();
    }
};
class UBMetadataDcSubsetAdaptor {
public:
    static bool persist(std::shared_ptr<UBDocumentProxy> d){
        ++Fixture::metadataWrites;QSaveFile f(d->persistencePath()+"/metadata.rdf");
        return f.open(QIODevice::WriteOnly)&&f.write("metadata")==8&&f.commit();
    }
};
class UBFileSystemUtils {public:static QString digitFileFormat(const QString &,int){return "/page000.svg";}};
