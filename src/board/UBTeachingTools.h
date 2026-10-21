// SPDX-License-Identifier: GPL-3.0-only
#ifndef UBTEACHINGTOOLS_H
#define UBTEACHINGTOOLS_H
#include <QObject>
#include <QMap>
#include <QPointer>
#include <QList>
#include <QStringList>
class QWidget;
class QAction;
class UBMainWindow;
class QWebEngineView;
class UBGraphicsSvgItem;
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
