#pragma once

#include <QObject>
#include <QSize>
#include <QtQml/qqmlregistration.h>

#include "gui/quick/bridges/settingsenums.h"

// What the size of the thumbnail strip depends on.
struct ThumbnailStripLayoutInput {
    // The panelPreviewsSize setting (bounded to kMinimumPreviewSize -
    // kMaximumPreviewSize).
    int previewsSize = 0;
    SettingsEnums::PanelStyle style = SettingsEnums::PanelStyle::Simple;
    SettingsEnums::PanelPosition position = SettingsEnums::PanelPosition::Top;
    // Line height of the label font (QFontMetrics::height()).
    int textHeight = 0;
};

// Geometry of the thumbnail strip and its cells, as the widget ThumbnailStrip
// and ThumbnailWidget compute it, in logical pixels. A cell is the thumbnail
// area (thumbnailSize x 3/4 of it) inside padding and margins, plus two label
// lines for the extended style; the highlight and hover surfaces cover the
// cell minus its margins. QML lays the strip and the cells out from these
// values only.
struct ThumbnailStripLayout {
    Q_GADGET
    QML_VALUE_TYPE(thumbnailStripLayout)
    QML_UNCREATABLE("Provided by ThumbnailPanelController.layout")
    Q_PROPERTY(bool horizontal MEMBER horizontal FINAL)
    Q_PROPERTY(int thumbnailSize MEMBER thumbnailSize FINAL)
    Q_PROPERTY(int cellWidth MEMBER cellWidth FINAL)
    Q_PROPERTY(int cellHeight MEMBER cellHeight FINAL)
    Q_PROPERTY(int itemExtent READ itemExtent FINAL)
    Q_PROPERTY(int marginX MEMBER marginX FINAL)
    Q_PROPERTY(int marginY MEMBER marginY FINAL)
    Q_PROPERTY(QSize imageArea MEMBER imageArea FINAL)
    Q_PROPERTY(int imageCenterOffset MEMBER imageCenterOffset FINAL)
    Q_PROPERTY(bool labels MEMBER labels FINAL)
    Q_PROPERTY(int labelLeft MEMBER labelLeft FINAL)
    Q_PROPERTY(int labelTop MEMBER labelTop FINAL)
    Q_PROPERTY(int infoTop MEMBER infoTop FINAL)
    Q_PROPERTY(int labelWidth MEMBER labelWidth FINAL)
    Q_PROPERTY(int textHeight MEMBER textHeight FINAL)
    Q_PROPERTY(int panelExtent MEMBER panelExtent FINAL)

public:
    static constexpr int kMinimumPreviewSize = 20;
    static constexpr int kMaximumPreviewSize = 300;
    // Padding between the cell surface and the thumbnail area.
    static constexpr int kPadding = 9;
    // Cell margins of a horizontal strip (top / bottom panel) and a vertical
    // one (left / right panel).
    static constexpr int kHorizontalMarginX = 2;
    static constexpr int kHorizontalMarginY = 4;
    static constexpr int kVerticalMarginX = 12;
    static constexpr int kVerticalMarginY = 2;
    // Space between the thumbnail area and the name label, and between the
    // name and the info label.
    static constexpr int kLabelSpacing = 9;
    static constexpr int kInfoLineSpacing = 2;
    // Height of the thumbnail area relative to its width.
    static constexpr double kImageAspect = 0.75;
    // Panel thickness beyond the cell: scroll bar and border, plus the strip
    // above a bottom panel.
    static constexpr int kPanelAllowance = 16;
    static constexpr int kBottomPanelStrip = 3;

    // The strip runs from left to right (top and bottom panel).
    bool horizontal = true;
    int thumbnailSize = 0;
    int cellWidth = 0;
    int cellHeight = 0;
    int marginX = 0;
    int marginY = 0;
    // Area the thumbnail is fitted into (thumbnailDrawSize()).
    QSize imageArea;
    // Vertical offset of the thumbnail centre from the cell centre.
    int imageCenterOffset = 0;
    // The extended style shows the name and the info line under the image.
    bool labels = false;
    int labelLeft = 0;
    int labelTop = 0;
    int infoTop = 0;
    int labelWidth = 0;
    int textHeight = 0;
    // Thickness of the panel across the strip.
    int panelExtent = 0;

    // Size of a cell along the strip.
    [[nodiscard]] int itemExtent() const { return horizontal ? cellWidth : cellHeight; }

    friend bool operator==(const ThumbnailStripLayout &, const ThumbnailStripLayout &) = default;
};

[[nodiscard]] ThumbnailStripLayout thumbnailStripLayoutFor(const ThumbnailStripLayoutInput &input);
