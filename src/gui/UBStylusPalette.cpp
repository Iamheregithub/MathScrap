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




#include "UBStylusPalette.h"

#include <QtGui>
#include <QLayout>

#include "UBMainWindow.h"

#include "core/UBApplication.h"
#include "core/UBSettings.h"
#include "core/UBApplicationController.h"
#include "core/UBShortcutManager.h"


#include "board/UBDrawingController.h"

#include "frameworks/UBPlatformUtils.h"

#include "core/memcheck.h"

UBStylusPalette::UBStylusPalette(QWidget *parent, Qt::Orientation orient)
    : UBActionPalette(Qt::TopLeftCorner, parent, orient)
    , mLastSelectedId(-1)
{
    QList<QAction*> actions;

    actions << UBApplication::mainWindow->actionPen;
    actions << UBApplication::mainWindow->actionEraser;
    actions << UBApplication::mainWindow->actionMarker;
    actions << UBApplication::mainWindow->actionSelector;
    actions << UBApplication::mainWindow->actionPlay;

    actions << UBApplication::mainWindow->actionHand;
    actions << UBApplication::mainWindow->actionZoomIn;
    actions << UBApplication::mainWindow->actionZoomOut;

    actions << UBApplication::mainWindow->actionPointer;
    actions << UBApplication::mainWindow->actionLine;
    if (auto dash = UBApplication::mainWindow->findChild<QAction*>("actionTeachingDash")) actions << dash;
    actions << UBApplication::mainWindow->actionText;
    actions << UBApplication::mainWindow->actionCapture;

    if(UBPlatformUtils::hasVirtualKeyboard())
    {
        actions << UBApplication::mainWindow->actionVirtualKeyboard;
        UBApplication::mainWindow->actionVirtualKeyboard->setProperty("ungrouped", true);
    }

    actions << UBApplication::mainWindow->actionSnap;
    UBApplication::mainWindow->actionSnap->setProperty("ungrouped", true);

    for (auto action : actions)
        action->setProperty("teachingHiddenBottom", !UBSettings::settings()->value("Teaching/Bottom/" + action->objectName(), action != UBApplication::mainWindow->actionPointer).toBool());
    setActions(actions);
    setButtonIconSize(QSize(42, 42));
    groupActions();

    UBShortcutManager::shortcutManager()->addActionGroup(mActionGroup);

    adjustSizeAndPosition();

    applyTeachingLayout(UBSettings::settings()->value("Teaching/BottomOrder").toStringList());
    initPosition();

    foreach(const UBActionPaletteButton* button, mButtons)
    {
        connect(button, SIGNAL(doubleClicked()), this, SLOT(stylusToolDoubleClicked()));
    }

}

void UBStylusPalette::initPosition()
{
    QWidget* pParentW = parentWidget();
    if(!pParentW) return ;

    mCustomPosition = true;

    QPoint pos;
    int parentWidth = pParentW->width();
    int parentHeight = pParentW->height();

    if(UBSettings::settings()->appToolBarOrientationVertical->get().toBool()){
        int posX = border();
        int posY = (parentHeight / 2) - (height() / 2);
        pos.setX(posX);
        pos.setY(posY);
    }
    else {
        int posX = (parentWidth / 2) - (width() / 2);
        int posY = parentHeight - border() - height();
        pos.setX(posX);
        pos.setY(posY);
    }
    moveInsideParent(pos);
}

UBStylusPalette::~UBStylusPalette()
{
    if (mActionGroup)
    {
        UBShortcutManager::shortcutManager()->removeActionGroup(mActionGroup);
    }
}

void UBStylusPalette::stylusToolDoubleClicked()
{
    auto action = mActionGroup->checkedAction();
    if (!action) action = UBApplication::mainWindow->actionLine;
    emit stylusToolDoubleClicked(action->property("id").toInt());
}



void UBStylusPalette::applyTeachingLayout(const QStringList &order)
{
    if (auto dash = UBApplication::mainWindow->findChild<QAction*>("actionTeachingDash"); dash && !mActions.contains(dash)) {
        addAction(dash);
        auto button = getButtonFromAction(dash);
        button->setIconSize(QSize(42,42));
        connect(button, SIGNAL(doubleClicked()), this, SLOT(stylusToolDoubleClicked()));
    }
    QStringList complete = order;
    for (auto action : mActions)
        if (!complete.contains(action->objectName())) complete << action->objectName();
    for (auto id : complete)
        for (auto action : mActions)
            if (action->objectName() == id) {
                auto button = getButtonFromAction(action);
                if (button) {
                    layout()->removeWidget(button);
                    layout()->addWidget(button);
                    button->setVisible(action->isVisible() && !action->property("teachingHiddenBottom").toBool());
                }
            }
    adjustSize();
    initPosition();
}
