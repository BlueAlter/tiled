/*
 * wangoverlay.cpp
 * Copyright 2020, Thorbjørn Lindeijer <bjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "wangoverlay.h"

#include "utils.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPalette>

namespace Tiled {

void paintWangOverlay(QPainter *painter,
                      WangId wangId,
                      const WangSet &wangSet,
                      const QRect &rect,
                      WangOverlayOptions options,
                      int centerColor)
{
    if (!wangId && centerColor == 0)
        return;

    const QRect adjustedRect = rect.adjusted(1, 1, -1, -1);
    if (adjustedRect.isEmpty())
        return;

    const qreal fillOpacity = options.testFlag(WO_TransparentFill) ? 0.45 : 1.0;
    const qreal penWidth = qBound(1.0, adjustedRect.width() / 32.0, 2.0);

    painter->save();
    painter->setClipRect(rect);
    painter->setRenderHint(QPainter::Antialiasing, false);

    const qreal cellWidth = adjustedRect.width() / 3.0;
    const qreal cellHeight = adjustedRect.height() / 3.0;

    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            int colorIndex = 0;
            if (x == 1 && y == 1) {
                colorIndex = centerColor;
            } else {
                const WangId::Index index = WangId::indexByGrid(x, y);
                colorIndex = wangId.indexColor(index);
            }

            if (colorIndex == 0)
                continue;

            QColor color;
            bool isEraseHover = false;

            if (colorIndex == (int)WangId::INDEX_MASK) {
                color = QGuiApplication::palette().color(QPalette::Highlight);
            } else if (colorIndex == -1) {
                // Erase preview indicator
                color = QColor(240, 50, 50);
                isEraseHover = true;
            } else if (colorIndex > 0 && colorIndex <= wangSet.colorCount()) {
                color = wangSet.colorAt(colorIndex)->color();
            }

            if (!color.isValid())
                continue;

            const qreal left = adjustedRect.left() + x * cellWidth;
            const qreal top = adjustedRect.top() + y * cellHeight;
            const qreal right = (x == 2) ? (adjustedRect.right() + 1) : (adjustedRect.left() + (x + 1) * cellWidth);
            const qreal bottom = (y == 2) ? (adjustedRect.bottom() + 1) : (adjustedRect.top() + (y + 1) * cellHeight);
            const QRectF cellRect(QPointF(left, top), QPointF(right, bottom));

            const QRectF innerRect = cellRect.adjusted(0.5, 0.5, -0.5, -0.5);

            if (options.testFlag(WO_Shadow)) {
                painter->setPen(QPen(QColor(0, 0, 0, 160), penWidth));
                painter->setBrush(Qt::NoBrush);
                painter->drawRect(innerRect.adjusted(1, 1, 1, 1));
            }

            const QColor fillColor(color.red(), color.green(), color.blue(),
                                   int(color.alpha() * (isEraseHover ? 0.6 : fillOpacity)));
            painter->fillRect(innerRect, fillColor);

            if (options.testFlag(WO_Outline) || isEraseHover) {
                QPen pen(isEraseHover ? color : (options.testFlag(WO_TransparentFill) ? color : Qt::black), penWidth);
                pen.setCosmetic(true);
                painter->setPen(pen);
                painter->setBrush(Qt::NoBrush);
                painter->drawRect(innerRect);
            }
        }
    }

    painter->restore();
}

static QIcon paintWangSetIcon(WangSet::Type type)
{
    static const auto iconSize = Utils::dpiScaled(QSize(32, 32));

    QPixmap pixmap(iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);

    WangSet wangSet(nullptr, QString(), type);
    wangSet.setColorCount(2);

    WangId wangId;
    int centerColor = 0;

    switch (type) {
    case WangSet::Corner:
        wangId.setIndexColor(WangId::TopLeft, 1);
        wangId.setIndexColor(WangId::BottomRight, 1);
        break;
    case WangSet::Edge:
        wangId.setIndexColor(WangId::Top, 1);
        wangId.setIndexColor(WangId::Bottom, 1);
        break;
    case WangSet::Mixed:
        wangId.setIndexColor(WangId::TopLeft, 1);
        wangId.setIndexColor(WangId::Top, 1);
        wangId.setIndexColor(WangId::Left, 1);
        centerColor = 1;
        break;
    }

    paintWangOverlay(&painter, wangId, wangSet, pixmap.rect(),
                     WO_TransparentFill | WO_Outline, centerColor);

    return QIcon(pixmap);
}

QIcon wangSetIcon(WangSet::Type type)
{
    switch (type) {
    case WangSet::Corner: {
        static QIcon icon = paintWangSetIcon(type);
        return icon;
    }
    case WangSet::Edge: {
        static QIcon icon = paintWangSetIcon(type);
        return icon;
    }
    case WangSet::Mixed: {
        static QIcon icon = paintWangSetIcon(type);
        return icon;
    }
    }
    return QIcon();
}

} // namespace Tiled
