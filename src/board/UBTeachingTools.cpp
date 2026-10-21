// SPDX-License-Identifier: GPL-3.0-only
#include "UBTeachingTools.h"
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
    auto dash = add("actionTeachingDash", tr("Dashed lines"), ":/images/teaching/dash.svg");
    dash->setCheckable(true);
    dash->setChecked(UBSettings::settings()->value("Teaching/DashedLines", false).toBool());
    connect(dash, &QAction::toggled, this, [](bool on) {
        UBSettings::settings()->setValue("Teaching/DashedLines", on);
    });
    auto shapes = add("actionTeachingShapes", tr("Shapes"), ":/images/teaching/shapes.svg");
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

    // Keep real actions registered and visible for shortcuts. Only toolbar
    // presentation proxies are hidden; this also preserves disabled states.
    int group = 0;
    for (auto source : toolbar->actions()) {
        if (source->isSeparator()) continue;
        if (source->objectName().isEmpty()) source->setObjectName(QString("teachingGroup%1").arg(group++));
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
        toolbar->insertAction(source, proxy);
        toolbar->removeAction(source);
        mProxies[source] = proxy;
    }
    const QList<QAction*> tools = {window->actionPen, window->actionEraser, window->actionMarker,
        window->actionSelector, window->actionPlay, window->actionHand, window->actionZoomIn,
        window->actionZoomOut, window->actionPointer, window->actionLine, window->actionText,
        window->actionCapture, window->actionVirtualKeyboard, window->actionSnap};
    for (auto action : tools) {
        mBottom[action->objectName()] = action;
        mDefaultBottom << action->objectName();
        action->setAutoRepeat(false);
    }
    auto customizeAction = mCustomize = new QAction(tr("Customize toolbars…"), this);
    customizeAction->setToolTip(tr("Choose visible tools and drag to change their order"));
    toolbar->addAction(customizeAction);
    connect(customizeAction, &QAction::triggered, this, &UBTeachingTools::customize);
    toolbar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(toolbar, &QWidget::customContextMenuRequested, this, [this] { customize(); });
    qApp->installEventFilter(this);
    applyLayout();
}

bool UBTeachingTools::eventFilter(QObject *, QEvent *event)
{
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
        mWindow->boardToolBar->insertAction(mCustomize, display);
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

void UBTeachingTools::shapeDialog()
{
    // Shape geometry is stored in SVG itself; existing OpenBoard transforms
    // provide selection, movement, resize and rotation, and UBZ persistence.
    QDialog dialog(mWindow);
    UBGraphicsSvgItem *target = nullptr;
    auto activeScene = UBApplication::boardController->activeScene();
    if (activeScene) for (auto item : activeScene->selectedItems()) {
        auto svg = dynamic_cast<UBGraphicsSvgItem*>(item);
        if (svg && svg->isTeachingShape()) { target = svg; break; }
    }
    QDomDocument existing;
    if (target) existing.setContent(target->fileData());
    dialog.setWindowTitle(target ? tr("Edit selected shape") : tr("Insert shape"));
    auto form = new QFormLayout(&dialog);
    auto type = new QComboBox;
    type->addItems({tr("Rectangle"), tr("Square"), tr("Circle"), tr("Triangle")});
    if (target) {
        type->setCurrentIndex(existing.elementsByTagName("metadata").at(0).toElement().text().toInt());
        type->setEnabled(false);
    }
    form->addRow(tr("Shape"), type);
    auto filled = new QCheckBox(tr("Filled")); form->addRow(filled);
    QColor outline = UBDrawingController::drawingController()->currentToolColor();
    QColor fill = QColor("#FFDD00");
    if (target) {
        auto group = existing.elementsByTagName("g").at(0).toElement();
        outline = QColor(group.attribute("stroke"));
        const QString value = group.attribute("fill", "none");
        filled->setChecked(value != "none");
        if (value != "none") fill = QColor(value);
    }
    auto outlineButton = new QPushButton(tr("Outline colour…"));
    auto fillButton = new QPushButton(tr("Fill colour…"));
    form->addRow(outlineButton, fillButton);
    connect(outlineButton, &QPushButton::clicked, &dialog, [&] {
        auto c = QColorDialog::getColor(outline, &dialog); if (c.isValid()) outline = c;
    });
    connect(fillButton, &QPushButton::clicked, &dialog, [&] {
        auto c = QColorDialog::getColor(fill, &dialog); if (c.isValid()) fill = c;
    });
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    if (target) {
        auto group = existing.elementsByTagName("g").at(0).toElement();
        group.setAttribute("stroke", outline.name());
        group.setAttribute("fill", filled->isChecked() ? fill.name() : "none");
        const auto before = target->fileData();
        target->setTeachingSvg(existing.toByteArray());
        target->commitTeachingSvg(before);
        return;
    }
    const QStringList elements = {"<rect x=\"12\" y=\"52\" width=\"276\" height=\"196\"/>",
        "<rect x=\"12\" y=\"12\" width=\"276\" height=\"276\"/>",
        "<circle cx=\"150\" cy=\"150\" r=\"138\"/>",
        "<polygon points=\"150,12 288,288 12,288\"/>"};
    const QString svg = QString("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"300\" height=\"300\" viewBox=\"0 0 300 300\"><metadata id=\"teaching-shape\">%1</metadata><g stroke=\"%2\" fill=\"%3\" stroke-width=\"3\" stroke-linejoin=\"round\">%4</g></svg>")
        .arg(type->currentIndex()).arg(outline.name(), filled->isChecked() ? fill.name() : "none", elements[type->currentIndex()]);
    auto board = UBApplication::boardController;
    if (!board->activeScene()) return;
    board->activeScene()->addSvg(QUrl(), board->controlView()->mapToScene(board->controlView()->viewport()->rect().center()), svg.toUtf8());
    mWindow->teachingTabs()->setCurrentIndex(0);
    mWindow->actionSelector->trigger();
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
