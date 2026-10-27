/*
 * Copyright (C) 2015-2022 Département de l'Instruction Publique (DIP-SEM)
 *
 * Copyright (C) 2013 Open Education Foundation
 *
 * Copyright (C) 2010-2013 Groupement d'Intérêt Public pour
 * l'Education Numérique en Afrique (GIP ENA)
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License,
 * with a specific linking exception for the OpenSSL project's
 * "OpenSSL" library (or with modified versions of it that use the
 * same license as the "OpenSSL" library).
 *
 * OpenBoard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBoard. If not, see <http://www.gnu.org/licenses/>.
 */




#include "UBGraphicsSvgItem.h"
#include "UBShapeSvg.h"
#include "UBQuadraticFormula.h"
#include "UBMathFormula.h"
#include <QtWidgets>
#include "UBUndoCommand.h"

#include <QtGui>
#include <QDomDocument>
#include <QCryptographicHash>
#include <QUndoCommand>
#include <QPointer>
#include "board/UBDrawingController.h"

#include "UBGraphicsScene.h"
#include "UBGraphicsItemDelegate.h"
#include "UBGraphicsPixmapItem.h"

#include "core/UBApplication.h"
#include "core/UBPersistenceManager.h"

#include "board/UBBoardController.h"
#include "board/UBBoardView.h"

#include "frameworks/UBFileSystemUtils.h"

#include "core/memcheck.h"

UBGraphicsSvgItem::UBGraphicsSvgItem(const QString& pFilePath, QGraphicsItem* parent)
    : QGraphicsSvgItem(pFilePath, parent)
{
    init();

    QFile f(pFilePath);

    if (f.open(QIODevice::ReadOnly))
    {
        mFileData = f.readAll();
        f.close();
    }
    initTeachingHandles();
    refreshFormulaInputs();
}

UBGraphicsSvgItem::UBGraphicsSvgItem(const QByteArray& pFileData, QGraphicsItem* parent)
    : QGraphicsSvgItem(parent)
{
    init();

    QSvgRenderer* renderer = new QSvgRenderer(pFileData, this);

    setSharedRenderer(renderer);
    mFileData = pFileData;
    initTeachingHandles();
    refreshFormulaInputs();
}


void UBGraphicsSvgItem::init()
{
    setData(UBGraphicsItemData::ItemLayerType, UBItemLayerType::Object);

    setDelegate(new UBGraphicsItemDelegate(this, 0, GF_COMMON
                                           | GF_RESPECT_RATIO
                                           | GF_REVOLVABLE));
    UBGraphicsFlags dfl = Delegate()->ubflags();
    Delegate()->setUBFlags(dfl | GF_FLIPPABLE_ALL_AXIS | GF_REVOLVABLE);

    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);

    setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    setMaximumCacheSize(boundingRect().size().toSize() * UB_MAX_ZOOM);

    setData(UBGraphicsItemData::itemLayerType, QVariant(itemLayerType::ObjectItem)); //Necessary to set if we want z value to be assigned correctly

    setData(UBGraphicsItemData::ItemCanBeSetAsBackground, true);

    setUuid(QUuid::createUuid());
}

UBGraphicsSvgItem::~UBGraphicsSvgItem()
{
    mFormulaSyncing=true;
    for(auto input:mFormulaInputs)disconnect(input,nullptr,this,nullptr);
    if(mFormulaProxy)delete mFormulaProxy.data();
}


QByteArray UBGraphicsSvgItem::fileData() const
{
    return isTeachingShape()?UBShapeGeometry::uniformSvg(mFileData,sceneTransform()):mFileData;
}


QVariant UBGraphicsSvgItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if(isLessonCover()&&mCoverPlacementReady&&(isScreenShade()||Delegate()->isLocked())){
        if(change==ItemPositionChange)return pos();
        if(change==ItemTransformChange)return transform();
        if(change==ItemScaleChange)return scale();
        if(change==ItemRotationChange)return rotation();
    }
    QVariant newValue = Delegate()->itemChange(change, value);
    if (change == QGraphicsItem::ItemZValueChange && isLessonCover()) newValue = 2000000.0;
    auto result = QGraphicsSvgItem::itemChange(change, newValue);
    if (change == QGraphicsItem::ItemSelectedHasChanged) refreshTeachingHandles();
    if(isLessonCover()&&(change==ItemPositionHasChanged||change==ItemTransformHasChanged||change==ItemScaleHasChanged||change==ItemRotationHasChanged))refreshCoverControls();
    return result;
}


void UBGraphicsSvgItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    mQuadraticPressedButton = event->button()==Qt::LeftButton?UBMathFormula::stepAt(mFileData, event->pos().x(), event->pos().y()):-1;
    if (mQuadraticPressedButton >= 0) {
        for(auto input:mFormulaInputs)input->clearFocus();
        event->accept();
        return;
    }
    QMimeData* pMime = new QMimeData();
    auto preview = toPixmapItem();
    QPixmap pixmap = preview->pixmap();
    delete preview;
    pMime->setImageData(pixmap.toImage());
    Delegate()->setMimeData(pMime);
    if (!pixmap.isNull()) {
        Delegate()->setDragPixmap(pixmap.scaledToWidth(100, Qt::SmoothTransformation));
    }
    if (!Delegate()->mousePressEvent(event))
        QGraphicsSvgItem::mousePressEvent(event);
}


void UBGraphicsSvgItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if(mQuadraticPressedButton>=0){event->accept();return;}
    if (Delegate()->mouseMoveEvent(event))
    {
        // NOOP;
    }
    else
    {
        QGraphicsSvgItem::mouseMoveEvent(event);
    }
}


void UBGraphicsSvgItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    const int direction = UBMathFormula::stepAt(mFileData, event->pos().x(), event->pos().y());
    if (mQuadraticPressedButton >= 0) {
        const bool clicked = direction == mQuadraticPressedButton;
        mQuadraticPressedButton = -1;
        UBMathFormula::State state;
        if (clicked && UBMathFormula::read(mFileData, &state)) {
            const int last=UBMathFormula::lastStep(state);
            const int next=direction==0?qMax(0,state.step-1):direction==1||direction==5?qMin(last,state.step+1):direction==2?0:last;
            const bool toggle=direction==4;
            if (toggle||next!=state.step) {
                const QByteArray before=mFileData;
                if(toggle)state.tapToReveal=!state.tapToReveal;else state.step=next;
                setTeachingSvg(UBMathFormula::render(state));
                commitTeachingSvg(before);
            }
        }
        event->accept();
        return;
    }
    Delegate()->mouseReleaseEvent(event);
    QGraphicsSvgItem::mouseReleaseEvent(event);
}


void UBGraphicsSvgItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    // Never draw the rubber band, we draw our custom selection with the DelegateFrame
    QStyleOptionGraphicsItem styleOption = QStyleOptionGraphicsItem(*option);
    styleOption.state &= ~QStyle::State_Selected;

    if (isTeachingShape()) {
        QSvgRenderer uniform(fileData());
        uniform.render(painter, boundingRect());
    } else if(isLessonCover()&&mFileData.contains("data-cover-controls")){
        QDomDocument doc;doc.setContent(mFileData);auto root=doc.documentElement();
        for(auto n=root.firstChildElement();!n.isNull();){auto next=n.nextSiblingElement();
            if(n.attribute("id")=="shade-close"||(n.tagName()=="path"&&n.attribute("stroke")=="#475569"))root.removeChild(n);n=next;}
        QSvgRenderer body(doc.toByteArray());body.render(painter,boundingRect());
    } else QGraphicsSvgItem::paint(painter, &styleOption, widget);
    if(isTeachingShape()||isLessonCover()||mFileData.contains("mathscrap-formula")) {
        // A light outline conveys selection without obscuring the diagram.
        if(isSelected()) {painter->save();QPen pen(QColor("#147d92"),1,Qt::DashLine);pen.setCosmetic(true);painter->setPen(pen);painter->setBrush(Qt::NoBrush);painter->drawRect(boundingRect().adjusted(1,1,-1,-1));painter->restore();}
    } else Delegate()->postpaint(painter, option, widget);
}


UBItem* UBGraphicsSvgItem::deepCopy() const
{
    UBGraphicsSvgItem* copy = new UBGraphicsSvgItem(this->fileData());

    if (!isTeachingShape() && !mFileData.contains("graphdesk-project") && !mFileData.contains("lessonboard-quadratic") && !mFileData.contains("mathscrap-formula"))
        copy->setUuid(this->uuid());

    copyItemParameters(copy);

    // TODO UB 4.7... complete all members ?

    return copy;

}

void UBGraphicsSvgItem::copyItemParameters(UBItem *copy) const
{
    UBGraphicsSvgItem *cp = dynamic_cast<UBGraphicsSvgItem*>(copy);
    if (cp)
    {
        cp->setPos(this->pos());
        cp->setTransform(this->transform());
        cp->setTransformOriginPoint(this->transformOriginPoint());
        cp->setRotation(this->rotation());
        cp->setScale(this->scale());
        cp->setFlag(QGraphicsItem::ItemIsMovable, true);
        cp->setFlag(QGraphicsItem::ItemIsSelectable, true);
        cp->setData(UBGraphicsItemData::ItemLayerType, this->data(UBGraphicsItemData::ItemLayerType));
        cp->setData(UBGraphicsItemData::ItemLocked, this->data(UBGraphicsItemData::ItemLocked));
        cp->setData(UBGraphicsItemData::ItemIsHiddenOnDisplay, this->data(UBGraphicsItemData::ItemIsHiddenOnDisplay));
        cp->setData(UBGraphicsItemData::ItemOwnZValue, this->data(UBGraphicsItemData::ItemOwnZValue));
        cp->setSourceUrl(this->sourceUrl());
        cp->setZValue(this->zValue());
    }
}

void UBGraphicsSvgItem::setRenderingQuality(RenderingQuality pRenderingQuality)
{
    UBItem::setRenderingQuality(pRenderingQuality);

    if (pRenderingQuality == RenderingQualityHigh)
    {
        setCacheMode(QGraphicsItem::NoCache);
    }
    else
    {
        setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    }
}


std::shared_ptr<UBGraphicsScene> UBGraphicsSvgItem::scene()
{
    auto scenePtr = dynamic_cast<UBGraphicsScene*>(QGraphicsItem::scene());
    return scenePtr ? scenePtr->shared_from_this() : nullptr;
}



UBGraphicsPixmapItem* UBGraphicsSvgItem::toPixmapItem() const
{
    QImage image(renderer()->viewBox().size(), QImage::Format_ARGB32);
    QPainter painter(&image);
    if (isTeachingShape()) { QSvgRenderer uniform(fileData()); uniform.render(&painter); }
    else renderer()->render(&painter);

    UBGraphicsPixmapItem *pixmapItem =  new UBGraphicsPixmapItem();
    pixmapItem->setPixmap(QPixmap::fromImage(image));

    pixmapItem->setPos(this->pos());
    pixmapItem->setTransform(this->transform());
    pixmapItem->setFlag(QGraphicsItem::ItemIsMovable, true);
    pixmapItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    pixmapItem->setData(UBGraphicsItemData::ItemLayerType, this->data(UBGraphicsItemData::ItemLayerType));

    return pixmapItem;
}

void UBGraphicsSvgItem::setUuid(const QUuid &pUuid)
{
    UBItem::setUuid(pUuid);
    setData(UBGraphicsItemData::ItemUuid, QVariant(pUuid)); //store item uuid inside the QGraphicsItem to fast operations with Items on the scene
}


void UBGraphicsSvgItem::clearSource()
{
    QString fileName = UBPersistenceManager::imageDirectory + "/" + uuid().toString() + ".svg";
    QString diskPath =  UBApplication::boardController->selectedDocument()->persistencePath() + "/" + fileName;
    UBFileSystemUtils::deleteFile(diskPath);
}

namespace {
QDomDocument shapeDocument(const QByteArray &data) {
    QDomDocument doc; doc.setContent(data); return doc;
}
QList<QPointF> trianglePoints(const QByteArray &data) {
    const auto doc = shapeDocument(data);
    const auto polygons = doc.elementsByTagName("polygon");
    if (polygons.isEmpty()) return {};
    const QStringList coords = polygons.at(0).toElement().attribute("points").split(QRegularExpression("[ ,]+"), Qt::SkipEmptyParts);
    if (coords.size() != 6) return {};
    QList<QPointF> points;
    for (int i = 0; i < 6; i += 2) points << QPointF(coords[i].toDouble(), coords[i+1].toDouble());
    return points;
}
class TeachingSvgUndo : public UBUndoCommand {
public:
    TeachingSvgUndo(UBGraphicsSvgItem *item, const QByteArray &before, const QByteArray &after)
        : mItem(item), mBefore(before), mAfter(after) { setText(QObject::tr("Edit shape or graph")); }
    void undo() override { if (mItem) mItem->setTeachingSvg(mBefore); }
    void redo() override { if (mItem) mItem->setTeachingSvg(mAfter); }
private:
    QPointer<UBGraphicsSvgItem> mItem;
    QByteArray mBefore, mAfter;
};
class CoverControl : public QGraphicsObject {
public:
    CoverControl(UBGraphicsSvgItem *cover,const QString &role):QGraphicsObject(cover),mCover(cover),mRole(role){
        setData(0,"cover-control");setData(1,role);
        setData(UBGraphicsItemData::ItemLayerType,UBItemLayerType::Control);
        setFlag(QGraphicsItem::ItemIgnoresTransformations);setAcceptedMouseButtons(Qt::NoButton);
        setZValue(30);setCursor(role=="grip"?Qt::SizeVerCursor:Qt::PointingHandCursor);
    }
    QRectF boundingRect() const override{return mRole=="grip"?QRectF(-24,-12,48,24):QRectF(-24,-24,48,48);}
    void paint(QPainter *p,const QStyleOptionGraphicsItem *,QWidget *widget) override{
        // Controls belong to the interactive board, never PDFs/thumbnails.
        auto scene=mCover->scene();if(!scene||scene->renderingContext()!=UBGraphicsScene::Screen||!widget)return;
        p->save();p->setRenderHint(QPainter::Antialiasing);p->setPen(QPen(QColor("#475569"),1.5));p->setBrush(QColor("#fffdf7"));
        if(mRole=="grip"){
            p->drawRoundedRect(QRectF(-23,-10,46,20),8,8);p->setPen(QPen(QColor("#475569"),2,Qt::SolidLine,Qt::RoundCap));
            p->drawLine(QPointF(-12,-2),QPointF(12,-2));p->drawLine(QPointF(-12,3),QPointF(12,3));
        }else{
            p->drawEllipse(QRectF(-22,-22,44,44));p->setPen(QPen(QColor("#334155"),2.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p->setBrush(Qt::NoBrush);
            if(mRole=="close"){p->drawLine(QPointF(-6,-6),QPointF(6,6));p->drawLine(QPointF(-6,6),QPointF(6,-6));}
            else{
                p->drawRoundedRect(QRectF(-8,-2,16,13),2,2);QPainterPath shackle;
                if(mCover->Delegate()->isLocked()){shackle.moveTo(-5,-2);shackle.lineTo(-5,-7);shackle.cubicTo(-5,-15,5,-15,5,-7);shackle.lineTo(5,-2);}
                else{shackle.moveTo(-5,-2);shackle.lineTo(-5,-7);shackle.cubicTo(-5,-15,6,-15,6,-7);}
                p->drawPath(shackle);p->drawLine(QPointF(0,3),QPointF(0,6));
            }
        }p->restore();
    }
private:UBGraphicsSvgItem *mCover;QString mRole;
};
class TeachingVertexHandle : public QGraphicsEllipseItem {
public:
    TeachingVertexHandle(UBGraphicsSvgItem *item, int index)
        : QGraphicsEllipseItem(-6,-6,12,12,item), mItem(item), mIndex(index) {
        setBrush(QColor("#ffffff")); setPen(QPen(QColor("#147d92"),2));
        setFlag(QGraphicsItem::ItemIgnoresTransformations);
        setAcceptedMouseButtons(Qt::LeftButton);
        setCursor(Qt::SizeAllCursor);
        setData(UBGraphicsItemData::ItemLayerType, UBItemLayerType::Control);
        setZValue(10);
    }
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
        mBefore = mItem->fileData(); event->accept();
    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override {
        auto points = trianglePoints(mItem->fileData());
        if (points.size() != 3) return;
        auto point = mItem->mapFromScene(event->scenePos());
        const auto bounds = mItem->boundingRect().adjusted(6,6,-6,-6);
        points[mIndex] = QPointF(qBound(bounds.left(),point.x(),bounds.right()), qBound(bounds.top(),point.y(),bounds.bottom()));
        QStringList values;
        for (auto p : points) values << QString("%1,%2").arg(p.x()).arg(p.y());
        auto doc = shapeDocument(mItem->fileData());
        doc.elementsByTagName("polygon").at(0).toElement().setAttribute("points", values.join(" "));
        mItem->setTeachingSvg(doc.toByteArray());
        event->accept();
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override {
        mItem->commitTeachingSvg(mBefore); event->accept();
    }
private:
    UBGraphicsSvgItem *mItem;
    int mIndex;
    QByteArray mBefore;
};
}

bool UBGraphicsSvgItem::isTeachingShape() const
{
    return mFileData.contains("teaching-shape");
}

void UBGraphicsSvgItem::initTeachingHandles()
{
    if (isLessonCover()) {
        setData(UBGraphicsItemData::itemLayerType, int(itemLayerType::ToolItem));
        Delegate()->setUBFlags(Delegate()->ubflags() & ~UBGraphicsFlags(GF_RESPECT_RATIO));
        setData(UBGraphicsItemData::ItemCanBeSetAsBackground, false);
        UBGraphicsItem::assignZValue(this,2000000.0);
        mCoverControls<<new CoverControl(this,"grip")<<new CoverControl(this,isScreenShade()?"close":"lock");
        if(isScreenShade())Delegate()->setLocked(true);
        if(UBApplication::boardController){
            QObject::connect(UBApplication::boardController,&UBBoardController::controlViewportChanged,this,[this]{refreshCoverControls();});
        }
        return;
    }
    if (!isTeachingShape()) return;
    auto doc = shapeDocument(mFileData);
    auto metadata = doc.elementsByTagName("metadata");
    const int type = metadata.isEmpty() ? -1 : metadata.at(0).toElement().text().toInt();
    const bool triangle=(type==3||type==5)&&trianglePoints(mFileData).size()==3;
    if (type == 0 || type == 4 || type == 6 || type == 8 || (type >= 10 && type != 14) || triangle)
        Delegate()->setUBFlags(Delegate()->ubflags() & ~UBGraphicsFlags(GF_RESPECT_RATIO));
    if (triangle && type!=5) {
        for (int i = 0; i < 3; ++i) mTeachingHandles << new TeachingVertexHandle(this, i);
        connect(UBDrawingController::drawingController(), &UBDrawingController::stylusToolChanged, this,
                [this](int, int) { refreshTeachingHandles(); });
    }
    refreshTeachingHandles();
}

void UBGraphicsSvgItem::refreshTeachingHandles()
{
    if(isLessonCover()){refreshCoverControls();return;}
    const auto points = trianglePoints(mFileData);
    const bool visible = isSelected() && UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Selector;
    for (int i = 0; i < mTeachingHandles.size(); ++i) {
        if (i < points.size()) mTeachingHandles[i]->setPos(points[i]);
        mTeachingHandles[i]->setVisible(visible);
    }
}

void UBGraphicsSvgItem::setTeachingSvg(const QByteArray &data)
{
    // QUndoStack calls redo() when a freshly committed edit is pushed. Reusing
    // the same content must not fit the object again at the current page zoom.
    if(data==mFileData)return;
    const QSizeF previousSize=boundingRect().size();
    // Detach on first edit: duplicated pages may initially share SVG assets.
    if (!mTeachingDetached) { setUuid(QUuid::createUuid()); mTeachingDetached = true; }
    prepareGeometryChange();
    mFileData = data;
    if(data.contains("lessonboard-quadratic")||data.contains("mathscrap-formula")) {
        // QGraphicsSvgItem caches the default bounds when a renderer is assigned.
        // Merely loading taller data squeezes every added line into the old bounds.
        QPointer<QSvgRenderer> previous=renderer();
        setSharedRenderer(new QSvgRenderer(data,this));
        if(previous && previous->parent()==this)previous->deleteLater();
        if(isSelected())Delegate()->update();
    } else renderer()->load(data);
    refreshTeachingHandles();
    refreshFormulaInputs();
    if(data.contains("mathscrap-formula")&&boundingRect().size()!=previousSize)fitFormulaInView();
    update();
    if (scene()) scene()->setModified(true);
}

void UBGraphicsSvgItem::commitTeachingSvg(const QByteArray &before)
{
    if (before != mFileData && UBApplication::undoStack)
        UBApplication::undoStack->push(new TeachingSvgUndo(this, before, mFileData));
}

bool UBGraphicsSvgItem::isScreenShade() const{return isLessonCover()&&mFileData.contains("data-screen-shade");}
void UBGraphicsSvgItem::setCoverLocked(bool locked){
    if(!isLessonCover())return;
    locked=locked||isScreenShade();
    Delegate()->setLocked(locked);
    setFlag(QGraphicsItem::ItemIsMovable,!locked);
    mCoverPlacementReady=true;
    if(Delegate()->controlsExist())Delegate()->lock(locked);
    refreshCoverControls();
    if(scene())scene()->setModified(true);
}
QRectF UBGraphicsSvgItem::coverControlRect(const QString &role) const{
    if(!UBApplication::boardController||!UBApplication::boardController->controlView())return {};
    auto view=UBApplication::boardController->controlView();
    for(auto control:mCoverControls)if(control->data(1).toString()==role)return control->deviceTransform(view->viewportTransform()).mapRect(control->boundingRect());
    return {};
}
void UBGraphicsSvgItem::refreshCoverControls(){
    if(!isLessonCover()||mCoverControls.isEmpty()||!scene()||!UBApplication::boardController)return;
    auto view=UBApplication::boardController->controlView();if(!view)return;
    QDomDocument doc;doc.setContent(mFileData);auto rect=doc.elementsByTagName("rect").at(0).toElement();
    QRectF visible(rect.attribute("x").toDouble(),rect.attribute("y").toDouble(),rect.attribute("width").toDouble(),rect.attribute("height").toDouble());
    if(doc.elementsByTagName("metadata").at(0).toElement().text()=="revealed")visible.setTop(qMax(visible.top(),visible.bottom()-8));
    const auto vp=view->viewport()->rect();
    auto screen=[&](const QPointF &p){return QPointF(view->mapFromScene(mapToScene(p)));};
    const QPointF top=screen(QPointF(visible.center().x(),visible.top()));
    const QPointF right=screen(visible.topRight());
    for(auto control:mCoverControls){
        const bool grip=control->data(1).toString()=="grip";
        QPointF p=grip?top+QPointF(0,8):right+QPointF(-26,26);
        // Keep a full-size button reachable when only a thin strip remains.
        if(!grip&&qAbs(p.x()-top.x())<56)p.setX(right.x()+26);
        const int margin=grip?14:26;
        p.setX(qBound(double(vp.left()+26),p.x(),double(qMax(vp.left()+26,vp.right()-26))));
        p.setY(qBound(double(vp.top()+margin),p.y(),double(qMax(vp.top()+margin,vp.bottom()-margin))));
        control->setPos(mapFromScene(view->mapToScene(p.toPoint())));
        control->setToolTip(grip?QObject::tr("Drag down to reveal; drag up to cover"):
            isScreenShade()?QObject::tr("Close screen shade"):
            Delegate()->isLocked()?QObject::tr("Locked — tap to unlock cover"):QObject::tr("Unlocked — tap to lock cover"));
        control->update();
    }
}

bool UBGraphicsSvgItem::isLessonCover() const
{
    return mFileData.contains("lessonboard-cover");
}

QWidget *UBGraphicsSvgItem::formulaInputWidget() const {return mFormulaProxy?mFormulaProxy->widget():nullptr;}
void UBGraphicsSvgItem::refreshFormulaInputs()
{
    if(mFormulaSyncing)return;
    UBMathFormula::State state;if(!UBMathFormula::read(mFileData,&state))return;
    mFormulaSyncing=true;
    const QString key=state.kind+QString::number(state.mode);
    if(!mFormulaProxy||key!=mFormulaLayoutKey){
        mFormulaLayoutKey=key;
        if(mFormulaProxy){mFormulaProxy->hide();mFormulaProxy->deleteLater();}
        mFormulaInputs.clear();
        auto row=new QWidget;row->setObjectName("mathscrapFormulaInputs");row->setProperty("mathscrapInputs",true);
        row->setAttribute(Qt::WA_TranslucentBackground);row->setStyleSheet("QWidget{background:transparent;color:#253e3c;} QFrame#mathscrapField{background:#f1eee5;border:1px solid transparent;border-radius:7px;} QLineEdit{background:transparent;border:0;border-radius:3px;padding:0;font-family:'Cambria Math';font-size:22px;color:#253e3c;selection-background-color:#c9ded4;} QLineEdit:focus{background:#fffdf7;border:1px solid #77a99a;} QLabel{font-family:'Cambria Math';font-size:22px;} QComboBox{background:#f1eee5;color:#253e3c;border:1px solid #d8d1c3;border-radius:5px;padding:4px;font-size:17px;}");
        const auto names=UBMathFormula::labels(state);const bool function=names.size()==1;
        const bool points=UBMathFormula::pointInputs(state);
        // Point labels and punctuation are already painted in the SVG. Only
        // coordinate fields are native widgets, avoiding duplicate text layers.
        for(int i=0;i<names.size();++i){auto frame=new QFrame(row);frame->setObjectName("mathscrapField");frame->setGeometry(UBMathFormula::fieldRect(state,i).toRect());auto layout=new QHBoxLayout(frame);layout->setContentsMargins(10,0,8,0);layout->setSpacing(4);if(!points){auto label=new QLabel(names[i]+" =",frame);if(function)label->setStyleSheet("font-size:18px;");layout->addWidget(label);}auto input=new QLineEdit(frame);input->setObjectName(QString("formulaValue%1").arg(i));input->setAccessibleName(points?QString(i<2?"Point A: %1":"Point B: %1").arg(names[i]):names[i]);input->setMinimumWidth(0);input->setMaxLength(function?180:14);input->setPlaceholderText(function?"e.g. x^2 + 6x + 7":"");if(function)input->setStyleSheet("font-size:18px;");layout->addWidget(input,1);mFormulaInputs<<input;
            if(i>0)QWidget::setTabOrder(mFormulaInputs[i-1],input);
            connect(input,&QLineEdit::textEdited,this,[this,key](const QString &){
                if(mFormulaSyncing||key!=mFormulaLayoutKey)return;
                UBMathFormula::State s;if(!UBMathFormula::read(mFileData,&s))return;
                if(mFormulaEditBefore.isEmpty())mFormulaEditBefore=mFileData;
                s.values.clear();for(auto field:mFormulaInputs)s.values<<field->text();s.step=0;
                if(UBMathFormula::error(s).isEmpty())s.step=qMin(1,UBMathFormula::lastStep(s));
                setTeachingSvg(UBMathFormula::render(s));
            });
            connect(input,&QLineEdit::editingFinished,this,[this,key]{if(key!=mFormulaLayoutKey||mFormulaEditBefore.isEmpty())return;const auto before=mFormulaEditBefore;mFormulaEditBefore.clear();commitTeachingSvg(before);});
        }
        const auto choices=UBMathFormula::modes(state);
        if(!choices.isEmpty()){auto mode=new QComboBox(row);mode->setObjectName("formulaMode");mode->setGeometry(0,47,270,34);mode->addItems(choices);mode->setCurrentIndex(qBound(0,state.mode,int(choices.size())-1));connect(mode,QOverload<int>::of(&QComboBox::activated),this,[this,key](int index){if(mFormulaSyncing||key!=mFormulaLayoutKey)return;UBMathFormula::State s;if(!UBMathFormula::read(mFileData,&s))return;const auto before=mFileData;s.mode=index;s.values.clear();s.step=0;mFormulaEditBefore.clear();setTeachingSvg(UBMathFormula::render(s));commitTeachingSvg(before);});}
        mFormulaProxy=new QGraphicsProxyWidget(this);mFormulaProxy->setWidget(row);mFormulaProxy->setData(0,"mathscrap-formula-input");mFormulaProxy->setZValue(20);mFormulaProxy->setData(UBGraphicsItemData::ItemLayerType,UBItemLayerType::Control);mFormulaProxy->setCacheMode(QGraphicsItem::NoCache);
    }
    const auto geometry=UBMathFormula::inputRect(state);mFormulaProxy->setPos(geometry.topLeft());mFormulaProxy->widget()->setFixedSize(geometry.size().toSize());
    for(int i=0;i<mFormulaInputs.size();++i){auto input=mFormulaInputs[i];if(input->text()!=state.values.value(i)){QSignalBlocker block(input);input->setText(state.values.value(i));}}
    mFormulaSyncing=false;
}

void UBGraphicsSvgItem::fitFormulaInView()
{
    if(!scene()||!UBApplication::boardController)return;
    auto view=UBApplication::boardController->controlView();
    if(!view||view->scene()!=scene())return;
    auto viewport=view->viewport();QRectF safe=viewport->rect().adjusted(16,16,-16,-16);
    if(auto pages=view->findChild<QWidget*>("paperPages"))if(pages->isVisible())safe.setLeft(pages->mapTo(viewport,QPoint(pages->width(),0)).x()+16);
    if(auto dock=view->findChild<QWidget*>("paperDock"))if(dock->isVisible())safe.setBottom(dock->mapTo(viewport,QPoint(0,0)).y()-16);
    if(safe.width()<160||safe.height()<160)return;
    auto bounds=view->viewportTransform().mapRect(sceneBoundingRect());
    if(bounds.isEmpty())return;
    const qreal factor=qMin(1.0,qMin(safe.width()/bounds.width(),safe.height()/bounds.height()));
    if(factor<1)setScale(scale()*factor);
    bounds=view->viewportTransform().mapRect(sceneBoundingRect());
    const qreal dx=bounds.left()<safe.left()?safe.left()-bounds.left():bounds.right()>safe.right()?safe.right()-bounds.right():0;
    const qreal dy=bounds.top()<safe.top()?safe.top()-bounds.top():bounds.bottom()>safe.bottom()?safe.bottom()-bounds.bottom():0;
    const QPointF origin=view->mapToScene(QPoint(0,0)),shift=view->mapToScene(QPoint(qRound(dx),qRound(dy)))-origin;
    setPos(pos()+shift);
}
