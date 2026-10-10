#include "foldergridlayout.h"

#include <QSizeF>
#include <QtGlobal>

#include <cmath>

QRectF FolderGridLayout::itemRect(int index) const {
    if (index < 0 || columns <= 0)
        return {};
    const int row = index / columns;
    const int column = index % columns;
    return {left + static_cast<qreal>(column) * cellWidth(),
            static_cast<qreal>(top) + static_cast<qreal>(row) * cellHeight(),
            static_cast<qreal>(cellWidth()), static_cast<qreal>(cellHeight())};
}

int boundedFolderIconSize(int size) {
    return qBound(FolderGridLayout::kMinimumIconSize, size, FolderGridLayout::kMaximumIconSize);
}

FolderGridLayout folderGridLayoutFor(const FolderGridLayoutInput &input) {
    FolderGridLayout layout;
    ThumbnailStripLayout &cell = layout.cell;
    cell.horizontal = false;
    cell.thumbnailSize = boundedFolderIconSize(input.iconSize);
    cell.marginX = FolderGridLayout::kCellMargin;
    cell.marginY = FolderGridLayout::kCellMargin;
    cell.labels = true;
    cell.textHeight = input.textHeight;

    // ThumbnailWidget::updateBoundingRect().
    const int inset = FolderGridLayout::kPadding + FolderGridLayout::kCellMargin;
    const double imageHeight = cell.thumbnailSize * ThumbnailStripLayout::kImageAspect;
    const QSizeF cellSize(cell.thumbnailSize + inset * 2,
                          imageHeight + inset * 2 + ThumbnailStripLayout::kLabelSpacing +
                              input.textHeight * 2);
    cell.cellWidth = qRound(cellSize.width());
    cell.cellHeight = qRound(cellSize.height());
    cell.imageArea = QSize(cell.thumbnailSize, qRound(imageHeight));
    // THUMB_NORMAL centres the thumbnail in the image area above the labels.
    cell.imageCenterOffset = qRound(inset + imageHeight / 2.0 - cell.cellHeight / 2.0);

    // ThumbnailWidget::setupTextLayout(): the rectangle is made of truncated
    // coordinates.
    cell.labelLeft = inset;
    cell.labelTop = static_cast<int>(inset + imageHeight + ThumbnailStripLayout::kLabelSpacing);
    cell.infoTop = cell.labelTop + input.textHeight + ThumbnailStripLayout::kInfoLineSpacing;
    cell.labelWidth = cell.thumbnailSize;

    // FolderGridView::gridGeometry(): the remainder of a full row is split
    // between both sides.
    const qreal cellWidth = cell.cellWidth;
    const qreal available =
        qMax(cellWidth, static_cast<qreal>(input.viewWidth - FolderGridLayout::kLeftMargin -
                                           FolderGridLayout::kRightMargin));
    layout.columns = qMax(1, static_cast<int>(available / cellWidth));
    const qreal centring =
        input.itemCount >= layout.columns ? std::fmod(available, cellWidth) / 2.0 : 0.0;
    layout.left = FolderGridLayout::kLeftMargin + centring;
    return layout;
}
