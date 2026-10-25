// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QPointer>
#include <QList>
#include <QString>
#include <QStringList>
#include <QIcon>
class UBMainWindow;
class QWidget;
class QToolBar;
class QFrame;
class QToolButton;
class QLabel;
class QListWidget;
class QAction;
class QMenu;
class QTimer;
class UBPaperInk : public QObject {
    Q_OBJECT
public:
    explicit UBPaperInk(UBMainWindow *window);
    void rebuild();
    void applyPreferences();
    static QStringList defaultTools();
    static QString toolLabel(const QString &id);
    static QIcon toolIcon(const QString &id);
    void sync();
protected:
    bool eventFilter(QObject *object,QEvent *event) override;
private:
    void layoutPanels();
    void openTray(QToolButton *anchor);
    void closeTray();
    void selectTrayTool();
    int trayTool() const;
    void refreshPages();
    void goToPage(int index);
    void refreshPen();
    void adoptContextBars();
    void scheduleLayout();
    void buildSettingsMenu(QToolButton *button);
    QToolButton *button(QAction *source,const QString &label,QWidget *parent);
    UBMainWindow *mWindow;
    QToolBar *mHeader=nullptr;
    QFrame *mDock=nullptr,*mPenPanel=nullptr,*mPages=nullptr;
    QLabel *mPageCount=nullptr;
    QListWidget *mPageList=nullptr;
    QToolButton *mPageToggle=nullptr,*mShapePicker=nullptr;
    QAction *mShapePickerAction=nullptr;
    QMenu *mSettingsMenu=nullptr,*mOverflowMenu=nullptr;
    QList<QToolButton*> mColours,mWidths;
    bool mScheduled=false,mRefreshing=false,mPagesOpen=true,mClosing=false,mLibraryOpen=false;
    QString mDocumentId,mTrayAction;
    QPointer<QToolButton> mTrayAnchor,mHoverAnchor;
    QTimer *mHoverTimer=nullptr,*mCloseTimer=nullptr;
    bool mTrayOpen=false;
};
