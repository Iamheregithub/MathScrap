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
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void createCoverToolbar();
    void armCover();
    void cancelCoverDrag();
    UBGraphicsSvgItem *createCover(const QRectF &rect, bool shade);
    void revealCovers(bool selected, bool revealed);
    void manageCovers();
    QToolBar *mCoverBar = nullptr;
    QAction *mCoverAction = nullptr;
    QAction *mDrawCover = nullptr;
    bool mDrawingCover = false;
    QColor mCoverColour = QColor("#e2e8f0");
    QPointF mCoverStart;
    QGraphicsRectItem *mCoverPreview = nullptr;
    QPointer<QGraphicsScene> mCoverScene;
    QPointer<UBGraphicsSvgItem> mShadeDrag;
    QByteArray mShadeBefore;
    int mShadeEdge = -1;
    UBMainWindow *mWindow;
    UBLessonHistory *mHistory;
};
