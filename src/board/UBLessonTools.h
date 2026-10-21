// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
class UBMainWindow;
class QAction;
class UBGraphicsSvgItem;
class UBLessonHistory;
class UBLessonTools : public QObject {
    Q_OBJECT
public:
    explicit UBLessonTools(UBMainWindow *window);
    QJsonArray favourites() const;
    bool saveFavourite(int slot, const QJsonObject &value);
    bool applyFavourite(int slot);
    UBGraphicsSvgItem *addCover();
    void setCoverRevealed(UBGraphicsSvgItem *cover, bool revealed);
    bool isCoverRevealed(UBGraphicsSvgItem *cover) const;
    UBLessonHistory *history() const { return mHistory; }
    void showHistory();
private:
    void refreshFavourites();
    void editFavourites();
    void manageCovers();
    UBMainWindow *mWindow;
    UBLessonHistory *mHistory;
    QList<QAction*> mFavouriteActions;
};
