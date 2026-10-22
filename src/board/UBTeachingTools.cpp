// SPDX-License-Identifier: GPL-3.0-only
#include "UBTeachingTools.h"
#include "UBLessonTools.h"
#include <QtWidgets>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDomDocument>
#include "domain/UBGraphicsItemDelegate.h"
#include "gui/UBMainWindow.h"
#include "gui/UBStylusPalette.h"
#include "core/UBApplication.h"
#include "core/UBApplicationController.h"
#include "core/UBSettings.h"
#include "core/UBShortcutManager.h"
#include "board/UBBoardController.h"
#include "board/UBBoardView.h"
#include "board/UBBoardPaletteManager.h"
#include "board/UBDrawingController.h"
#include "domain/UBGraphicsScene.h"
#include "domain/UBGraphicsSvgItem.h"

UBTeachingTools::UBTeachingTools(UBMainWindow *window) : QObject(window), mWindow(window)
{
    auto toolbar = window->boardToolBar;
    auto add = [this, toolbar](const QString &id, const QString &text, const QString &icon) {
        auto action = new QAction(QIcon(icon), text, mWindow);
        action->setObjectName(id);
        action->setToolTip(text);
        toolbar->insertAction(mWindow->actionBoard, action);
        mWindow->addAction(action);
        return action;
    };
    auto dash = add("actionTeachingDash", tr("Dashed Line"), ":/images/teaching/dash.svg");
    dash->setCheckable(true);
    dash->setAutoRepeat(false);
    dash->setShortcut(QKeySequence("Ctrl+Shift+L"));
    dash->setProperty("ungrouped", true);
    window->actionLine->setText(tr("Solid Line"));
    window->actionLine->setToolTip(tr("Draw a solid straight line"));
    auto syncLineChoices = [window, dash] {
        const bool line = UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Line;
        const bool dashed = UBSettings::settings()->value("Teaching/DashedLines", false).toBool();
        dash->setChecked(line && dashed);
        window->actionLine->setChecked(line && !dashed);
    };
    connect(dash, &QAction::triggered, this, [syncLineChoices] {
        UBSettings::settings()->setValue("Teaching/DashedLines", true);
        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Line);
        syncLineChoices();
    });
    connect(window->actionLine, &QAction::triggered, this, [syncLineChoices] {
        UBSettings::settings()->setValue("Teaching/DashedLines", false);
        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Line);
        syncLineChoices();
    });
    connect(UBDrawingController::drawingController(), &UBDrawingController::stylusToolChanged, this,
            [syncLineChoices](int, int) { syncLineChoices(); });
    syncLineChoices();
    auto shapes = add("actionTeachingShapes", tr("Shapes"), ":/images/teaching/shapes.svg");
    mShapes = shapes;
    shapes->setCheckable(true);
    shapes->setProperty("ungrouped", true);
    connect(shapes, &QAction::triggered, this, &UBTeachingTools::shapeDialog);
    auto graph = add("actionTeachingGraphDesk", tr("GraphDesk"), ":/images/teaching/graph.svg");
    connect(graph, &QAction::triggered, this, &UBTeachingTools::openGraphDesk);
    auto zoomIn = add("actionTeachingZoomIn", tr("Zoom +"), ":/images/stylusPalette/zoomIn.png");
    auto zoomOut = add("actionTeachingZoomOut", tr("Zoom −"), ":/images/stylusPalette/zoomOut.png");
    auto reset = add("actionTeachingZoomReset", tr("Reset zoom"), ":/images/stylusPalette/zoomOut.png");
    zoomIn->setShortcut(QKeySequence("Ctrl++"));
    zoomOut->setShortcut(QKeySequence("Ctrl+-"));
    reset->setShortcut(QKeySequence("Ctrl+0"));
    connect(zoomIn, &QAction::triggered, this, [] { UBApplication::boardController->zoomIn(); });
    connect(zoomOut, &QAction::triggered, this, [] { UBApplication::boardController->zoomOut(); });
    connect(reset, &QAction::triggered, this, [] { UBApplication::boardController->zoomRestore(); });
    UBShortcutManager::shortcutManager()->addActions(tr("Teaching"), {dash, shapes, graph, zoomIn, zoomOut, reset}, window);

    new UBLessonTools(window);

    // Keep real actions registered and visible for shortcuts. Only toolbar
    // presentation proxies are hidden; this also preserves disabled states.
    int group = 0;
    for (auto source : toolbar->actions()) {
        if (source->isSeparator()) {
            // Fixed upstream group dividers have no position in a user-ordered
            // toolbar. Leaving them in place gathers all five before Stylus.
            toolbar->removeAction(source);
            continue;
        }
        // Keep numbering stable for existing saved toolbar preferences, but
        // never expose the expansion spacer as an unnamed classroom control.
        if (source->objectName().isEmpty()) source->setObjectName(QString("teachingGroup%1").arg(group++));
        if (auto widgetAction = qobject_cast<QWidgetAction*>(source)) {
            auto widget = widgetAction->defaultWidget();
            if (widget && widget->property("teachingLabel").toString().isEmpty()
                && widget->sizePolicy().horizontalPolicy() == QSizePolicy::MinimumExpanding) {
                toolbar->removeAction(source);
                continue;
            }
        }
        const QString id = source->objectName();
        mDefaultTop << id;
        mTop[id] = source;
        if (qobject_cast<QWidgetAction*>(source)) continue;
        mWindow->addAction(source);
        auto proxy = new QAction(this);
        proxy->setMenu(source->menu());
        auto sync = [source, proxy] {
            proxy->setText(source->text());
            proxy->setIcon(source->icon());
            proxy->setToolTip(source->toolTip());
            proxy->setEnabled(source->isEnabled());
            proxy->setCheckable(source->isCheckable());
            proxy->setChecked(source->isChecked());
            proxy->setVisible(source->isVisible() && !source->property("teachingHiddenTop").toBool());
        };
        connect(source, &QAction::changed, proxy, sync);
        connect(proxy, &QAction::triggered, source, [source] { source->trigger(); });
        sync();
        QAction *presentation = proxy;
        // These native buttons carry menus or press/long-press behaviour on
        // the widget itself. Keep a stable owned widget when changing order.
        if (source == window->actionMenu || source == window->actionPages || source == window->actionErase) {
            auto oldButton = qobject_cast<QToolButton*>(toolbar->widgetForAction(source));
            auto button = new QToolButton;
            button->setObjectName("teaching-" + id);
            if (oldButton && !oldButton->objectName().isEmpty())
                button->setObjectName(oldButton->objectName());
            button->setDefaultAction(proxy);
            button->setAutoRaise(true);
            button->setIconSize(toolbar->iconSize());
            button->setToolButtonStyle(toolbar->toolButtonStyle());
            connect(toolbar, &QToolBar::iconSizeChanged, button, &QToolButton::setIconSize);
            connect(toolbar, &QToolBar::toolButtonStyleChanged, button, &QToolButton::setToolButtonStyle);
            if (oldButton && oldButton->menu()) {
                button->setMenu(oldButton->menu());
                button->setPopupMode(oldButton->popupMode());
            }
            auto palette = UBApplication::boardController->paletteManager();
            if (source == window->actionPages) {
                connect(button, SIGNAL(pressed()), palette, SLOT(pagePaletteButtonPressed()));
                connect(button, SIGNAL(released()), palette, SLOT(pagePaletteButtonReleased()));
            } else if (source == window->actionErase) {
                connect(button, SIGNAL(pressed()), palette, SLOT(erasePaletteButtonPressed()));
                connect(button, SIGNAL(released()), palette, SLOT(erasePaletteButtonReleased()));
            }
            auto wrapper = new QWidgetAction(this);
            wrapper->setObjectName("teaching-presentation-" + id);
            wrapper->setDefaultWidget(button);
            wrapper->setMenu(button->menu());
            wrapper->setText(source->text());
            wrapper->setIcon(source->icon());
            wrapper->setEnabled(source->isEnabled());
            connect(wrapper, &QAction::triggered, button, &QToolButton::click);
            connect(source, &QAction::changed, wrapper, [source, wrapper] {
                wrapper->setVisible(source->isVisible() && !source->property("teachingHiddenTop").toBool());
                wrapper->setText(source->text());
                wrapper->setIcon(source->icon());
                wrapper->setEnabled(source->isEnabled());
            });
            presentation = wrapper;
        }
        toolbar->insertAction(source, presentation);
        toolbar->removeAction(source);
        mProxies[source] = presentation;
    }
    const QList<QAction*> tools = {window->actionPen, window->actionEraser, window->actionMarker,
        window->actionSelector, window->actionPlay, window->actionHand, window->actionZoomIn,
        window->actionZoomOut, window->actionPointer, window->actionLine, dash, window->actionText,
        window->actionCapture, window->actionVirtualKeyboard, window->actionSnap};
    for (auto action : tools) {
        mBottom[action->objectName()] = action;
        mDefaultBottom << action->objectName();
        action->setAutoRepeat(false);
    }
    auto customizeAction = mCustomize = new QAction(tr("Customize toolbars…"), this);
    customizeAction->setToolTip(tr("Choose visible tools and drag to change their order"));
    customizeAction->setObjectName("actionTeachingCustomize");
    for (auto menu : window->findChildren<QMenu*>())
        if (menu->actions().contains(window->actionPreferences))
            menu->insertAction(window->actionPreferences, customizeAction);
    connect(customizeAction, &QAction::triggered, this, &UBTeachingTools::customize);
    toolbar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(toolbar, &QWidget::customContextMenuRequested, this, [this] { customize(); });
    createShapeToolbar();
    qApp->installEventFilter(this);
    applyLayout();
}

bool UBTeachingTools::eventFilter(QObject *watched, QEvent *event)
{
    auto board = UBApplication::boardController;
    auto view = board->controlView();
    if (mShapeDrawing && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        cancelShapeDrag(); mShapeDrawing = false; view->viewport()->unsetCursor(); return true;
    }
    if (mShapeDrawing && (watched == view->viewport() || (watched == view && (event->type()==QEvent::TabletPress || event->type()==QEvent::TabletMove || event->type()==QEvent::TabletRelease))) && mWindow->teachingTabs()->currentIndex() == 0) {
        const auto type = event->type();
        const bool tablet = type == QEvent::TabletPress || type == QEvent::TabletMove || type == QEvent::TabletRelease;
        const bool mouse = type == QEvent::MouseButtonPress || type == QEvent::MouseMove || type == QEvent::MouseButtonRelease;
        if (tablet || mouse) {
            QPointF position;
            bool press = type == QEvent::TabletPress || type == QEvent::MouseButtonPress;
            bool release = type == QEvent::TabletRelease || type == QEvent::MouseButtonRelease;
            if (tablet) {
                auto e = static_cast<QTabletEvent*>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                position = e->position();
#else
                position = e->posF();
#endif
            } else {
                auto e = static_cast<QMouseEvent*>(event);
                if ((press || release) && e->button() != Qt::LeftButton) return false;
                position = e->pos();
            }
            auto scene = board->activeScene();
            if (!scene) return false;
            const auto point = view->mapToScene(position.toPoint());
            if (press) {
                cancelShapeDrag(); mShapeTarget.clear(); scene->clearSelection();
                mShapeScene = scene.get(); mShapeStart = point; mShapeDragging = true;
                mShapePreview = new QGraphicsPathItem;
                mShapePreview->setZValue(1900000); scene->addItem(mShapePreview);
            }
            if (mShapeDragging && mShapeScene == scene.get()) {
                QPointF end = point;
                if (mShapeType == 1 || mShapeType == 2) {
                    const auto delta = point - mShapeStart;
                    const double size = qMax(qAbs(delta.x()), qAbs(delta.y()));
                    end = mShapeStart + QPointF(delta.x() < 0 ? -size : size, delta.y() < 0 ? -size : size);
                }
                const QRectF rect = QRectF(mShapeStart, end).normalized();
                mShapePreview->setPath(shapePath(rect));
                QPen pen(mNoBorder ? Qt::transparent : mShapeBorder, mShapeWidth->currentData().toDouble());
                pen.setJoinStyle(Qt::RoundJoin);
                if (mShapeStyle->currentIndex() == 1) pen.setDashPattern({4,3});
                mShapePreview->setPen(pen);
                auto fill = mShapeFill; fill.setAlphaF(mShapeOpacity->value()/100.0);
                mShapePreview->setBrush(mNoFill ? QBrush(Qt::NoBrush) : QBrush(fill));
                if (release) {
                    const auto svg = shapeSvg(rect);
                    cancelShapeDrag();
                    if (rect.width() >= 3 && rect.height() >= 3) {
                        auto item = scene->addSvg(QUrl(), rect.center(), svg);
                        // addSvg centres and scales imported SVGs; drawn geometry
                        // uses exact page dimensions and placement instead.
                        if (!item) { mShapeDrawing=false;view->viewport()->unsetCursor();return true; }
                        item->setScale(1); item->setTransform(QTransform());
                        item->setPos(rect.topLeft() - QPointF(12,12));
                        mShapeDrawing = false; view->viewport()->unsetCursor();
                        mWindow->actionSelector->trigger(); scene->clearSelection(); item->setSelected(true);
                    }
                }
            }
            event->accept(); return true;
        }
    }
    if (event->type() != QEvent::ShortcutOverride) return false;
    auto key = static_cast<QKeyEvent*>(event);
    if (key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) return false;
    bool editing = false;
    for (auto widget = QApplication::focusWidget(); widget; widget = widget->parentWidget()) {
        editing |= qobject_cast<QLineEdit*>(widget) || qobject_cast<QTextEdit*>(widget)
                || qobject_cast<QPlainTextEdit*>(widget) || qobject_cast<QAbstractSpinBox*>(widget)
                || qobject_cast<QWebEngineView*>(widget);
        if (auto view = qobject_cast<QGraphicsView*>(widget))
            editing |= view->scene() && dynamic_cast<QGraphicsTextItem*>(view->scene()->focusItem());
    }
    if (editing) { key->accept(); return true; }
    return false;
}

void UBTeachingTools::applyLayout()
{
    auto settings = UBSettings::settings();
    QStringList topOrder = settings->value("Teaching/TopOrder", mDefaultTop).toStringList();
    for (auto id : mDefaultTop) if (!topOrder.contains(id)) topOrder << id;
    for (auto id : topOrder) {
        auto source = mTop.value(id);
        if (!source) continue;
        const bool show = settings->value("Teaching/Top/" + id, true).toBool();
        source->setProperty("teachingHiddenTop", !show);
        auto display = mProxies.value(source, source);
        // Moving QWidgetActions via removeAction can delete their widgets.
        mWindow->boardToolBar->addAction(display);
        if (!mProxies.contains(source)) display->setVisible(show);
        if (mProxies.contains(source))
            display->setVisible(show && source->isVisible());
    }
    for (auto source : mBottom) {
        const bool show = settings->value("Teaching/Bottom/" + source->objectName(),
                                         source != mWindow->actionPointer).toBool();
        source->setProperty("teachingHiddenBottom", !show);
    }
    QStringList bottomOrder = settings->value("Teaching/BottomOrder", mDefaultBottom).toStringList();
    for (auto palette : mWindow->findChildren<UBStylusPalette*>())
        palette->applyTeachingLayout(bottomOrder);
}

void UBTeachingTools::customize()
{
    QDialog dialog(mWindow);
    dialog.setWindowTitle(tr("Customize toolbars"));
    dialog.resize(620, 560);
    auto layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Check tools to show them. Drag rows to rearrange them. Hidden tools retain their shortcuts.")));
    auto tabs = new QTabWidget;
    layout->addWidget(tabs);
    auto makeList = [&](const QString &label, const QString &prefix, const QMap<QString, QAction*> &actions,
                        const QStringList &defaults) {
        auto list = new QListWidget;
        list->setDragDropMode(QAbstractItemView::InternalMove);
        auto order = UBSettings::settings()->value("Teaching/" + prefix + "Order", defaults).toStringList();
        for (auto id : defaults) if (!order.contains(id)) order << id;
        for (auto id : order) {
            auto source = actions.value(id); if (!source) continue;
            QString text = source->text();
            if (text.isEmpty()) {
                auto widgetAction = qobject_cast<QWidgetAction*>(source);
                if (widgetAction && widgetAction->defaultWidget())
                    text = widgetAction->defaultWidget()->property("teachingLabel").toString();
            }
            if (text.isEmpty()) text = id;
            auto item = new QListWidgetItem(source->icon(), text, list);
            item->setData(Qt::UserRole, id);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(UBSettings::settings()->value("Teaching/" + prefix + "/" + id,
                prefix != "Bottom" || source != mWindow->actionPointer).toBool() ? Qt::Checked : Qt::Unchecked);
        }
        tabs->addTab(list, label);
        return list;
    };
    auto top = makeList(tr("Top toolbar"), "Top", mTop, mDefaultTop);
    auto bottom = makeList(tr("Pen toolbar"), "Bottom", mBottom, mDefaultBottom);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, &dialog, [&] {
        for (auto pair : {qMakePair(top, mDefaultTop), qMakePair(bottom, mDefaultBottom)}) {
            auto list = pair.first;
            for (int target = 0; target < pair.second.size(); ++target)
                for (int row = target; row < list->count(); ++row)
                    if (list->item(row)->data(Qt::UserRole).toString() == pair.second[target]) {
                        auto item = list->takeItem(row); list->insertItem(target, item);
                        item->setCheckState(pair.first == bottom && pair.second[target] == "actionPointer" ? Qt::Unchecked : Qt::Checked);
                        break;
                    }
        }
    });
    if (dialog.exec() != QDialog::Accepted) return;
    for (auto pair : {qMakePair(top, QString("Top")), qMakePair(bottom, QString("Bottom"))}) {
        QStringList order;
        for (int row = 0; row < pair.first->count(); ++row) {
            auto item = pair.first->item(row);
            auto id = item->data(Qt::UserRole).toString();
            order << id;
            UBSettings::settings()->setValue("Teaching/" + pair.second + "/" + id, item->checkState() == Qt::Checked);
        }
        UBSettings::settings()->setValue("Teaching/" + pair.second + "Order", order);
    }
    applyLayout();
}

void UBTeachingTools::openGraphDesk()
{
    if (UBApplication::applicationController->displayMode() != UBApplicationController::Board)
        UBApplication::applicationController->showBoard();
    if (!mGraph) {
        mGraphTab = new QWidget;
        auto layout = new QVBoxLayout(mGraphTab);
        layout->setContentsMargins(0,0,0,0);
        auto buttons = new QHBoxLayout;
        auto insert = new QPushButton(tr("Insert graph onto current lesson page"));
        auto edit = new QPushButton(tr("Load selected graph for editing"));
        auto update = new QPushButton(tr("Update loaded graph on lesson"));
        buttons->addWidget(insert); buttons->addWidget(edit); buttons->addWidget(update);
        connect(update, &QPushButton::clicked, this, [this] { writeGraph(true); });
        layout->addLayout(buttons);
        connect(edit, &QPushButton::clicked, this, &UBTeachingTools::editSelectedGraph);
        mGraph = new QWebEngineView;
        layout->addWidget(mGraph);
        connect(insert, &QPushButton::clicked, this, &UBTeachingTools::insertGraph);
        insert->setEnabled(false); edit->setEnabled(false); update->setEnabled(false);
        connect(mGraph, &QWebEngineView::loadFinished, this, [insert,edit,update](bool ok) {
            insert->setEnabled(ok); edit->setEnabled(ok); update->setEnabled(ok);
        });
        mGraph->load(QUrl("qrc:/graphdesk/GraphDesk.html"));
        mWindow->teachingTabs()->addTab(mGraphTab, tr("GraphDesk"));
    }
    mWindow->teachingTabs()->setCurrentWidget(mGraphTab);
}

void UBTeachingTools::insertGraph() { writeGraph(false); }

void UBTeachingTools::writeGraph(bool replace)
{
    if (!mGraph) return;
    QPointer<UBGraphicsSvgItem> target = replace ? mGraphTarget : nullptr;
    if (replace && !target) {
        QMessageBox::information(mWindow, tr("GraphDesk"), tr("Load a selected graph before updating it."));
        return;
    }
    auto destination = UBApplication::boardController->activeScene();
    QPointer<UBTeachingTools> guard(this);
    mGraph->page()->runJavaScript("JSON.stringify((()=>{validForExport(); return {svg:exportScene(scene(),'key').svg,project:project()}})())",
        [this,guard,target,destination,replace](const QVariant &value) {
            if (!guard) return;
            const auto data = QJsonDocument::fromJson(value.toString().toUtf8()).object();
            QByteArray svg = data.value("svg").toString().toUtf8();
            if (!svg.startsWith("<svg")) {
                QMessageBox::warning(mWindow, tr("GraphDesk"), tr("Fix the graph errors before inserting it."));
                return;
            }
            const QByteArray state = QJsonDocument(data.value("project").toObject()).toJson(QJsonDocument::Compact).toBase64();
            const int end = svg.indexOf('>');
            svg.insert(end+1, "<metadata id=\"graphdesk-project\">" + state + "</metadata>");
            auto board = UBApplication::boardController;
            if (!destination || board->activeScene() != destination) {
                QMessageBox::information(mWindow, tr("GraphDesk"), tr("The lesson page changed. Insert the graph again."));
                return;
            }
            if (replace) {
                if (!target || target->scene() != destination) {
                    QMessageBox::information(mWindow, tr("GraphDesk"), tr("The loaded graph is no longer on this lesson page."));
                    return;
                }
                const auto before = target->fileData();
                target->setTeachingSvg(svg); target->commitTeachingSvg(before);
            } else
                destination->addSvg(QUrl(), board->controlView()->mapToScene(board->controlView()->viewport()->rect().center()), svg);
            mWindow->teachingTabs()->setCurrentIndex(0);
            mWindow->actionSelector->trigger();
        });
}

QPainterPath UBTeachingTools::shapePath(const QRectF &r) const
{
    QPainterPath p;
    if (mShapeType <= 1) p.addRect(r);
    else if (mShapeType == 2 || mShapeType == 4) p.addEllipse(r);
    else {
        QPolygonF vertices;
        if (mShapeType == 3) vertices << QPointF(r.center().x(),r.top()) << r.bottomRight() << r.bottomLeft();
        else if (mShapeType == 5) vertices << r.topLeft() << r.bottomRight() << r.bottomLeft();
        else vertices << QPointF(r.center().x(),r.top()) << QPointF(r.right(),r.center().y())
                      << QPointF(r.center().x(),r.bottom()) << QPointF(r.left(),r.center().y());
        p.addPolygon(vertices); p.closeSubpath();
    }
    return p;
}

QByteArray UBTeachingTools::shapeSvg(const QRectF &r) const
{
    const double w=r.width(), h=r.height();
    QString element;
    if (mShapeType <= 1) element=QString("<rect x='12' y='12' width='%1' height='%2'/>").arg(w).arg(h);
    else if (mShapeType == 2) element=QString("<circle cx='%1' cy='%2' r='%3'/>").arg(12+w/2).arg(12+h/2).arg(w/2);
    else if (mShapeType == 4) element=QString("<ellipse cx='%1' cy='%2' rx='%3' ry='%4'/>").arg(12+w/2).arg(12+h/2).arg(w/2).arg(h/2);
    else {
        const auto polygon=shapePath(QRectF(12,12,w,h)).toFillPolygon();
        QStringList points;
        // toFillPolygon closes the path; omit its duplicated last vertex so
        // triangles keep exactly three independently draggable handles.
        for(int i=0;i<polygon.size()-1;++i) points << QString("%1,%2").arg(polygon[i].x()).arg(polygon[i].y());
        element=QString("<polygon points='%1'/>").arg(points.join(' '));
    }
    return QString("<svg xmlns='http://www.w3.org/2000/svg' width='%1' height='%2' viewBox='0 0 %1 %2'><metadata id='teaching-shape'>%3</metadata><g stroke='%4' fill='%5' stroke-width='%6' stroke-linejoin='round' fill-opacity='%7' stroke-dasharray='%8'>%9</g></svg>")
        .arg(w+24).arg(h+24).arg(mShapeType).arg(mNoBorder?"none":mShapeBorder.name(),mNoFill?"none":mShapeFill.name())
        .arg(mShapeWidth->currentData().toDouble()).arg(mShapeOpacity->value()/100.0)
        .arg(mShapeStyle->currentIndex()==1?QString("%1 %2").arg(mShapeWidth->currentData().toDouble()*4).arg(mShapeWidth->currentData().toDouble()*3):"none",element).toUtf8();
}

void UBTeachingTools::cancelShapeDrag()
{
    if (mShapePreview && mShapeScene) { mShapeScene->removeItem(mShapePreview); delete mShapePreview; }
    mShapePreview=nullptr; mShapeDragging=false; mShapeScene.clear();
}

void UBTeachingTools::armShape(int type)
{
    cancelShapeDrag(); mShapeTarget.clear();
    mWindow->teachingTabs()->setCurrentIndex(0);
    mWindow->actionSelector->trigger();
    if(auto scene=UBApplication::boardController->activeScene()) scene->clearSelection();
    mShapeType=type; mShapeChoices[type]->setChecked(true); mShapeDrawing=true;
    UBApplication::boardController->controlView()->viewport()->setCursor(Qt::CrossCursor);
    mShapeBar->show(); mShapes->setChecked(true); applyShapeStyle();
}

void UBTeachingTools::shapeDialog()
{
    if (!mShapes->isChecked()) {
        cancelShapeDrag(); mShapeDrawing=false; mShapeBar->hide();
        UBApplication::boardController->controlView()->viewport()->unsetCursor(); return;
    }
    mWindow->teachingTabs()->setCurrentIndex(0); mShapeBar->show(); syncShapeSelection();
    if (!mShapeTarget) armShape(mShapeType);
}

void UBTeachingTools::syncShapeSelection()
{
    if (mShapeDragging) return;
    mShapeTarget.clear();
    if(auto scene=UBApplication::boardController->activeScene())
        for(auto item:scene->selectedItems()) if(auto svg=dynamic_cast<UBGraphicsSvgItem*>(item))
            if(svg->isTeachingShape()) { mShapeTarget=svg; break; }
    if (!mShapeTarget) return;
    mShapeDrawing=false; UBApplication::boardController->controlView()->viewport()->unsetCursor();
    QDomDocument doc; doc.setContent(mShapeTarget->fileData());
    auto groups=doc.elementsByTagName("g"); if(groups.isEmpty())return;
    const auto g=groups.at(0).toElement();
    mSyncingShape=true;
    auto metadata=doc.elementsByTagName("metadata");
    if(!metadata.isEmpty())mShapeType=qBound(0,metadata.at(0).toElement().text().toInt(),6);
    mShapeChoices[mShapeType]->setChecked(true);
    mNoBorder=g.attribute("stroke")=="none"; if(!mNoBorder)mShapeBorder=QColor(g.attribute("stroke"));
    mNoFill=g.attribute("fill","none")=="none"; if(!mNoFill)mShapeFill=QColor(g.attribute("fill"));
    const double width=g.attribute("stroke-width","3").toDouble();
    int index=mShapeWidth->findData(width);
    if(index<0) {mShapeWidth->addItem(QString::number(width),width);index=mShapeWidth->count()-1;}
    mShapeWidth->setCurrentIndex(index);
    mShapeStyle->setCurrentIndex(g.attribute("stroke-dasharray","none")!="none"?1:0);
    mShapeOpacity->setValue(qRound(g.attribute("fill-opacity","1").toDouble()*100));
    applyShapeStyle(); mSyncingShape=false;
}

void UBTeachingTools::applyShapeStyle()
{
    auto icon=[](QColor colour,bool none) {QPixmap p(22,22);p.fill(Qt::transparent);QPainter painter(&p);painter.setPen(QPen(Qt::gray,1));painter.setBrush(none?QColor(Qt::white):colour);painter.drawRect(1,1,19,19);if(none){painter.setPen(QPen(Qt::red,2));painter.drawLine(2,20,20,2);}return QIcon(p);};
    mBorderButton->setIcon(icon(mShapeBorder,mNoBorder));mFillButton->setIcon(icon(mShapeFill,mNoFill));
    if(mSyncingShape)return;
    auto settings=UBSettings::settings();settings->setValue("Teaching/ShapeBorder",mShapeBorder);settings->setValue("Teaching/ShapeFill",mShapeFill);
    settings->setValue("Teaching/ShapeNoBorder",mNoBorder);settings->setValue("Teaching/ShapeNoFill",mNoFill);
    settings->setValue("Teaching/ShapeWidth",mShapeWidth->currentData());settings->setValue("Teaching/ShapeDashed",mShapeStyle->currentIndex()==1);
    settings->setValue("Teaching/ShapeOpacity",mShapeOpacity->value());settings->setValue("Teaching/ShapeType",mShapeType);
    if(!mShapeTarget || mShapeTarget->scene()!=UBApplication::boardController->activeScene().get())return;
    const auto before=mShapeTarget->fileData(); QDomDocument doc;doc.setContent(before);
    auto groups=doc.elementsByTagName("g");if(groups.isEmpty())return;auto g=groups.at(0).toElement();
    g.setAttribute("stroke",mNoBorder?"none":mShapeBorder.name());g.setAttribute("fill",mNoFill?"none":mShapeFill.name());
    g.setAttribute("stroke-width",QString::number(mShapeWidth->currentData().toDouble()));
    g.setAttribute("fill-opacity",QString::number(mShapeOpacity->value()/100.0));
    g.setAttribute("stroke-dasharray",mShapeStyle->currentIndex()==1?QString("%1 %2").arg(mShapeWidth->currentData().toDouble()*4).arg(mShapeWidth->currentData().toDouble()*3):"none");
    // Selection synchronization must not rewrite SVGs or add undo commands.
    QDomDocument old;old.setContent(before);
    const auto oldGroup=old.elementsByTagName("g").at(0).toElement();
    bool changed=false;
    for(const auto name:{"stroke","fill","stroke-width","fill-opacity","stroke-dasharray"})
        changed |= oldGroup.attribute(name,QString(name)=="fill-opacity"?"1":QString(name)=="stroke-dasharray"?"none":"") != g.attribute(name);
    if(changed){mShapeTarget->setTeachingSvg(doc.toByteArray());mShapeTarget->commitTeachingSvg(before);}

}

void UBTeachingTools::createShapeToolbar()
{
    auto settings=UBSettings::settings();
    mShapeBorder=settings->value("Teaching/ShapeBorder",QColor(Qt::black)).value<QColor>();
    mShapeFill=settings->value("Teaching/ShapeFill",QColor("#ffdd00")).value<QColor>();
    mNoBorder=settings->value("Teaching/ShapeNoBorder",false).toBool();mNoFill=settings->value("Teaching/ShapeNoFill",true).toBool();
    mShapeType=qBound(0,settings->value("Teaching/ShapeType",0).toInt(),6);
    mShapeBar=new QToolBar(tr("Shape controls"),mWindow);mShapeBar->setObjectName("lessonShapeToolbar");
    mShapeBar->setMovable(false);mShapeBar->setFloatable(false);mShapeBar->setIconSize(QSize(26,26));
    mShapeBar->setStyleSheet("QToolBar {spacing:4px;padding:4px;} QToolButton {height:32px;min-width:30px;padding:2px;} QComboBox {min-height:28px;}");
    mWindow->addToolBarBreak(Qt::TopToolBarArea);mWindow->addToolBar(Qt::TopToolBarArea,mShapeBar);
    auto group=new QActionGroup(this);group->setExclusive(true);
    const QStringList names={tr("Rectangle"),tr("Square"),tr("Circle"),tr("Triangle"),tr("Ellipse"),tr("Right triangle"),tr("Diamond")};
    for(int i=0;i<names.size();++i) {
        mShapeType=i;QPixmap pix(30,30);pix.fill(Qt::transparent);QPainter painter(&pix);painter.setRenderHint(QPainter::Antialiasing);painter.setPen(QPen(QColor("#164f63"),2));painter.setBrush(QColor("#e3f0f4"));painter.drawPath(shapePath(QRectF(4,i==0?8:4,22,i==0?14:22)));painter.end();
        auto action=mShapeBar->addAction(QIcon(pix),names[i]);action->setObjectName(QString("lessonShape%1").arg(i));action->setCheckable(true);group->addAction(action);mShapeChoices<<action;
        connect(action,&QAction::triggered,this,[this,i]{armShape(i);});
    }
    mShapeType=qBound(0,settings->value("Teaching/ShapeType",0).toInt(),6);mShapeChoices[mShapeType]->setChecked(true);
    auto colourButton=[&](const QString &text,bool border) {
        auto button=new QToolButton;button->setText(text);button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);button->setPopupMode(QToolButton::InstantPopup);
        button->setObjectName(border?"lessonShapeBorder":"lessonShapeFill");auto menu=new QMenu(button);button->setMenu(menu);
        auto none=menu->addAction(border?tr("No border"):tr("No fill"));none->setObjectName(border?"lessonShapeNoBorder":"lessonShapeNoFill");
        connect(none,&QAction::triggered,this,[this,border]{if(border)mNoBorder=true;else mNoFill=true;applyShapeStyle();});
        auto panel=new QWidget;auto grid=new QGridLayout(panel);grid->setContentsMargins(5,5,5,5);
        const auto colours=UBSettings::settings()->penColors(false);
        for(int i=0;i<colours.size();++i){auto b=new QToolButton;const auto c=colours[i];b->setFixedSize(30,28);b->setStyleSheet(QString("QToolButton {background:%1;border:1px solid #777;}").arg(c.name()));b->setToolTip(c.name());grid->addWidget(b,i/5,i%5);connect(b,&QToolButton::clicked,this,[this,border,c,menu]{if(border){mNoBorder=false;mShapeBorder=c;}else{mNoFill=false;mShapeFill=c;}applyShapeStyle();menu->close();});}
        auto wrapper=new QWidgetAction(menu);wrapper->setDefaultWidget(panel);menu->addAction(wrapper);
        auto custom=menu->addAction(tr("Custom colour…"));connect(custom,&QAction::triggered,this,[this,border]{auto c=QColorDialog::getColor(border?mShapeBorder:mShapeFill,mWindow);if(!c.isValid())return;if(border){mNoBorder=false;mShapeBorder=c;}else{mNoFill=false;mShapeFill=c;}applyShapeStyle();});
        mShapeBar->addWidget(button);return button;
    };
    mShapeBar->addSeparator();mBorderButton=colourButton(tr("Border"),true);mFillButton=colourButton(tr("Fill"),false);
    mShapeBar->addWidget(new QLabel(tr("Width")));mShapeWidth=new QComboBox;mShapeWidth->setObjectName("lessonShapeWidth");
    for(double w:{1.,2.,3.,5.,8.,12.})mShapeWidth->addItem(QString::number(w),w);
    mShapeWidth->setCurrentIndex(qMax(0,mShapeWidth->findData(settings->value("Teaching/ShapeWidth",3.).toDouble())));mShapeBar->addWidget(mShapeWidth);
    mShapeStyle=new QComboBox;mShapeStyle->setObjectName("lessonShapeBorderStyle");mShapeStyle->addItems({tr("Solid"),tr("Dashed")});mShapeStyle->setCurrentIndex(settings->value("Teaching/ShapeDashed",false).toBool()?1:0);mShapeBar->addWidget(mShapeStyle);
    mShapeBar->addWidget(new QLabel(tr("Fill opacity")));mShapeOpacity=new QSlider(Qt::Horizontal);mShapeOpacity->setObjectName("lessonShapeOpacity");mShapeOpacity->setRange(0,100);mShapeOpacity->setValue(settings->value("Teaching/ShapeOpacity",100).toInt());mShapeOpacity->setFixedWidth(85);mShapeBar->addWidget(mShapeOpacity);
    auto value=new QLabel(QString::number(mShapeOpacity->value())+"%");value->setMinimumWidth(36);mShapeBar->addWidget(value);
    connect(mShapeOpacity,&QSlider::valueChanged,this,[this,value](int v){value->setText(QString::number(v)+"%");if(!mShapeOpacity->isSliderDown())applyShapeStyle();});
    connect(mShapeOpacity,&QSlider::sliderReleased,this,&UBTeachingTools::applyShapeStyle);
    connect(mShapeWidth,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{applyShapeStyle();});
    connect(mShapeStyle,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{applyShapeStyle();});
    auto next=mShapeBar->addAction(tr("Draw next"));next->setObjectName("lessonShapeDrawNext");connect(next,&QAction::triggered,this,[this]{armShape(mShapeType);});
    auto close=mShapeBar->addAction(tr("Close"));connect(close,&QAction::triggered,this,[this]{mShapes->setChecked(false);shapeDialog();});
    auto attach=[this]{cancelShapeDrag();mShapeDrawing=false;QObject::disconnect(mShapeSelectionConnection);if(auto scene=UBApplication::boardController->activeScene())mShapeSelectionConnection=connect(scene.get(),&QGraphicsScene::selectionChanged,this,&UBTeachingTools::syncShapeSelection);syncShapeSelection();};
    connect(UBApplication::boardController,&UBBoardController::activeSceneChanged,this,attach);attach();
    connect(UBDrawingController::drawingController(),&UBDrawingController::stylusToolChanged,this,[this](int tool,int){if(tool!=UBStylusTool::Selector){cancelShapeDrag();mShapeDrawing=false;UBApplication::boardController->controlView()->viewport()->unsetCursor();mShapeBar->hide();mShapes->setChecked(false);}});
    connect(mWindow->teachingTabs(),&QTabWidget::currentChanged,this,[this](int index){if(index!=0){cancelShapeDrag();mShapeDrawing=false;mShapeBar->hide();mShapes->setChecked(false);}});
    connect(mWindow->boardToolBar,&QToolBar::visibilityChanged,this,[this](bool visible){if(!visible){cancelShapeDrag();mShapeDrawing=false;mShapeBar->hide();mShapes->setChecked(false);}});
    mSyncingShape=true;applyShapeStyle();mSyncingShape=false;mShapeBar->hide();
}

void UBTeachingTools::editSelectedGraph()
{
    auto scene = UBApplication::boardController->activeScene();
    if (!scene || !mGraph) return;
    for (auto item : scene->selectedItems()) {
        auto svg = dynamic_cast<UBGraphicsSvgItem*>(item);
        if (!svg) continue;
        QDomDocument doc; doc.setContent(svg->fileData());
        auto metadata = doc.elementsByTagName("metadata");
        for (int i = 0; i < metadata.size(); ++i) {
            auto element = metadata.at(i).toElement();
            if (element.attribute("id") != "graphdesk-project") continue;
            const auto state = QJsonDocument::fromJson(QByteArray::fromBase64(element.text().toLatin1()));
            if (!state.isObject()) continue;
            mGraphTarget = svg;
            mGraph->page()->runJavaScript(QString("importProject(%1)").arg(QString::fromUtf8(state.toJson(QJsonDocument::Compact))));
            return;
        }
    }
    QMessageBox::information(mWindow, tr("GraphDesk"), tr("Select an inserted GraphDesk graph on the Lesson tab first."));
}
