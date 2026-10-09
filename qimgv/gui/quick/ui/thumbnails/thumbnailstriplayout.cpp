#include "thumbnailstriplayout.h"

#include <QSizeF>
#include <QtGlobal>

namespace Enums = SettingsEnums;

ThumbnailStripLayout thumbnailStripLayoutFor(const ThumbnailStripLayoutInput &input) {
    ThumbnailStripLayout layout;
    layout.horizontal =
        input.position == Enums::PanelPosition::Top || input.position == Enums::PanelPosition::Bottom;
    layout.thumbnailSize = qBound(ThumbnailStripLayout::kMinimumPreviewSize, input.previewsSize,
                                  ThumbnailStripLayout::kMaximumPreviewSize);
    layout.marginX = layout.horizontal ? ThumbnailStripLayout::kHorizontalMarginX
                                       : ThumbnailStripLayout::kVerticalMarginX;
    layout.marginY = layout.horizontal ? ThumbnailStripLayout::kHorizontalMarginY
                                       : ThumbnailStripLayout::kVerticalMarginY;
    layout.labels = input.style != SettingsEnums::PanelStyle::Simple;
    layout.textHeight = input.textHeight;

    // ThumbnailWidget::updateBoundingRect(); the strip uses the rounded size
    // (ThumbnailStrip::itemSize()).
    const double imageHeight = layout.thumbnailSize * ThumbnailStripLayout::kImageAspect;
    QSizeF cell(layout.thumbnailSize + (ThumbnailStripLayout::kPadding + layout.marginX) * 2,
                imageHeight + (ThumbnailStripLayout::kPadding + layout.marginY) * 2);
    if (layout.labels)
        cell.rheight() += ThumbnailStripLayout::kLabelSpacing + input.textHeight * 2;
    const QSize cellSize = cell.toSize();
    layout.cellWidth = cellSize.width();
    layout.cellHeight = cellSize.height();

    layout.imageArea = QSize(layout.thumbnailSize, qRound(imageHeight));
    layout.imageCenterOffset = layout.labels ? -input.textHeight : 0;

    // ThumbnailWidget::setupTextLayout(): the rectangle is made of truncated
    // coordinates.
    layout.labelLeft = ThumbnailStripLayout::kPadding + layout.marginX;
    layout.labelTop = static_cast<int>(ThumbnailStripLayout::kPadding + layout.marginY +
                                       imageHeight + ThumbnailStripLayout::kLabelSpacing);
    layout.infoTop = layout.labelTop + input.textHeight + ThumbnailStripLayout::kInfoLineSpacing;
    layout.labelWidth = layout.thumbnailSize;

    // MainPanel::sizeHint().
    if (layout.horizontal) {
        layout.panelExtent = layout.cellHeight + ThumbnailStripLayout::kPanelAllowance;
        if (input.position == Enums::PanelPosition::Bottom)
            layout.panelExtent += ThumbnailStripLayout::kBottomPanelStrip;
    } else {
        layout.panelExtent = layout.cellWidth + ThumbnailStripLayout::kPanelAllowance;
    }
    return layout;
}
