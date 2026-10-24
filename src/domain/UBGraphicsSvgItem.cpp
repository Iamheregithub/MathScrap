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
    if (!isTeachingShape()) return mFileData;
    QDomDocument doc;
    if (!doc.setContent(mFileData)) return mFileData;
    auto root = doc.documentElement();
    auto groups = doc.elementsByTagName("g");
    if (groups.isEmpty()) return mFileData;
    auto group = groups.at(0).toElement();
    auto shape = group.firstChildElement();
    if (shape.isNull()) return mFileData;
    const auto matrix = sceneTransform();
    // Work in page coordinates, excluding translation. Zoom remains a view
    // transform, so an outline still zooms naturally with the rest of the page.
    QTransform linear(matrix.m11(), matrix.m12(), matrix.m21(), matrix.m22(), 0, 0);
    bool invertible = false;
    const auto inverse = linear.inverted(&invertible);
    if (!invertible) return mFileData;
    QStringList key {shape.tagName(), group.attribute("stroke"), group.attribute("stroke-width", "3"), group.attribute("stroke-dasharray", "none")};
    for (const auto &name : {"x", "y", "width", "height", "cx", "cy", "r", "rx", "ry", "points"}) key << shape.attribute(name);
    for (auto v : {matrix.m11(), matrix.m12(), matrix.m21(), matrix.m22()}) key << QString::number(v, 'g', 6);
    const QString fingerprint = QString::fromLatin1(QCryptographicHash::hash(key.join('|').toUtf8(), QCryptographicHash::Sha256).toHex());
    if (root.attribute("data-lessonboard-outline-key") == fingerprint) return mFileData;
    QPainterPath path;
    if (shape.tagName() == "rect") {
        path.addRect(QRectF(shape.attribute("x").toDouble(), shape.attribute("y").toDouble(),
                           shape.attribute("width").toDouble(), shape.attribute("height").toDouble()));
    } else if (shape.tagName() == "circle") {
        const double radius = shape.attribute("r").toDouble();
        path.addEllipse(QPointF(shape.attribute("cx").toDouble(), shape.attribute("cy").toDouble()), radius, radius);
    } else if (shape.tagName() == "ellipse") {
        path.addEllipse(QPointF(shape.attribute("cx").toDouble(), shape.attribute("cy").toDouble()), shape.attribute("rx").toDouble(), shape.attribute("ry").toDouble());
    } else if (shape.tagName() == "polygon") {
        const auto coords = shape.attribute("points").split(QRegularExpression("[ ,]+"), Qt::SkipEmptyParts);
        if (coords.size() < 6 || coords.size() % 2) return mFileData;
        path.moveTo(coords[0].toDouble(), coords[1].toDouble());
        for (int i = 2; i < coords.size(); i += 2) path.lineTo(coords[i].toDouble(), coords[i+1].toDouble());
        path.closeSubpath();
    } else return mFileData;
    // A filled SVG path represents the outline. Ordinary OpenBoard and PDF
    // exporters can therefore retain its uniform width without custom painting
    // or relying on SVG vector-effect support.
    QPainterPathStroker stroker;
    stroker.setWidth(group.attribute("stroke-width", "3").toDouble());
    if (group.attribute("stroke-dasharray", "none") != "none") {
        QVector<qreal> dashes;
        const double width = group.attribute("stroke-width", "3").toDouble();
        for (const auto &v : group.attribute("stroke-dasharray").split(QRegularExpression("[ ,]+"), Qt::SkipEmptyParts))
            if (width > 0) dashes << v.toDouble()/width;
        if (!dashes.isEmpty()) stroker.setDashPattern(dashes);
    }
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setCapStyle(Qt::RoundCap);
    const auto outline = inverse.map(stroker.createStroke(linear.map(path)));
    QString commands;
    auto number = [](double v) { return QString::number(v, 'g', 17); };
    for (int i = 0; i < outline.elementCount(); ++i) {
        const auto e = outline.elementAt(i);
        if (e.isMoveTo()) commands += "M" + number(e.x) + " " + number(e.y);
        else if (e.isLineTo()) commands += "L" + number(e.x) + " " + number(e.y);
        else if (e.type == QPainterPath::CurveToElement && i + 2 < outline.elementCount()) {
            const auto c2 = outline.elementAt(++i), end = outline.elementAt(++i);
            commands += "C" + number(e.x) + " " + number(e.y) + " " + number(c2.x) + " " + number(c2.y) + " " + number(end.x) + " " + number(end.y);
        }
    }
    auto old = root.firstChildElement("path");
    while (!old.isNull()) {
        const auto next = old.nextSiblingElement("path");
        if (old.attribute("id") == "lessonboard-shape-outline") root.removeChild(old);
        old = next;
    }
    shape.setAttribute("stroke", "none");
    auto baked = doc.createElement("path");
    baked.setAttribute("id", "lessonboard-shape-outline");
    baked.setAttribute("d", commands);
    baked.setAttribute("fill", group.attribute("stroke", "#000000"));
    baked.setAttribute("stroke", "none");
    baked.setAttribute("fill-rule", "nonzero");
    root.appendChild(baked);
    root.setAttribute("data-lessonboard-outline-key", fingerprint);
    return doc.toByteArray();
}


QVariant UBGraphicsSvgItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    QVariant newValue = Delegate()->itemChange(change, value);
    if (change == QGraphicsItem::ItemZValueChange && isLessonCover()) newValue = 2000000.0;
    auto result = QGraphicsSvgItem::itemChange(change, newValue);
    if (change == QGraphicsItem::ItemSelectedHasChanged) refreshTeachingHandles();
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
        const bool clicked = event->button()==Qt::LeftButton && direction == mQuadraticPressedButton;
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
    } else QGraphicsSvgItem::paint(painter, &styleOption, widget);
    Delegate()->postpaint(painter, option, widget);
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
        setZValue(2000000.0);
        return;
    }
    if (!isTeachingShape()) return;
    const bool triangle = trianglePoints(mFileData).size() == 3;
    auto doc = shapeDocument(mFileData);
    auto metadata = doc.elementsByTagName("metadata");
    const int type = metadata.isEmpty() ? -1 : metadata.at(0).toElement().text().toInt();
    if (type == 0 || type == 4 || type == 6 || triangle)
        Delegate()->setUBFlags(Delegate()->ubflags() & ~UBGraphicsFlags(GF_RESPECT_RATIO));
    if (triangle) {
        for (int i = 0; i < 3; ++i) mTeachingHandles << new TeachingVertexHandle(this, i);
        connect(UBDrawingController::drawingController(), &UBDrawingController::stylusToolChanged, this,
                [this](int, int) { refreshTeachingHandles(); });
    }
    refreshTeachingHandles();
}

void UBGraphicsSvgItem::refreshTeachingHandles()
{
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
