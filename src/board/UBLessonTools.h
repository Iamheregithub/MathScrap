// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QColor>
#include <QPointer>
#include <QPointF>
#include <QRectF>
class UBMainWindow;
class QWidget;
class QAction;
class UBGraphicsSvgItem;
class UBLessonHistory;
class QToolBar;
class QGraphicsRectItem;
class QGraphicsScene;
class UBLessonTools : public QObject {
    Q_OBJECT
public:
    explicit UBLessonTools(UBMainWindow *window);
    UBGraphicsSvgItem *addCover();
    void setCoverRevealed(UBGraphicsSvgItem *cover, bool revealed);
    bool isCoverRevealed(UBGraphicsSvgItem *cover) const;
    UBLessonHistory *history() const { return mHistory; }
    void showHistory();
    void applyTouchMode();
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    bool deleteAllHistory(QWidget *parent);
    void createCoverToolbar();
    void armCover();
    void cancelCoverDrag();
    UBGraphicsSvgItem *createCover(const QRectF &rect, bool shade);
    void revealCovers(bool selected, bool revealed);
    void manageCovers();
    void syncCoverLock();
    void resetReveals();
    QAction *mLockCover = nullptr;
    QMetaObject::Connection mCoverSelectionConnection;
    QToolBar *mCoverBar = nullptr;
    QAction *mCoverAction = nullptr;
    QAction *mDrawCover = nullptr;
    bool mDrawingCover = false;
    QColor mCoverColour = QColor("#e2e8f0");
    QPointF mCoverStart;
    QGraphicsRectItem *mCoverPreview = nullptr;
    QPointer<QGraphicsScene> mCoverScene;
    QPointer<UBGraphicsSvgItem> mShadeDrag;
    QPointer<UBGraphicsSvgItem> mLockedPress;
    QByteArray mShadeBefore;
    int mShadeEdge = -1;
    UBMainWindow *mWindow;
    UBLessonHistory *mHistory;
};
