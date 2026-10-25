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




#include "UBPreferencesController.h"
#include "board/UBPaperInk.h"
#include "board/UBLessonTools.h"
#include "core/UBShortcutManager.h"
#include "gui/UBMainWindow.h"
#include <QtWidgets>

#include "UBSettings.h"
#include "UBApplication.h"

#include "gui/UBCircleFrame.h"

#include "core/UBSetting.h"
#include "core/UBApplicationController.h"
#include "core/UBDisplayManager.h"

#include "frameworks/UBStringUtils.h"

#include "board/UBBoardController.h"
#include "document/UBDocumentController.h"
#include "domain/UBGraphicsScene.h"
#include "board/UBDrawingController.h"
#include "podcast/UBPodcastController.h"

#include "ui_preferences.h"

#include "core/memcheck.h"


qreal UBPreferencesController::sSliderRatio = 10.0;
qreal UBPreferencesController::sMinPenWidth = 0.5;
qreal UBPreferencesController::sMaxPenWidth = 50.0;


UBPreferencesDialog::UBPreferencesDialog(UBPreferencesController* prefController, QWidget* parent,Qt::WindowFlags f)
    :QDialog(parent,f)
    ,mPreferencesController(prefController)
{
}

UBPreferencesDialog::~UBPreferencesDialog()
{
}

void UBPreferencesDialog::closeEvent(QCloseEvent* e)
{
    mPreferencesController->savePendingFields();
    e->accept();
}



UBPreferencesController::UBPreferencesController(QWidget *parent)
    : QObject(parent)
    , mPreferencesWindow(0)
    , mPreferencesUI(0)
    , mPenProperties(0)
    , mMarkerProperties(0)
    , mDarkBackgroundGridColorPicker(0)
    , mLightBackgroundGridColorPicker(0)
{
    mPreferencesWindow = new UBPreferencesDialog(this,parent, Qt::Dialog);
    mPreferencesUI = new Ui::preferencesDialog();  // deleted in destructor
    mPreferencesUI->setupUi(mPreferencesWindow);
    createDocumentLocationTab();
    adjustScreensPreferences();

    connect(UBApplication::displayManager, &UBDisplayManager::availableScreenCountChanged, this, &UBPreferencesController::adjustScreensPreferences);
    wire();
    rebuildPreferences();
    connect(mPreferencesWindow, &QDialog::finished, this, &UBPreferencesController::savePendingFields);
}


UBPreferencesController::~UBPreferencesController()
{
    delete mPreferencesWindow;

    delete mPreferencesUI;

    delete mPenProperties;

    delete mMarkerProperties;
}

void UBPreferencesController::adjustScreensPreferences()
{
    bool enabled = UBApplication::displayManager->numScreens() > 1;
    mPreferencesUI->multiDisplayGroupBox->setEnabled(enabled);

    if (enabled)
    {
        mPreferencesUI->screenList->setPlaceholderText(tr("Use all available displays"));
    }
    else
    {
        mPreferencesUI->screenList->setPlaceholderText("");
    }

    auto availableScreens = UBApplication::displayManager->availableScreens();
    QStringList screenNames;
    static QRegularExpression specialChars("[^a-zA-Z0-9]+");

    for (QScreen* screen : availableScreens)
    {
        screenNames << screen->name().replace(specialChars, "-");
    }

    QString screenConfiguration = screenNames.join('_');

    QString path = UBSettings::settings()->appScreenList->path() + "-" + screenConfiguration;

    if (path != mScreenConfigurationPath)
    {
        mScreenConfigurationPath = path;
        QVariant value = UBSettings::settings()->value(path);
        UBSettings::settings()->appScreenList->set(value);
        mPreferencesUI->screenList->loadScreenList(value.toStringList());
    }
}

void UBPreferencesController::show()
{
    init();
    refreshDocumentLocation();
    refreshWorkspacePreferences();
    if (auto shortcuts = mPreferencesWindow->findChild<QSortFilterProxyModel*>("preferencesShortcutProxy")) shortcuts->invalidate();

    mPreferencesWindow->exec();
}

void UBPreferencesController::wire()
{
    UBSettings* settings = UBSettings::settings();

    // main tab
    mPreferencesUI->mainTabWidget->setCurrentWidget(mPreferencesUI->displayTab);
    mPreferencesUI->versionLabel->setText(tr("version: ") + UBApplication::applicationVersion());

    connect(mPreferencesUI->closeButton, SIGNAL(released()), this, SLOT(close()));
    connect(mPreferencesUI->defaultSettingsButton, SIGNAL(released()), this, SLOT(defaultSettings()));

    connect(mPreferencesUI->screenList, &UBScreenListLineEdit::screenListChanged, this, [this,settings](const QStringList screenList){
        settings->appScreenList->set(screenList);

        if (!mScreenConfigurationPath.isEmpty())
        {
            settings->setValue(mScreenConfigurationPath, screenList);
        }
    });

    // OSK preferences

    for(int i = 0; i < settings->supportedKeyboardSizes->size(); i++)
        mPreferencesUI->keyboardPaletteKeyButtonSize->addItem(settings->supportedKeyboardSizes->at(i));


    connect(mPreferencesUI->useSystemOSKCheckBox, SIGNAL(clicked(bool)), settings->useSystemOnScreenKeyboard, SLOT(setBool(bool)));
    connect(mPreferencesUI->useSystemOSKCheckBox, SIGNAL(clicked(bool)), this, SLOT(systemOSKCheckBoxToggled(bool)));

    // PDF preferences
    connect(mPreferencesUI->exportBackgroundGrid, SIGNAL(clicked(bool)), settings->exportBackgroundGrid, SLOT(setBool(bool)));
    connect(mPreferencesUI->exportBackgroundColor, SIGNAL(clicked(bool)), settings->exportBackgroundColor, SLOT(setBool(bool)));

    // Documents Mode preferences
    connect(mPreferencesUI->showDateColumnOnAlphabeticalSort, SIGNAL(clicked(bool)), settings->showDateColumnOnAlphabeticalSort, SLOT(setBool(bool)));
    connect(mPreferencesUI->showDateColumnOnAlphabeticalSort, SIGNAL(clicked(bool)), UBApplication::documentController, SLOT(refreshDateColumns()));
    connect(mPreferencesUI->emptyTrashForOlderDocuments, SIGNAL(clicked(bool)), settings->emptyTrashForOlderDocuments, SLOT(setBool(bool)));
    connect(mPreferencesUI->emptyTrashDaysValue, SIGNAL(valueChanged(int)), settings->emptyTrashDaysValue,  SLOT(setInt(int)));


    connect(mPreferencesUI->keyboardPaletteKeyButtonSize, qOverload<int>(&QComboBox::currentIndexChanged), settings->boardKeyboardPaletteKeyBtnSize, [=](int index) {
        settings->boardKeyboardPaletteKeyBtnSize->setString(mPreferencesUI->keyboardPaletteKeyButtonSize->itemText(index));
    });
    connect(mPreferencesUI->startModeComboBox, SIGNAL(currentIndexChanged(int)), settings->appStartMode, SLOT(setInt(int)));

    connect(mPreferencesUI->useExternalBrowserCheckBox, SIGNAL(clicked(bool)), settings->webUseExternalBrowser, SLOT(setBool(bool)));
    connect(mPreferencesUI->displayBrowserPageCheckBox, SIGNAL(clicked(bool)), settings->webShowPageImmediatelyOnMirroredScreen, SLOT(setBool(bool)));

    connect(mPreferencesUI->toolbarAtTopRadioButton, SIGNAL(clicked(bool)), this, SLOT(toolbarPositionChanged(bool)));
    connect(mPreferencesUI->toolbarAtBottomRadioButton, SIGNAL(clicked(bool)), this, SLOT(toolbarPositionChanged(bool)));
    connect(mPreferencesUI->horizontalChoice, SIGNAL(clicked(bool)), this, SLOT(toolbarOrientationHorizontal(bool)));
    connect(mPreferencesUI->verticalChoice, SIGNAL(clicked(bool)), this, SLOT(toolbarOrientationVertical(bool)));
    connect(mPreferencesUI->toolbarDisplayTextCheckBox, SIGNAL(clicked(bool)), settings->appToolBarDisplayText, SLOT(setBool(bool)));

    //grid tab
    //On light background
    QPalette lightBackgroundPalette = QApplication::palette();
    lightBackgroundPalette.setColor(QPalette::Window, Qt::white);

    mPreferencesUI->crossColorLightBackgroundFrame->setAutoFillBackground(true);
    mPreferencesUI->crossColorLightBackgroundFrame->setPalette(lightBackgroundPalette);

    QPalette darkBackgroundPalette = QApplication::palette();
    darkBackgroundPalette.setColor(QPalette::Window, Qt::black);
    darkBackgroundPalette.setColor(QPalette::ButtonText, Qt::white);
    darkBackgroundPalette.setColor(QPalette::WindowText, Qt::white);

    mPreferencesUI->crossColorDarkBackgroundFrame->setAutoFillBackground(true);
    mPreferencesUI->crossColorDarkBackgroundFrame->setPalette(darkBackgroundPalette);
    mPreferencesUI->crossColorDarkBackgroundLabel->setPalette(darkBackgroundPalette);

    QList<QColor> gridLightBackgroundColors = settings->boardGridLightBackgroundColors->colors();
    QColor selectedCrossColorLightBackground(settings->boardCrossColorLightBackground->get().toString());

    mLightBackgroundGridColorPicker = new UBColorPicker(mPreferencesUI->crossColorLightBackgroundFrame);
    mLightBackgroundGridColorPicker->setObjectName(QString::fromUtf8("crossColorLightBackgroundFrame"));
    mLightBackgroundGridColorPicker->setMinimumSize(QSize(32, 32));
    mLightBackgroundGridColorPicker->setFrameShape(QFrame::StyledPanel);
    mLightBackgroundGridColorPicker->setFrameShadow(QFrame::Raised);

    mPreferencesUI->crossColorLightBackgroundLayout->addWidget(mLightBackgroundGridColorPicker);
    mLightBackgroundGridColorPicker->setColors(gridLightBackgroundColors);
    mLightBackgroundGridColorPicker->setSelectedColorIndex(gridLightBackgroundColors.indexOf(selectedCrossColorLightBackground));
    mPreferencesUI->lightBackgroundOpacitySlider->setValue(selectedCrossColorLightBackground.alpha()*100 / 255);

    QObject::connect(mLightBackgroundGridColorPicker, SIGNAL(colorSelected(const QColor&)), this, SLOT(setCrossColorOnLightBackground(const QColor&)));
    connect(mPreferencesUI->lightBackgroundOpacitySlider, SIGNAL(valueChanged(int)), this, SLOT(lightBackgroundCrossOpacityValueChanged(int)));

    //On dark background
    QList<QColor> gridDarkBackgroundColors = settings->boardGridDarkBackgroundColors->colors();
    QColor selectedCrossColorDarkBackground(settings->boardCrossColorDarkBackground->get().toString());

    mDarkBackgroundGridColorPicker = new UBColorPicker(mPreferencesUI->crossColorDarkBackgroundFrame);
    mDarkBackgroundGridColorPicker->setObjectName(QString::fromUtf8("crossColorDarkBackgroundFrame"));
    mDarkBackgroundGridColorPicker->setMinimumSize(QSize(32, 32));
    mDarkBackgroundGridColorPicker->setFrameShape(QFrame::StyledPanel);
    mDarkBackgroundGridColorPicker->setFrameShadow(QFrame::Raised);

    mPreferencesUI->crossColorDarkBackgroundLayout->addWidget(mDarkBackgroundGridColorPicker);
    mDarkBackgroundGridColorPicker->setColors(gridDarkBackgroundColors);
    mDarkBackgroundGridColorPicker->setSelectedColorIndex(gridDarkBackgroundColors.indexOf(selectedCrossColorDarkBackground));
    mPreferencesUI->darkBackgroundOpacitySlider->setValue(selectedCrossColorDarkBackground.alpha()*100 / 255);

    QObject::connect(mDarkBackgroundGridColorPicker, SIGNAL(colorSelected(const QColor&)), this, SLOT(setCrossColorOnDarkBackground(const QColor&)));
    connect(mPreferencesUI->darkBackgroundOpacitySlider, SIGNAL(valueChanged(int)), this, SLOT(darkBackgroundCrossOpacityValueChanged(int)));

    // pen
    QList<QColor> penLightBackgroundColors = settings->boardPenLightBackgroundColors->colors();
    QList<QColor> penDarkBackgroundColors = settings->boardPenDarkBackgroundColors->colors();
    QList<QColor> penLightBackgroundSelectedColors = settings->boardPenLightBackgroundSelectedColors->colors();
    QList<QColor> penDarkBackgroundSelectedColors = settings->boardPenDarkBackgroundSelectedColors->colors();

    mPenProperties = new UBBrushPropertiesFrame(mPreferencesUI->penFrame,
                                                penLightBackgroundColors, penDarkBackgroundColors, penLightBackgroundSelectedColors,
                                                penDarkBackgroundSelectedColors, this);

    mPenProperties->opacityFrame->hide();

    connect(mPenProperties->fineSlider, SIGNAL(valueChanged(int)), this, SLOT(widthSliderChanged(int)));
    connect(mPenProperties->mediumSlider, SIGNAL(valueChanged(int)), this, SLOT(widthSliderChanged(int)));
    connect(mPenProperties->strongSlider, SIGNAL(valueChanged(int)), this, SLOT(widthSliderChanged(int)));
    connect(mPenProperties->pressureSensitiveCheckBox, SIGNAL(clicked(bool)), settings, SLOT(setPenPressureSensitive(bool)));
    connect(mPenProperties->circleCheckBox, SIGNAL(clicked(bool)), settings, SLOT(setPenPreviewCircle(bool)));
    connect(mPenProperties->circleSpinBox, SIGNAL(valueChanged(int)), this, SLOT(penPreviewFromSizeChanged(int)));

    // marker
    QList<QColor> markerLightBackgroundColors = settings->boardMarkerLightBackgroundColors->colors();
    QList<QColor> markerDarkBackgroundColors = settings->boardMarkerDarkBackgroundColors->colors();
    QList<QColor> markerLightBackgroundSelectedColors = settings->boardMarkerLightBackgroundSelectedColors->colors();
    QList<QColor> markerDarkBackgroundSelectedColors = settings->boardMarkerDarkBackgroundSelectedColors->colors();

    mMarkerProperties = new UBBrushPropertiesFrame(mPreferencesUI->markerFrame, markerLightBackgroundColors,
                                                   markerDarkBackgroundColors, markerLightBackgroundSelectedColors,
                                                   markerDarkBackgroundSelectedColors, this);

    mMarkerProperties->pressureSensitiveCheckBox->setText(tr("Marker is pressure sensitive"));

    mMarkerProperties->circleFrame->hide();

    connect(mMarkerProperties->fineSlider, SIGNAL(valueChanged(int)), this, SLOT(widthSliderChanged(int)));
    connect(mMarkerProperties->mediumSlider, SIGNAL(valueChanged(int)), this, SLOT(widthSliderChanged(int)));
    connect(mMarkerProperties->strongSlider, SIGNAL(valueChanged(int)), this, SLOT(widthSliderChanged(int)));
    connect(mMarkerProperties->pressureSensitiveCheckBox, SIGNAL(clicked(bool)), settings, SLOT(setMarkerPressureSensitive(bool)));
    connect(mMarkerProperties->opacitySlider, SIGNAL(valueChanged(int)), this, SLOT(opacitySliderChanged(int)));

    for (auto edit : {mPreferencesUI->webHomePage, mPreferencesUI->proxyUsername, mPreferencesUI->proxyPassword})
        connect(edit, &QLineEdit::editingFinished, this, &UBPreferencesController::savePendingFields);

    // about tab
    connect(mPreferencesUI->checkSoftwareUpdateAtLaunchCheckBox, SIGNAL(clicked(bool)), settings->appEnableAutomaticSoftwareUpdates, SLOT(setBool(bool)));
}

void UBPreferencesController::init()
{
    UBSettings* settings = UBSettings::settings();

    // about tab
    mPreferencesUI->checkSoftwareUpdateAtLaunchCheckBox->setChecked(settings->appEnableAutomaticSoftwareUpdates->get().toBool());

    // display tab
    for(int i=0; i<mPreferencesUI->keyboardPaletteKeyButtonSize->count(); i++)
        if (mPreferencesUI->keyboardPaletteKeyButtonSize->itemText(i) == settings->boardKeyboardPaletteKeyBtnSize->get().toString()) {
            mPreferencesUI->keyboardPaletteKeyButtonSize->setCurrentIndex(i);
            break;
        }

    mPreferencesUI->useSystemOSKCheckBox->setChecked(settings->useSystemOnScreenKeyboard->get().toBool());
    this->systemOSKCheckBoxToggled(mPreferencesUI->useSystemOSKCheckBox->isChecked());

    mPreferencesUI->exportBackgroundGrid->setChecked(settings->exportBackgroundGrid->get().toBool());
    mPreferencesUI->exportBackgroundColor->setChecked(settings->exportBackgroundColor->get().toBool());

    mPreferencesUI->showDateColumnOnAlphabeticalSort->setChecked(settings->showDateColumnOnAlphabeticalSort->get().toBool());
    mPreferencesUI->emptyTrashForOlderDocuments->setChecked(settings->emptyTrashForOlderDocuments->get().toBool());
    mPreferencesUI->emptyTrashDaysValue->setValue(settings->emptyTrashDaysValue->get().toInt());

    mPreferencesUI->startModeComboBox->setCurrentIndex(settings->appStartMode->get().toInt());

    mPreferencesUI->useExternalBrowserCheckBox->setChecked(settings->webUseExternalBrowser->get().toBool());
    mPreferencesUI->displayBrowserPageCheckBox->setChecked(settings->webShowPageImmediatelyOnMirroredScreen->get().toBool());
    mPreferencesUI->screenList->loadScreenList(settings->appScreenList->get().toStringList());
    mPreferencesUI->webHomePage->setText(settings->webHomePage->get().toString());

    mPreferencesUI->proxyUsername->setText(settings->proxyUsername());
    mPreferencesUI->proxyPassword->setText(settings->proxyPassword());

    mPreferencesUI->toolbarAtTopRadioButton->setChecked(settings->appToolBarPositionedAtTop->get().toBool());
    mPreferencesUI->toolbarAtBottomRadioButton->setChecked(!settings->appToolBarPositionedAtTop->get().toBool());
    mPreferencesUI->toolbarDisplayTextCheckBox->setChecked(settings->appToolBarDisplayText->get().toBool());
    mPreferencesUI->verticalChoice->setChecked(settings->appToolBarOrientationVertical->get().toBool());
    mPreferencesUI->horizontalChoice->setChecked(!settings->appToolBarOrientationVertical->get().toBool());

    // pen tab
    mPenProperties->fineSlider->setValue(settings->boardPenFineWidth->get().toDouble() * sSliderRatio);
    mPenProperties->mediumSlider->setValue(settings->boardPenMediumWidth->get().toDouble() * sSliderRatio);
    mPenProperties->strongSlider->setValue(settings->boardPenStrongWidth->get().toDouble() * sSliderRatio);
    mPenProperties->pressureSensitiveCheckBox->setChecked(settings->boardPenPressureSensitive->get().toBool());
    mPenProperties->circleCheckBox->setChecked(settings->showPenPreviewCircle->get().toBool());
    mPenProperties->circleSpinBox->setValue(settings->penPreviewFromSize->get().toInt());

    // marker tab
    mMarkerProperties->fineSlider->setValue(settings->boardMarkerFineWidth->get().toDouble() * sSliderRatio);
    mMarkerProperties->mediumSlider->setValue(settings->boardMarkerMediumWidth->get().toDouble() * sSliderRatio);
    mMarkerProperties->strongSlider->setValue(settings->boardMarkerStrongWidth->get().toDouble() * sSliderRatio);
    mMarkerProperties->pressureSensitiveCheckBox->setChecked(settings->boardMarkerPressureSensitive->get().toBool());

    mMarkerProperties->opacitySlider->setValue(settings->boardMarkerAlpha->get().toDouble() * 100);

}

void UBPreferencesController::savePendingFields()
{
    //web
    QString homePage = mPreferencesUI->webHomePage->text();

    UBSettings::settings()->webHomePage->set(homePage);
    UBSettings::settings()->setProxyUsername(mPreferencesUI->proxyUsername->text());
    UBSettings::settings()->setProxyPassword(mPreferencesUI->proxyPassword->text());

    UBSettings::settings()->save();
}

void UBPreferencesController::close()
{
    savePendingFields();
    mPreferencesWindow->accept();
}

void UBPreferencesController::defaultSettings()
{
    auto settings = UBSettings::settings();
    const auto section = mPreferencesUI->mainTabWidget->currentWidget()->property("preferenceSection").toString();
    QWidget *brushPage = mWritingTabs->currentIndex() == 0 ? mPreferencesUI->penTab : mPreferencesUI->markerTab;
    if (QMessageBox::question(mPreferencesWindow, tr("Reset this section"),
        tr("Restore the settings in this section to their defaults? Your lessons, recovery copies, document folder and other sections will stay as they are."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    if (section == "workspace") {
        settings->setValue("Teaching/TouchFriendly", false);
        settings->setValue("Teaching/ToolLabels", true);
        settings->setValue("Teaching/PaperInkPages", true);
        settings->appStartMode->reset();
        refreshWorkspacePreferences(); applyWorkspacePreferences();
    }
    else if (section == "documents") {
        settings->showDateColumnOnAlphabeticalSort->reset();
        settings->emptyTrashForOlderDocuments->reset(); settings->emptyTrashDaysValue->reset();
        UBApplication::documentController->refreshDateColumns();
    }
    else if (section == "keyboard") {
        settings->useSystemOnScreenKeyboard->reset(); settings->boardKeyboardPaletteKeyBtnSize->reset();
    }
    else if (section == "projector") {
        settings->appScreenList->reset();
        if (!mScreenConfigurationPath.isEmpty()) settings->setValue(mScreenConfigurationPath,settings->appScreenList->get());
        mPreferencesUI->screenList->setDefault(); settings->webShowPageImmediatelyOnMirroredScreen->reset();
    }
    else if (section == "web") {
        settings->webUseExternalBrowser->reset(); settings->webHomePage->reset();
    }
    else if (section == "writing" && brushPage == mPreferencesUI->penTab)
    {

        mPenProperties->fineSlider->setValue(settings->boardPenFineWidth->reset().toDouble() * sSliderRatio);
        mPenProperties->mediumSlider->setValue(settings->boardPenMediumWidth->reset().toDouble() * sSliderRatio);
        mPenProperties->strongSlider->setValue(settings->boardPenStrongWidth->reset().toDouble() * sSliderRatio);
        mPenProperties->pressureSensitiveCheckBox->setChecked(settings->boardPenPressureSensitive->reset().toBool());
        mPenProperties->circleCheckBox->setChecked(settings->showPenPreviewCircle->reset().toBool());
        mPenProperties->circleSpinBox->setValue(settings->penPreviewFromSize->reset().toInt());

        settings->boardPenLightBackgroundSelectedColors->reset();
        QList<QColor> lightBackgroundSelectedColors = settings->boardPenLightBackgroundSelectedColors->colors();

        settings->boardPenDarkBackgroundSelectedColors->reset();
        QList<QColor> darkBackgroundSelectedColors = settings->boardPenDarkBackgroundSelectedColors->colors();

        for (int i = 0 ; i < settings->colorPaletteSize ; i++)
        {
            mPenProperties->lightBackgroundColorPickers[i]->setSelectedColorIndex(lightBackgroundSelectedColors.indexOf(settings->penColors(false).at(i)));
            mPenProperties->darkBackgroundColorPickers[i]->setSelectedColorIndex(darkBackgroundSelectedColors.indexOf(settings->penColors(true).at(i)));
        }
    }
    else if (section == "writing" && brushPage == mPreferencesUI->markerTab)
    {
        mMarkerProperties->fineSlider->setValue(settings->boardMarkerFineWidth->reset().toDouble() * sSliderRatio);
        mMarkerProperties->mediumSlider->setValue(settings->boardMarkerMediumWidth->reset().toDouble() * sSliderRatio);
        mMarkerProperties->strongSlider->setValue(settings->boardMarkerStrongWidth->reset().toDouble() * sSliderRatio);
        mMarkerProperties->pressureSensitiveCheckBox->setChecked(settings->boardMarkerPressureSensitive->reset().toBool());

        mMarkerProperties->opacitySlider->setValue(settings->boardMarkerAlpha->reset().toDouble() * 100);

        settings->boardMarkerLightBackgroundSelectedColors->reset();
        QList<QColor> lightBackgroundSelectedColors = settings->boardMarkerLightBackgroundSelectedColors->colors();

        settings->boardMarkerDarkBackgroundSelectedColors->reset();
        QList<QColor> darkBackgroundSelectedColors = settings->boardMarkerDarkBackgroundSelectedColors->colors();

        for (int i = 0 ; i < settings->colorPaletteSize ; i++)
        {
            mMarkerProperties->lightBackgroundColorPickers[i]->setSelectedColorIndex(lightBackgroundSelectedColors.indexOf(settings->markerColors(false).at(i)));
            mMarkerProperties->darkBackgroundColorPickers[i]->setSelectedColorIndex(darkBackgroundSelectedColors.indexOf(settings->markerColors(true).at(i)));
        }
    }
    else if(section == "page")
    {
        settings->boardCrossColorDarkBackground->reset();
        settings->boardGridDarkBackgroundColors->reset();

        QList<QColor> gridDarkBackgroundColors = settings->boardGridDarkBackgroundColors->colors();
        QColor selectedCrossColorDarkBackground(settings->boardCrossColorDarkBackground->get().toString());

        mDarkBackgroundGridColorPicker->setColors(gridDarkBackgroundColors);
        mDarkBackgroundGridColorPicker->setSelectedColorIndex(gridDarkBackgroundColors.indexOf(selectedCrossColorDarkBackground));
        int darkBackgroundOpacity = selectedCrossColorDarkBackground.alpha()*100 / 255;
        mPreferencesUI->darkBackgroundOpacitySlider->setValue(darkBackgroundOpacity);
        darkBackgroundCrossOpacityValueChanged(darkBackgroundOpacity);

        settings->boardCrossColorLightBackground->reset();
        settings->boardGridLightBackgroundColors->reset();

        QList<QColor> gridLightBackgroundColors = settings->boardGridLightBackgroundColors->colors();
        QColor selectedCrossColorLightBackground(settings->boardCrossColorLightBackground->get().toString());

        mLightBackgroundGridColorPicker->setColors(gridLightBackgroundColors);
        mLightBackgroundGridColorPicker->setSelectedColorIndex(gridLightBackgroundColors.indexOf(selectedCrossColorLightBackground));
        int lightBackgroundOpacity = selectedCrossColorLightBackground.alpha()*100 / 255;
        mPreferencesUI->lightBackgroundOpacitySlider->setValue(lightBackgroundOpacity);
        lightBackgroundCrossOpacityValueChanged(lightBackgroundOpacity);

    }
    if (section == "page") { settings->exportBackgroundGrid->reset(); settings->exportBackgroundColor->reset(); }
    init(); refreshDocumentLocation(); settings->save();
}

void UBPreferencesController::darkBackgroundCrossOpacityValueChanged(int value)
{
    UBSettings* settings = UBSettings::settings();
    int opacity = value * 255 / 100;

    QList<QColor> gridDarkBackgroundColors = settings->boardGridDarkBackgroundColors->colors();

    int index = mDarkBackgroundGridColorPicker->selectedColorIndex();
    if (index < 0 || index >= gridDarkBackgroundColors.size()) return;
    QColor currentColor = gridDarkBackgroundColors.at(index);
    currentColor.setAlpha(opacity);
    gridDarkBackgroundColors.replace(index, currentColor);
    mDarkBackgroundGridColorPicker->setColors(gridDarkBackgroundColors);

    settings->boardGridDarkBackgroundColors->setColor(index, currentColor);

    UBSettings::settings()->boardCrossColorDarkBackground->set(currentColor.name(QColor::HexArgb));

    if (UBApplication::boardController && UBApplication::boardController->activeScene())
    {
        foreach(QGraphicsView* view, UBApplication::boardController->activeScene()->views())
            view->resetCachedContent();
    }
}

void UBPreferencesController::lightBackgroundCrossOpacityValueChanged(int value)
{
    UBSettings* settings = UBSettings::settings();
    int opacity = value * 255 / 100;

    QList<QColor> gridLightBackgroundColors = settings->boardGridLightBackgroundColors->colors();

    int index = mLightBackgroundGridColorPicker->selectedColorIndex();
    if (index < 0 || index >= gridLightBackgroundColors.size()) return;
    QColor currentColor = gridLightBackgroundColors.at(index);
    currentColor.setAlpha(opacity);
    gridLightBackgroundColors.replace(index, currentColor);
    mLightBackgroundGridColorPicker->setColors(gridLightBackgroundColors);

    settings->boardGridLightBackgroundColors->setColor(index, currentColor);

    UBSettings::settings()->boardCrossColorLightBackground->set(currentColor.name(QColor::HexArgb));

    if (UBApplication::boardController && UBApplication::boardController->activeScene())
    {
        foreach(QGraphicsView* view, UBApplication::boardController->activeScene()->views())
            view->resetCachedContent();
    }
}

void UBPreferencesController::penPreviewFromSizeChanged(int value)
{
    UBSettings::settings()->setPenPreviewFromSize(value);
}

void UBPreferencesController::widthSliderChanged(int value)
{
    UBSettings* settings = UBSettings::settings();

    qreal width = value / sSliderRatio;

    QObject *slider = sender();
    UBCircleFrame* display = 0;

    if (slider == mPenProperties->fineSlider)
    {
        settings->boardPenFineWidth->set(width);
        display = mPenProperties->fineDisplayFrame;
    }
    else if (slider == mPenProperties->mediumSlider)
    {
        settings->boardPenMediumWidth->set(width);
        display = mPenProperties->mediumDisplayFrame;
    }
    else if (slider == mPenProperties->strongSlider)
    {
        settings->boardPenStrongWidth->set(width);
        display = mPenProperties->strongDisplayFrame;
    }
    else if (slider == mMarkerProperties->fineSlider)
    {
        settings->boardMarkerFineWidth->set(width);
        display = mMarkerProperties->fineDisplayFrame;
    }
    else if (slider == mMarkerProperties->mediumSlider)
    {
        settings->boardMarkerMediumWidth->set(width);
        display = mMarkerProperties->mediumDisplayFrame;
    }
    else if (slider == mMarkerProperties->strongSlider)
    {
        settings->boardMarkerStrongWidth->set(width);
        display = mMarkerProperties->strongDisplayFrame;
    }
    else
    {
        qDebug() << "widthSliderChanged : unknown sender ... ignoring event";
    }

    if (display)
    {
        display->currentPenWidth = width;
        display->maxPenWidth = sMaxPenWidth;
        display->repaint();
    }
}


void UBPreferencesController::opacitySliderChanged(int value)
{
    qreal opacity = ((qreal)value) / 100;

    QObject *slider = sender();

    if (slider == mMarkerProperties->opacitySlider)
    {
        UBDrawingController::drawingController()->setMarkerAlpha(opacity);
    }
}


void UBPreferencesController::colorSelected(const QColor& color)
{
    UBColorPicker *colorPicker = qobject_cast<UBColorPicker*>(sender());

    int index = mPenProperties->lightBackgroundColorPickers.indexOf(colorPicker);

    if (index >= 0)
    {
        UBDrawingController::drawingController()->setPenColor(false, color, index);
        return;
    }

    index = mPenProperties->darkBackgroundColorPickers.indexOf(colorPicker);

    if (index >= 0)
    {
        UBDrawingController::drawingController()->setPenColor(true, color, index);
    }

    index = mMarkerProperties->lightBackgroundColorPickers.indexOf(colorPicker);

    if (index >= 0)
    {
        UBDrawingController::drawingController()->setMarkerColor(false, color, index);
    }

    index = mMarkerProperties->darkBackgroundColorPickers.indexOf(colorPicker);

    if (index >= 0)
    {
        UBDrawingController::drawingController()->setMarkerColor(true, color, index);
    }

}

void UBPreferencesController::setCrossColorOnDarkBackground(const QColor& color)
{
    UBSettings::settings()->boardCrossColorDarkBackground->set(color.name(QColor::HexArgb));

    mPreferencesUI->darkBackgroundOpacitySlider->setValue(color.alpha() * 100 / 255);

    if (UBApplication::boardController && UBApplication::boardController->activeScene())
    {
        foreach(QGraphicsView* view, UBApplication::boardController->activeScene()->views())
            view->resetCachedContent();
    }
}

void UBPreferencesController::setCrossColorOnLightBackground(const QColor& color)
{
    UBSettings::settings()->boardCrossColorLightBackground->set(color.name(QColor::HexArgb));

    mPreferencesUI->lightBackgroundOpacitySlider->setValue(color.alpha() * 100 / 255);

    if (UBApplication::boardController && UBApplication::boardController->activeScene())
    {
        foreach(QGraphicsView* view, UBApplication::boardController->activeScene()->views())
            view->resetCachedContent();
    }
}

void UBPreferencesController::toolbarPositionChanged(bool checked)
{
    Q_UNUSED(checked);

    UBSettings* settings = UBSettings::settings();

    settings->appToolBarPositionedAtTop->set(mPreferencesUI->toolbarAtTopRadioButton->isChecked());

}

void UBPreferencesController::toolbarOrientationVertical(bool checked)
{
    UBSettings* settings = UBSettings::settings();
    settings->appToolBarOrientationVertical->set(checked);
}

void UBPreferencesController::toolbarOrientationHorizontal(bool checked)
{
    UBSettings* settings = UBSettings::settings();
    settings->appToolBarOrientationVertical->set(!checked);
}

void UBPreferencesController::systemOSKCheckBoxToggled(bool checked)
{
    mPreferencesUI->keyboardPaletteKeyButtonSize->setVisible(!checked);
    mPreferencesUI->keyboardPaletteKeyButtonSize_Label->setVisible(!checked);
}

UBBrushPropertiesFrame::UBBrushPropertiesFrame(QFrame* owner, const QList<QColor>& lightBackgroundColors,
                                               const QList<QColor>& darkBackgroundColors, const QList<QColor>& lightBackgroundSelectedColors,
                                               const QList<QColor>& darkBackgroundSelectedColors, UBPreferencesController* controller)
{
    setupUi(owner);

    QPalette lightBackgroundPalette = QApplication::palette();
    lightBackgroundPalette.setColor(QPalette::Window, Qt::white);

    lightBackgroundFrame->setAutoFillBackground(true);
    lightBackgroundFrame->setPalette(lightBackgroundPalette);

    QPalette darkBackgroundPalette = QApplication::palette();
    darkBackgroundPalette.setColor(QPalette::Window, Qt::black);
    darkBackgroundPalette.setColor(QPalette::ButtonText, Qt::white);
    darkBackgroundPalette.setColor(QPalette::WindowText, Qt::white);

    darkBackgroundFrame->setAutoFillBackground(true);
    darkBackgroundFrame->setPalette(darkBackgroundPalette);
    darkBackgroundLabel->setPalette(darkBackgroundPalette);

    QList<QColor> firstLightBackgroundColor;
    firstLightBackgroundColor.append(lightBackgroundColors[0]);

    lightBackgroundColorPicker0->setColors(firstLightBackgroundColor);
    lightBackgroundColorPicker0->setSelectedColorIndex(0);
    lightBackgroundColorPickers.append(lightBackgroundColorPicker0);

    for (int i = 1 ; i < UBSettings::settings()->colorPaletteSize ; i++)
    {
        UBColorPicker *picker = new UBColorPicker(lightBackgroundFrame);
        picker->setObjectName(QString("penLightBackgroundColor%1").arg(i));
        picker->setMinimumSize(QSize(32, 32));
        picker->setFrameShape(QFrame::StyledPanel);
        picker->setFrameShadow(QFrame::Raised);

        lightBackgroundLayout->addWidget(picker);

        picker->setColors(lightBackgroundColors);

        picker->setSelectedColorIndex(lightBackgroundColors.indexOf(lightBackgroundSelectedColors.at(i)));

        lightBackgroundColorPickers.append(picker);

        QObject::connect(picker, SIGNAL(colorSelected(const QColor&)), controller, SLOT(colorSelected(const QColor&)));

    }

    QList<QColor> firstDarkBackgroundColor;
    firstDarkBackgroundColor.append(darkBackgroundColors[0]);

    darkBackgroundColorPicker0->setColors(firstDarkBackgroundColor);
    darkBackgroundColorPicker0->setSelectedColorIndex(0);
    darkBackgroundColorPickers.append(darkBackgroundColorPicker0);

    for (int i = 1 ; i < UBSettings::settings()->colorPaletteSize ; i++)
    {
        UBColorPicker *picker = new UBColorPicker(darkBackgroundFrame);
        picker->setObjectName(QString("penDarkBackgroundColor%1").arg(i));
        picker->setMinimumSize(QSize(32, 32));
        picker->setFrameShape(QFrame::StyledPanel);
        picker->setFrameShadow(QFrame::Raised);

        darkBackgroundLayout->addWidget(picker);

        picker->setColors(darkBackgroundColors);
        picker->setSelectedColorIndex(darkBackgroundColors.indexOf(darkBackgroundSelectedColors.at(i)));

        darkBackgroundColorPickers.append(picker);

        QObject::connect(picker, SIGNAL(colorSelected(const QColor&)), controller, SLOT(colorSelected(const QColor&)));

    }
}

UBScreenListLineEdit::UBScreenListLineEdit(QWidget *parent)
    : QLineEdit(parent)
    , mValidator(nullptr)
{
    connect(this, &QLineEdit::textChanged, this, &UBScreenListLineEdit::onTextChanged);
}

void UBScreenListLineEdit::setDefault()
{
    setText("");
}

void UBScreenListLineEdit::loadScreenList(const QStringList &screenList)
{
    setText(screenList.join(','));
}

void UBScreenListLineEdit::focusInEvent(QFocusEvent *focusEvent)
{
    QLineEdit::focusInEvent(focusEvent);

    if (mScreenLabels.empty())
    {
        QStringList screenList = UBStringUtils::trimmed(text().split(','));

        QList<QScreen*> screens = UBApplication::displayManager->availableScreens();
        QStringList availableScreenIndexes;
        int screenIndex = 1;
        QFont font;
        font.setPointSize(48);

        for (QScreen* screen : screens)
        {
            QString index = QString::number(screenIndex);
            availableScreenIndexes << index;

            QPushButton* button = new QPushButton(this);
            button->setWindowFlag(Qt::FramelessWindowHint, true);
            button->setWindowFlag(Qt::WindowStaysOnTopHint, true);
            button->setWindowFlag(Qt::X11BypassWindowManagerHint, true);
            button->setWindowFlag(Qt::Window, true);
            button->setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);
            button->setAttribute(Qt::WA_ShowWithoutActivating, true);
            button->setProperty("screenIndex", index);
#ifdef QT_DEBUG
            button->setText(index + "(" + screen->name() + ")");
#else
            button->setText(index);
#endif
            button->setFont(font);
            button->move(screen->geometry().topLeft());
            button->setMinimumSize(300, 150);
            button->setDisabled(screenList.contains(index));
            button->setCursor(Qt::PointingHandCursor);
            button->show();

            connect(button, &QPushButton::pressed, this, &UBScreenListLineEdit::addScreen);

            mScreenLabels << button;
            ++screenIndex;
        }

        if (!mValidator)
        {
            mValidator = new UBStringListValidator(this);
            setValidator(mValidator);
        }

        mValidator->setValidationStringList(availableScreenIndexes);
    }
}

void UBScreenListLineEdit::focusOutEvent(QFocusEvent *focusEvent)
{
    QLineEdit::focusOutEvent(focusEvent);
    qDeleteAll(mScreenLabels);
    mScreenLabels.clear();
}

void UBScreenListLineEdit::addScreen()
{
    QPushButton* button = dynamic_cast<QPushButton*>(sender());

    if (button)
    {
        QString list = text();
        mValidator->fixup(list);
        QString screenIndex = button->property("screenIndex").toString();

        if (list.isEmpty())
        {
            setText(screenIndex);
        }
        else
        {
            setText(list + "," + screenIndex);
        }

        button->setEnabled(false);
    }
}

void UBScreenListLineEdit::onTextChanged(const QString &input)
{
    const QStringList screenList = UBStringUtils::trimmed(input.split(','));

    for (QPushButton* button : std::as_const(mScreenLabels))
    {
        button->setDisabled(screenList.contains(button->property("screenIndex").toString()));
    }

    if (input.isEmpty() || input.right(1) == ',')
    {
        // create and attach a new QCompleter
        QStringList model;

        for (QPushButton* button : std::as_const(mScreenLabels))
        {
            if (button->isEnabled())
            {
                model << input + button->property("screenIndex").toString();
            }
        }
    }

    // user indication of acceptable input
    if (hasAcceptableInput())
    {
        setStyleSheet("");
        emit screenListChanged(screenList);
    }
    else
    {
        setStyleSheet("QLineEdit { background-color: #FFB3C8; }");
    }
}

UBStringListValidator::UBStringListValidator(QObject *parent)
    : QValidator(parent)
{

}

void UBStringListValidator::fixup(QString &input) const
{
    // remove invalid tokens from list, trim tokens
    QStringList inputList = UBStringUtils::trimmed(input.split(','));
    QStringList outputList;

    for (const QString& token : inputList)
    {
        if (mList.contains(token))
        {
            outputList << token;
        }
    }

    input = outputList.join(',');
}

QValidator::State UBStringListValidator::validate(QString &input, int &) const
{
    bool ok = true;
    QStringList inputList = UBStringUtils::trimmed(input.split(','));
    // number of commas must match number of list items - 1
    int commas = input.count(',');

    if (commas && commas + 1 != inputList.size())
    {
        ok = false;
    }

    for (const QString& token : inputList)
    {
        ok &= mList.contains(token) && inputList.count(token) == 1;
    }

    return ok ? Acceptable : Intermediate;
}

void UBStringListValidator::setValidationStringList(const QStringList &list)
{
    mList = list;
}

void UBPreferencesController::refreshWorkspacePreferences()
{
    if (!mTouchControls) return;
    const QSignalBlocker touch(mTouchControls), labels(mToolLabels), pages(mShowPages);
    auto settings = UBSettings::settings();
    mTouchControls->setChecked(settings->value("Teaching/TouchFriendly", false).toBool());
    mToolLabels->setChecked(settings->value("Teaching/ToolLabels", true).toBool());
    mShowPages->setChecked(settings->value("Teaching/PaperInkPages", true).toBool());
}

void UBPreferencesController::applyWorkspacePreferences()
{
    auto settings = UBSettings::settings();
    settings->setValue("Teaching/TouchFriendly", mTouchControls->isChecked());
    settings->setValue("Teaching/ToolLabels", mToolLabels->isChecked());
    settings->setValue("Teaching/PaperInkPages", mShowPages->isChecked());
    settings->save();
    if (auto paper = UBApplication::mainWindow->findChild<UBPaperInk*>()) paper->applyPreferences();
    if (auto tools = UBApplication::mainWindow->findChild<UBLessonTools*>()) tools->applyTouchMode();
}

void UBPreferencesController::rebuildPreferences()
{
    auto tabs = mPreferencesUI->mainTabWidget;
    auto documents = mPreferencesWindow->findChild<QWidget*>("documentsStorageTab");
    // Reuse the original controls and connections, including colour palettes,
    // document migration, screen selection and complete third-party notices.
    while (tabs->count()) {
        auto page = tabs->widget(0); tabs->removeTab(0);
        page->setParent(mPreferencesWindow); page->hide();
    }
    auto contentLayout = new QHBoxLayout;
    auto sidebar = new QWidget;
    auto sideLayout = new QVBoxLayout(sidebar); sideLayout->setContentsMargins(0,0,0,0);
    auto search = new QLineEdit; search->setObjectName("preferencesSearch");
    search->setPlaceholderText(tr("Find a setting…")); search->setClearButtonEnabled(true);
    sideLayout->addWidget(search);
    mNavigation = new QListWidget; mNavigation->setObjectName("preferencesNavigation");
    mNavigation->setMinimumWidth(180); mNavigation->setMaximumWidth(215);
    mNavigation->setSpacing(3); sideLayout->addWidget(mNavigation,1);
    contentLayout->addWidget(sidebar); contentLayout->addWidget(tabs,1);
    auto content = new QWidget; content->setLayout(contentLayout);
    mPreferencesUI->gridLayout->removeWidget(tabs);
    mPreferencesUI->gridLayout->addWidget(content,1,0);
    tabs->tabBar()->hide(); tabs->setDocumentMode(true);
    auto note = [](const QString &text) {
        auto label = new QLabel(text); label->setWordWrap(true);
        label->setTextFormat(Qt::PlainText); label->setProperty("preferenceNote",true);
        return label;
    };
    auto page = [&](const QString &title, const QString &description) {
        auto widget = new QWidget; auto layout = new QVBoxLayout(widget);
        layout->setContentsMargins(20,14,20,18); layout->setSpacing(16);
        auto heading = new QLabel(title); auto font = heading->font();font.setPointSize(18);font.setBold(true);heading->setFont(font);
        layout->addWidget(heading); layout->addWidget(note(description)); return widget;
    };
    auto addSection = [&](QWidget *widget, const QString &id, const QString &title, const QString &keywords) {
        auto scroll = new QScrollArea; scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(widget); scroll->setProperty("preferenceSection",id);
        scroll->setObjectName("preferencesSection-"+id); tabs->addTab(scroll,title);
        auto item = new QListWidgetItem(title,mNavigation);item->setData(Qt::UserRole,id);
        item->setData(Qt::UserRole+1,title+" "+keywords);item->setSizeHint(QSize(170,42));
    };
    auto workspace = page(tr("Workspace"),tr("Choose how MathScrap feels while you teach. Changes take effect immediately."));
    auto workspaceLayout = qobject_cast<QVBoxLayout*>(workspace->layout());
    auto appearance = new QGroupBox(tr("Floating toolbar and pages"));auto appearanceLayout = new QVBoxLayout(appearance);
    mTouchControls = new QCheckBox(tr("Larger controls for touch and interactive boards"));mTouchControls->setObjectName("preferencesTouchControls");
    mToolLabels = new QCheckBox(tr("Show tool names on the floating toolbar"));mToolLabels->setObjectName("preferencesToolLabels");
    mShowPages = new QCheckBox(tr("Show the Pages sidebar"));mShowPages->setObjectName("preferencesShowPages");
    for(auto check : {mTouchControls,mToolLabels,mShowPages}) {
        appearanceLayout->addWidget(check);connect(check,&QCheckBox::toggled,this,[this]{applyWorkspacePreferences();});
    }
    auto customize = new QPushButton(tr("Customize workspace…"));customize->setObjectName("preferencesCustomizeWorkspace");
    appearanceLayout->addWidget(customize,0,Qt::AlignLeft);
    appearanceLayout->addWidget(note(tr("Choose which tools appear, rearrange them, and preview the floating toolbar. Add optional tools through Customize Workspace. The cog in the top toolbar opens Settings & more.")));
    connect(customize,&QPushButton::clicked,this,[this]{
        if(auto action=UBApplication::mainWindow->findChild<QAction*>("actionTeachingCustomize")) action->trigger();
        refreshWorkspacePreferences();
    });
    workspaceLayout->addWidget(appearance);workspaceLayout->addWidget(mPreferencesUI->modeGroupBox);
    mPreferencesUI->modeGroupBox->setTitle(tr("When MathScrap starts"));mPreferencesUI->modeLabel->setText(tr("Start in:"));
    mPreferencesUI->startModeComboBox->setItemText(0,tr("Lesson"));
    workspaceLayout->addWidget(note(tr("Line: double-click to switch solid/dashed. Zoom: double-click to switch in/out; click the active Zoom tool again to return to Select.")));
    workspaceLayout->addStretch();addSection(workspace,"workspace",tr("Workspace"),tr("toolbar touch labels pages startup customize"));

    auto writing = page(tr("Writing & colours"),tr("Set your pen and highlighter palettes, pressure response and starting widths. Each colour remembers its chosen thickness separately for pen, highlighter, solid line and dashed line."));
    mWritingTabs = new QTabWidget;mWritingTabs->setObjectName("preferencesWritingTabs");
    mWritingTabs->addTab(mPreferencesUI->penTab,tr("Pen"));mWritingTabs->addTab(mPreferencesUI->markerTab,tr("Highlighter"));
    mMarkerProperties->pressureSensitiveCheckBox->setText(tr("Use pen pressure for the highlighter"));
    mPenProperties->pressureSensitiveCheckBox->setText(tr("Use pen pressure for the pen"));
    qobject_cast<QVBoxLayout*>(writing->layout())->addWidget(mWritingTabs);
    addSection(writing,"writing",tr("Writing & colours"),tr("pen highlighter marker pressure thickness width palette colour color"));

    auto paper = page(tr("Page & PDF"),tr("Choose grid colours for light and dark pages, and decide what appears in exported PDFs."));
    auto paperLayout = qobject_cast<QVBoxLayout*>(paper->layout());
    paperLayout->addWidget(mPreferencesUI->gridTab);mPreferencesUI->gridTab->show();
    mPreferencesUI->PDFGroupBox->setTitle(tr("PDF appearance"));paperLayout->addWidget(mPreferencesUI->PDFGroupBox);
    mPreferencesUI->exportBackgroundGrid->setText(tr("Include the page grid in PDF exports"));
    mPreferencesUI->exportBackgroundColor->setText(tr("Include the page background colour in PDF exports"));
    paperLayout->addWidget(note(tr("To export only some pages, select their thumbnails in Documents before choosing Export to PDF.")));
    paperLayout->addStretch();addSection(paper,"page",tr("Page & PDF"),tr("grid background opacity export print PDF"));

    auto docPage = page(tr("Documents & recovery"),tr("Choose where lessons are stored and how they appear in Documents."));
    auto docLayout = qobject_cast<QVBoxLayout*>(docPage->layout());
    docLayout->addWidget(documents);documents->show();
    mPreferencesUI->documentsGroupBox->setTitle(tr("Document list and Trash"));docLayout->addWidget(mPreferencesUI->documentsGroupBox);
    mPreferencesUI->showDateColumnOnAlphabeticalSort->setText(tr("Show the date column when sorting by name"));
    mPreferencesUI->emptyTrashForOlderDocuments->setText(tr("Automatically remove Trash items older than"));
    mPreferencesUI->emptyTrashDaysValue->setEnabled(mPreferencesUI->emptyTrashForOlderDocuments->isChecked());
    connect(mPreferencesUI->emptyTrashForOlderDocuments,&QCheckBox::toggled,mPreferencesUI->emptyTrashDaysValue,&QWidget::setEnabled);
    auto recovery = new QGroupBox(tr("Recovery and saved versions"));auto recoveryLayout = new QVBoxLayout(recovery);
    recoveryLayout->addWidget(note(tr("Recovery saves every 20 seconds while you are not drawing. Changed lessons keep versions every 5 minutes, on exit, and when you choose Save version. Restoring opens a separate copy.")));
    auto versions = new QPushButton(tr("Open lesson versions…"));versions->setObjectName("preferencesOpenVersions");recoveryLayout->addWidget(versions,0,Qt::AlignLeft);
    connect(versions,&QPushButton::clicked,this,[]{if(auto action=UBApplication::mainWindow->findChild<QAction*>("actionLessonHistory"))action->trigger();});
    docLayout->addWidget(recovery);docLayout->addStretch();addSection(docPage,"documents",tr("Documents"),tr("folder storage move browse recovery versions trash date"));

    auto keyboard = page(tr("Keyboard & shortcuts"),tr("Choose your on-screen keyboard and personalize keyboard shortcuts. Select a command below to change its shortcut."));
    auto keyboardLayout = qobject_cast<QVBoxLayout*>(keyboard->layout());keyboardLayout->addWidget(mPreferencesUI->keyboardGroupBox);
    auto filter = new QLineEdit;filter->setObjectName("preferencesShortcutSearch");filter->setPlaceholderText(tr("Find a command…"));filter->setClearButtonEnabled(true);keyboardLayout->addWidget(filter);
    auto proxy = new QSortFilterProxyModel(keyboard);proxy->setObjectName("preferencesShortcutProxy");proxy->setSourceModel(UBShortcutManager::shortcutManager());proxy->setFilterKeyColumn(-1);proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    auto shortcuts = new QTableView;shortcuts->setObjectName("preferencesShortcuts");shortcuts->setModel(proxy);
    shortcuts->setSelectionBehavior(QAbstractItemView::SelectRows);shortcuts->setSelectionMode(QAbstractItemView::SingleSelection);shortcuts->setEditTriggers(QAbstractItemView::NoEditTriggers);
    shortcuts->setColumnHidden(1,true);shortcuts->setColumnHidden(3,true);shortcuts->setColumnHidden(4,true);shortcuts->verticalHeader()->hide();shortcuts->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);shortcuts->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);shortcuts->setMinimumHeight(220);keyboardLayout->addWidget(shortcuts,1);
    connect(filter,&QLineEdit::textChanged,proxy,&QSortFilterProxyModel::setFilterFixedString);
    auto editShortcut = new QPushButton(tr("Change selected shortcut…"));editShortcut->setObjectName("preferencesEditShortcut");editShortcut->setEnabled(false);keyboardLayout->addWidget(editShortcut,0,Qt::AlignLeft);
    connect(shortcuts->selectionModel(),&QItemSelectionModel::selectionChanged,this,[=]{auto index=proxy->mapToSource(shortcuts->currentIndex());editShortcut->setEnabled(index.data(UBShortcutManager::ActionRole).toBool());});
    connect(editShortcut,&QPushButton::clicked,this,[=]{
        const QPersistentModelIndex index(proxy->mapToSource(shortcuts->currentIndex()).siblingAtColumn(2));
        if(!index.isValid()||!index.data(UBShortcutManager::ActionRole).toBool())return;
        QDialog dialog(mPreferencesWindow);dialog.setObjectName("preferencesShortcutEditor");dialog.setWindowTitle(tr("Change shortcut"));auto layout=new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel(index.sibling(index.row(),0).data().toString().remove('&')));
        auto key=new QKeySequenceEdit(QKeySequence(index.data(UBShortcutManager::PrimaryShortcutRole).toString()));key->setObjectName("preferencesShortcutKey");layout->addWidget(key);
        auto message=note(tr("Press the new shortcut. Clear leaves the command without a keyboard shortcut."));layout->addWidget(message);
        auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);auto clear=buttons->addButton(tr("Clear"),QDialogButtonBox::ResetRole);layout->addWidget(buttons);
        connect(clear,&QPushButton::clicked,key,&QKeySequenceEdit::clear);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
            const auto chosen=key->keySequence();auto model=UBShortcutManager::shortcutManager();
            if(!chosen.isEmpty())for(int row=0;row<model->rowCount();++row){if(row==index.row())continue;const auto other=model->index(row,2);for(const auto &text:other.data(UBShortcutManager::ShortcutsRole).toStringList()){
                const QKeySequence existing(text);if(chosen.matches(existing)!=QKeySequence::NoMatch||existing.matches(chosen)!=QKeySequence::NoMatch){message->setText(tr("This shortcut conflicts with %1. Choose another combination.").arg(other.siblingAtColumn(0).data().toString().remove('&')));return;}
            }}
            model->setData(index,chosen.toString());UBSettings::settings()->save();dialog.accept();
        });dialog.exec();
    });
    keyboardLayout->addWidget(note(tr("Reset keyboard options affects only the on-screen keyboard. Shortcut bindings are changed individually. Saved mouse and tablet-button assignments are preserved. Single-letter drawing shortcuts are ignored while typing in text and formula fields.")));
    addSection(keyboard,"keyboard",tr("Keyboard"),tr("shortcut keys virtual keyboard tablet commands"));

    auto projector=page(tr("Display & projector"),tr("Select the screens used for the lesson and presentation views."));auto projectorLayout=qobject_cast<QVBoxLayout*>(projector->layout());
    projectorLayout->addWidget(mPreferencesUI->multiDisplayGroupBox);
    projectorLayout->addWidget(note(tr("For both Lesson and GraphDesk on a projector, use Duplicate displays in Windows. The separate extended-display lesson view does not mirror the GraphDesk tab.")));
    projectorLayout->addWidget(note(tr("Screen selection becomes available when more than one display is connected.")));projectorLayout->addStretch();
    addSection(projector,"projector",tr("Projector"),tr("display monitor screen HDMI duplicate presentation"));
    auto web=page(tr("Web & network"),tr("Choose how web pages open and provide proxy credentials if your school network requires them."));auto webLayout=qobject_cast<QVBoxLayout*>(web->layout());
    webLayout->addWidget(mPreferencesUI->internetGroupBox);webLayout->addStretch();
    mPreferencesUI->useExternalBrowserCheckBox->setText(tr("Open web pages in the system browser"));mPreferencesUI->labelHomePage->setText(tr("Browser home page:"));mPreferencesUI->label->setText(tr("Proxy username:"));mPreferencesUI->label_3->setText(tr("Proxy password:"));
    addSection(web,"web",tr("Web & network"),tr("internet browser home proxy password network"));

    auto about=page(tr("About MathScrap"),tr("A mathematics teaching workspace built on OpenBoard, Open-Sankoré and Uniboard."));auto aboutLayout=qobject_cast<QVBoxLayout*>(about->layout());
    aboutLayout->addWidget(new QLabel(tr("Application version: %1").arg(UBApplication::applicationVersion())));
    auto release=new QPushButton(tr("MathScrap releases and downloads"));release->setObjectName("preferencesReleases");aboutLayout->addWidget(release,0,Qt::AlignLeft);
    connect(release,&QPushButton::clicked,this,[]{UBApplication::mainWindow->actionCheckUpdate->trigger();});
    auto notices=new QTabWidget;notices->setObjectName("preferencesAboutTabs");notices->addTab(mPreferencesUI->aboutTab,tr("Project"));notices->addTab(mPreferencesUI->thirdPartyLicence,tr("Licences"));notices->addTab(mPreferencesUI->creditsTab,tr("Credits"));
    mPreferencesUI->softwareUpdateGroupBox->hide();aboutLayout->addWidget(notices,1);
    addSection(about,"about",tr("About"),tr("version updates releases licence credits copyright"));
    auto empty=note(tr("No matching settings sections."));empty->hide();contentLayout->addWidget(empty);
    auto refreshResetButton=[=]{
        const int row=mNavigation->currentRow();
        const QStringList titles={tr("Reset workspace options…"),mWritingTabs->currentIndex()==0?tr("Reset pen options…"):tr("Reset highlighter options…"),tr("Reset page & PDF options…"),tr("Reset document options…"),tr("Reset keyboard options…"),tr("Reset display options…"),tr("Reset browser options…"),tr("Reset this section…")};
        if(row>=0&&row<titles.size())mPreferencesUI->defaultSettingsButton->setText(titles[row]);
    };
    connect(mNavigation,&QListWidget::currentRowChanged,this,[=](int row){if(row<0)return;tabs->setCurrentIndex(row);mPreferencesUI->defaultSettingsButton->setEnabled(row!=7);refreshResetButton();});
    connect(mWritingTabs,&QTabWidget::currentChanged,this,[=]{refreshResetButton();});
    connect(search,&QLineEdit::textChanged,this,[=](const QString &text){
        int first=-1;for(int i=0;i<mNavigation->count();++i){auto item=mNavigation->item(i);bool match=item->data(Qt::UserRole+1).toString().contains(text,Qt::CaseInsensitive);item->setHidden(!match);if(match&&first<0)first=i;}
        tabs->setVisible(first>=0);empty->setVisible(first<0);if(first>=0&&(mNavigation->currentRow()<0||mNavigation->currentItem()->isHidden()))mNavigation->setCurrentRow(first);
        mPreferencesUI->defaultSettingsButton->setEnabled(first>=0&&mNavigation->currentRow()!=7);
    });
    mPreferencesUI->toolbarGroupBox->hide();mPreferencesUI->stylusPaletteGroupBox->hide();
    mPreferencesUI->versionLabel->setText(tr("MathScrap Preferences"));auto headingFont=mPreferencesUI->versionLabel->font();headingFont.setPointSize(18);headingFont.setBold(true);mPreferencesUI->versionLabel->setFont(headingFont);
    auto saved=note(tr("Changes are saved automatically. Folder changes take effect on the next launch."));mPreferencesUI->horizontalLayout_2->insertWidget(1,saved,1);
    mPreferencesUI->defaultSettingsButton->setText(tr("Reset this section…"));mPreferencesUI->closeButton->setText(tr("Done"));
    mPreferencesWindow->setWindowTitle(tr("MathScrap Preferences"));mPreferencesWindow->setMinimumSize(800,560);
    const auto available=mPreferencesWindow->screen()->availableGeometry().size();mPreferencesWindow->resize(qMin(1060,available.width()-40),qMin(760,available.height()-50));
    mPreferencesWindow->setStyleSheet(QStringLiteral("QDialog#preferencesDialog{background:#f5f1e7;color:#263f41;} QScrollArea{border:0;} QGroupBox{border:1px solid #d8d5cb;border-radius:8px;margin-top:12px;padding:16px 12px 10px;font-weight:600;} QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 5px;} QListWidget#preferencesNavigation{background:#ece8dd;border:0;border-radius:8px;} QListWidget#preferencesNavigation::item{padding:4px 8px;border-radius:6px;color:#263f41;} QListWidget#preferencesNavigation::item:selected{background:#d7e7e2;color:#174a47;} QPushButton{padding:7px 12px;} QLineEdit{padding:5px;} QCheckBox{spacing:8px;padding:3px 0;}"));
    mNavigation->setCurrentRow(0);refreshWorkspacePreferences();
}

void UBPreferencesController::refreshDocumentLocation()
{
    auto settings = UBSettings::settings();
    const QString pending = settings->pendingDocumentDirectory();
    mDocumentLocation->setText(QDir::toNativeSeparators(pending.isEmpty() ? UBSettings::userDocumentDirectory() : pending));
    mDocumentLocationNote->setText(pending.isEmpty()
        ? tr("MathScrap stores your lessons in this folder.")
        : tr("Current location: %1. The new location will be used next time you start MathScrap.")
            .arg(QDir::toNativeSeparators(UBSettings::userDocumentDirectory())));
    mCancelDocumentLocation->setEnabled(!pending.isEmpty());
}

void UBPreferencesController::createDocumentLocationTab()
{
    auto tab = new QWidget;
    tab->setObjectName("documentsStorageTab");
    auto layout = new QVBoxLayout(tab);
    auto group = new QGroupBox(tr("My Documents location"));
    auto form = new QVBoxLayout(group);
    auto row = new QHBoxLayout;
    mDocumentLocation = new QLineEdit;
    mDocumentLocation->setObjectName("documentStoragePath");
    mDocumentLocation->setReadOnly(true);
    auto browse = new QPushButton(tr("Browse…"));
    browse->setObjectName("browseDocumentStorage");
    row->addWidget(mDocumentLocation, 1); row->addWidget(browse);
    form->addLayout(row);
    mDocumentLocationNote = new QLabel;
    mDocumentLocationNote->setWordWrap(true);
    form->addWidget(mDocumentLocationNote);
    mCancelDocumentLocation = new QPushButton(tr("Cancel pending folder change"));
    form->addWidget(mCancelDocumentLocation);
    layout->addWidget(group);
    auto help = new QLabel(tr("Choose a dedicated folder for MathScrap lessons. You can move your existing documents to an empty folder, or use an empty/existing OpenBoard folder without moving them. Changes take effect on the next launch, after your current work has been saved."));
    help->setWordWrap(true); layout->addWidget(help); layout->addStretch();
    mPreferencesUI->mainTabWidget->addTab(tab, tr("Documents"));
    connect(mCancelDocumentLocation, &QPushButton::clicked, this, [this] {
        UBSettings::settings()->cancelDocumentDirectoryChange();
        refreshDocumentLocation();
    });
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString selected = QFileDialog::getExistingDirectory(mPreferencesWindow, tr("Choose My Documents location"), mDocumentLocation->text());
        if (selected.isEmpty()) return;
        QMessageBox choice(mPreferencesWindow);
        choice.setWindowTitle(tr("Change My Documents location"));
        choice.setText(tr("How should MathScrap use %1?").arg(QDir::toNativeSeparators(selected)));
        choice.setInformativeText(tr("Move existing documents requires an empty destination. Using the folder without moving leaves your current documents in their original folder. The change takes effect on the next launch."));
        auto move = choice.addButton(tr("Move existing documents"), QMessageBox::AcceptRole);
        auto use = choice.addButton(tr("Use folder without moving"), QMessageBox::ActionRole);
        choice.addButton(QMessageBox::Cancel);
        choice.exec();
        if (choice.clickedButton() != move && choice.clickedButton() != use) return;
        QString error;
        if (!UBSettings::settings()->scheduleDocumentDirectoryChange(selected, choice.clickedButton() == move, &error))
            QMessageBox::warning(mPreferencesWindow, tr("Cannot change documents folder"), error);
        refreshDocumentLocation();
    });
    refreshDocumentLocation();
}
