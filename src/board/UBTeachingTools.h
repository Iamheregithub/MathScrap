// SPDX-License-Identifier: GPL-3.0-only
#ifndef UBTEACHINGTOOLS_H
#define UBTEACHINGTOOLS_H
#include <QObject>
#include <QMap>
#include <QPointer>
#include <QList>
#include <QStringList>
#include <QColor>
#include <QPointF>
class QWidget;
class QAction;
class UBMainWindow;
class QWebEngineView;
class UBGraphicsSvgItem;
class QToolBar;
class QComboBox;
class QSlider;
class QToolButton;
class QGraphicsPathItem;
class QGraphicsScene;
class QPainterPath;
class QRectF;
class UBTeachingTools : public QObject
{
public:
    explicit UBTeachingTools(UBMainWindow *window);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void customize();
    void applyLayout();
    void openGraphDesk();
    void insertGraph();
    void writeGraph(bool replace);
    void editSelectedGraph();
    void shapeDialog();
    void createShapeToolbar();
    void syncShapeSelection();
    void applyShapeStyle();
    void cancelShapeDrag();
    void armShape(int type);
    QByteArray shapeSvg(const QRectF &rect) const;
    QPainterPath shapePath(const QRectF &rect) const;
    QToolBar *mShapeBar = nullptr;
    QAction *mShapes = nullptr;
    QList<QAction*> mShapeChoices;
    QComboBox *mShapeWidth = nullptr;
    QComboBox *mShapeStyle = nullptr;
    QSlider *mShapeOpacity = nullptr;
    QToolButton *mBorderButton = nullptr;
    QToolButton *mFillButton = nullptr;
    QColor mShapeBorder = Qt::black;
    QColor mShapeFill = QColor("#ffdd00");
    bool mNoBorder = false, mNoFill = true, mShapeDrawing = false, mShapeDragging = false, mSyncingShape = false;
    int mShapeType = 0;
    QPointF mShapeStart;
    QGraphicsPathItem *mShapePreview = nullptr;
    QPointer<QGraphicsScene> mShapeScene;
    QPointer<UBGraphicsSvgItem> mShapeTarget;
    QMetaObject::Connection mShapeSelectionConnection;
    UBMainWindow *mWindow;
    QMap<QString, QAction*> mTop;
    QMap<QString, QAction*> mBottom;
    QMap<QAction*, QAction*> mProxies;
    QStringList mDefaultTop;
    QStringList mDefaultBottom;
    QPointer<QWebEngineView> mGraph;
    QAction *mCustomize = nullptr;
    QWidget *mGraphTab = nullptr;
    QPointer<UBGraphicsSvgItem> mGraphTarget;
};
#endif
