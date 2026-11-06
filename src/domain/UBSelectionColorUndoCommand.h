// SPDX-License-Identifier: GPL-3.0-only
#ifndef UBSELECTIONCOLORUNDOCOMMAND_H
#define UBSELECTIONCOLORUNDOCOMMAND_H

#include <QDomDocument>
#include <QPointer>
#include <QSet>
#include "UBUndoCommand.h"
#include "UBGraphicsScene.h"
#include "UBGraphicsPolygonItem.h"
#include "UBGraphicsSvgItem.h"
#include "UBGraphicsGroupContainerItem.h"
#include "UBShapeSvg.h"

// A single transaction retains each polygon's individual colours, including
// mixed-colour groups and highlighter alpha. Geometry is never replaced.
class UBSelectionColorUndoCommand : public UBUndoCommand
{
    struct Ink {
        UBGraphicsPolygonItem *item;
        QColor before, lightBefore, darkBefore, after, lightAfter, darkAfter;
    };
    struct Shape {
        QPointer<UBGraphicsSvgItem> item;
        QByteArray before, after;
    };
    std::shared_ptr<UBGraphicsScene> mScene;
    QList<Ink> mInk;
    QList<Shape> mShapes;
    QSet<QGraphicsItem*> mVisited;
    void collect(QGraphicsItem *item, QColor light, QColor dark)
    {
        if (mVisited.contains(item)) return;
        mVisited.insert(item);
        if (auto polygon = dynamic_cast<UBGraphicsPolygonItem*>(item)) {
            const auto before = polygon->color();
            light.setAlpha(before.alpha()); dark.setAlpha(before.alpha());
            const auto after = mScene->isDarkBackground() ? dark : light;
            if (before != after || polygon->colorOnLightBackground() != light || polygon->colorOnDarkBackground() != dark)
                mInk.append({polygon, before, polygon->colorOnLightBackground(), polygon->colorOnDarkBackground(), after, light, dark});
        } else if (auto svg = dynamic_cast<UBGraphicsSvgItem*>(item)) {
            if (!svg->isTeachingShape()) return; // PDFs, pictures, covers and formulae are untouched.
            const auto before = svg->fileData();
            QDomDocument doc;
            if (!doc.setContent(before)) return;
            auto groups = doc.elementsByTagName("g");
            if (groups.isEmpty()) return;
            auto group = groups.at(0).toElement();
            const auto colour = mScene->isDarkBackground() ? dark : light;
            if (group.attribute("stroke") == colour.name()) return;
            group.setAttribute("stroke", colour.name());
            // Rebuild the baked outlines used by native painting and export.
            const auto after = UBShapeGeometry::uniformSvg(doc.toByteArray(), svg->sceneTransform());
            mShapes.append({svg, before, after});
        } else if (item->type() == UBGraphicsStrokesGroup::Type || item->type() == UBGraphicsGroupContainerItem::Type) {
            for (auto child : item->childItems()) collect(child, light, dark);
        }
    }
    void apply(bool forward)
    {
        for (const auto &ink : mInk) {
            QColor visible = ink.before;
            if (forward)
                visible = mScene->isDarkBackground() ? ink.darkAfter : ink.lightAfter;
            else if (ink.before == ink.lightBefore || ink.before == ink.darkBefore)
                visible = mScene->isDarkBackground() ? ink.darkBefore : ink.lightBefore;
            ink.item->setColor(visible);
            ink.item->setColorOnLightBackground(forward ? ink.lightAfter : ink.lightBefore);
            ink.item->setColorOnDarkBackground(forward ? ink.darkAfter : ink.darkBefore);
            ink.item->update();
        }
        for (const auto &shape : mShapes)
            if (shape.item) shape.item->setTeachingSvg(forward ? shape.after : shape.before);
        mScene->setModified(true);
        mScene->update();
    }
public:
    UBSelectionColorUndoCommand(std::shared_ptr<UBGraphicsScene> scene, QColor light, QColor dark) : mScene(scene)
    {
        setText(QObject::tr("Change selection colour"));
        for (auto item : scene->selectedItems()) collect(item, light, dark);
        mVisited.clear();
    }
    bool hasChanges() const { return !mInk.isEmpty() || !mShapes.isEmpty(); }
    void undo() override { apply(false); }
    void redo() override { apply(true); }
};
#endif
