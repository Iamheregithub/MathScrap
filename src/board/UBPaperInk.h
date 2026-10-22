// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QPointer>
#include <QList>
#include <QString>
class UBMainWindow;
class QWidget;
class QToolBar;
class QFrame;
class QToolButton;
class QLabel;
class QListWidget;
class QAction;
class QMenu;
class UBPaperInk : public QObject {
    Q_OBJECT
public:
    explicit UBPaperInk(UBMainWindow *window);
    void rebuild();
    void sync();
protected:
    bool eventFilter(QObject *object,QEvent *event) override;
private:
    void layoutPanels();
    void refreshPages();
    void goToPage(int index);
    void refreshPen();
    void adoptContextBars();
    void scheduleLayout();
    QToolButton *button(QAction *source,const QString &label,QWidget *parent);
    UBMainWindow *mWindow;
    QToolBar *mHeader=nullptr;
    QFrame *mDock=nullptr,*mPenPanel=nullptr,*mPages=nullptr;
    QLabel *mTitle=nullptr,*mPageCount=nullptr;
    QListWidget *mPageList=nullptr;
    QToolButton *mPageToggle=nullptr;
    QMenu *mMore=nullptr;
    QList<QAction*> mOverflowActions;
    QList<QToolButton*> mColours,mWidths;
    bool mScheduled=false,mRefreshing=false,mPagesOpen=true,mClosing=false,mLibraryOpen=false;
    QString mDocumentId;
};
