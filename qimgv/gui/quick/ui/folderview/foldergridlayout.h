#pragma once

#include <QObject>
#include <QRectF>
#include <QtQml/qqmlregistration.h>

#include "gui/quick/ui/thumbnails/thumbnailstriplayout.h"

// What the layout of the folder grid depends on.
struct FolderGridLayoutInput {
    // Width of the grid area without the scroll bar, in logical pixels.
    int viewWidth = 0;
    // The folderViewIconSize setting (bounded to kMinimumIconSize -
    // kMaximumIconSize).
    int iconSize = 0;
    int itemCount = 0;
    // Line height of the label font (QFontMetrics::height()).
    int textHeight = 0;
};

// Geometry of the folder grid, as FolderGridView::gridGeometry() and the
// labelled ThumbnailWidget (THUMB_NORMAL, padding 8) compute it, in logical
// pixels: the columns that fit between the side margins, centred when the
// directory fills a row, below a top margin. The cell is described like a
// cell of the thumbnail strip, so the strip's cell item draws it; the cell
// size is rounded to whole pixels.
struct FolderGridLayout {
    Q_GADGET
    QML_VALUE_TYPE(folderGridLayout)
    QML_UNCREATABLE("Provided by FolderGridController.layout")
    Q_PROPERTY(ThumbnailStripLayout cell MEMBER cell FINAL)
    Q_PROPERTY(int columns MEMBER columns FINAL)
    Q_PROPERTY(qreal left MEMBER left FINAL)
    Q_PROPERTY(int top MEMBER top FINAL)
    Q_PROPERTY(int cellWidth READ cellWidth FINAL)
    Q_PROPERTY(int cellHeight READ cellHeight FINAL)

public:
    static constexpr int kMinimumIconSize = 128;
    static constexpr int kMaximumIconSize = 512;
    // Ctrl + wheel changes the icon size by this step.
    static constexpr int kZoomStep = 16;
    static constexpr int kLeftMargin = 9;
    static constexpr int kRightMargin = 9;
    static constexpr int kTopMargin = 6;
    // Padding between the cell surface and the thumbnail area, and the cell
    // margins (ThumbnailWidget defaults).
    static constexpr int kPadding = 8;
    static constexpr int kCellMargin = 2;

    ThumbnailStripLayout cell;
    int columns = 1;
    // Position of the first column and of the first row.
    qreal left = kLeftMargin;
    int top = kTopMargin;

    [[nodiscard]] int cellWidth() const { return cell.cellWidth; }
    [[nodiscard]] int cellHeight() const { return cell.cellHeight; }
    // Cell of the item at index, in content coordinates (the first row
    // starts at top).
    [[nodiscard]] QRectF itemRect(int index) const;

    friend bool operator==(const FolderGridLayout &, const FolderGridLayout &) = default;
};

[[nodiscard]] int boundedFolderIconSize(int size);
[[nodiscard]] FolderGridLayout folderGridLayoutFor(const FolderGridLayoutInput &input);
