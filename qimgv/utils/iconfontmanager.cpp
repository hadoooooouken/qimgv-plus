#include "iconfontmanager.h"

#include <QDebug>
#include <QFontDatabase>
#include <QHash>
#include <QPainter>
#include <QPixmapCache>

namespace {

// Codepoints from microsoft/fluentui-system-icons, Regular weight
// (fonts/FluentSystemIcons-Regular.json). Entries are grouped by their
// current UI consumers and sorted alphabetically within each group.
const QHash<FluentIcon, char32_t> &codepointTable() {
    static const QHash<FluentIcon, char32_t> table = {
        // Used across multiple UI areas
        { FluentIcon::Adjustments20,       0xE00B }, // ic_fluent_add_subtract_circle_20_regular <- Context menu and color-adjustments overlay header
        { FluentIcon::AiUpscale20,         0xF0799 }, // ic_fluent_brain_sparkle_20_regular <- Context menu and AI-upscale notifications
        { FluentIcon::ArrowAutofitWidth20, 0xE07C }, // ic_fluent_arrow_autofit_width_20_regular <- Context menu and Fit Width notification
        { FluentIcon::ArrowExpand20,       0xE0C5 }, // ic_fluent_arrow_expand_20_regular <- Context menu and Fit Window notification
        { FluentIcon::Blur20,              0xF8FA }, // ic_fluent_blur_20_regular <- Context menu and CAS overlay header
        { FluentIcon::CheckboxChecked16,   0xF27C }, // ic_fluent_checkbox_checked_16_regular <- Checked checkbox indicator
        { FluentIcon::CheckboxIndeterminate16, 0xE2FD }, // ic_fluent_checkbox_indeterminate_16_regular <- Partially checked checkbox indicator
        { FluentIcon::CheckboxUnchecked16, 0xF290 }, // ic_fluent_square_16_regular <- Unchecked checkbox indicator
        { FluentIcon::CopyAdd20,           0xE41F }, // ic_fluent_copy_add_20_regular <- Context menu and copy overlay
        { FluentIcon::Delete20,            0xF34C }, // ic_fluent_delete_20_regular <- Trash actions in context and folder-view menus
        { FluentIcon::Dismiss16,           0xF368 }, // ic_fluent_dismiss_16_regular <- Overlay close buttons and viewer exit controls
        { FluentIcon::Dismiss20,           0xF369 }, // ic_fluent_dismiss_20_regular <- Permanent-delete actions in context and folder-view menus
        { FluentIcon::Folder20,            0xF418 }, // ic_fluent_folder_20_regular <- Path selector and directory notifications
        { FluentIcon::Grid20,              0xF462 }, // ic_fluent_grid_20_regular <- Folder-view actions in context menu and viewer controls
        { FluentIcon::Info20,              0xF4A3 }, // ic_fluent_info_20_regular <- Image-info UI and informational notifications
        { FluentIcon::Move20,              0xE422 }, // ic_fluent_copy_arrow_right_20_regular <- Context menu and move overlay
        { FluentIcon::RadioButton16,       0xF153 }, // ic_fluent_radio_button_16_regular <- Unchecked radio-button indicator
        { FluentIcon::Record16,            0xF660 }, // ic_fluent_record_16_regular <- Checked radio-button indicator
        { FluentIcon::Rename20,            0xF0A39 }, // ic_fluent_rename_a_20_regular <- Context/folder menus and rename
        { FluentIcon::Settings20,          0xF6A9 }, // ic_fluent_settings_20_regular <- Settings actions in context menu, toolbars, and panels
        { FluentIcon::ShowInFolder20,      0xF42E }, // ic_fluent_folder_open_20_regular <- Show-in-folder actions in context and folder-view menus
        { FluentIcon::ZoomOriginal20,      0xF70A }, // ic_fluent_ratio_one_to_one_20_regular <- Context menu and Fit 1:1 notification

        // Context menu
        { FluentIcon::ArrowAutofitHeight20,          0xE077 }, // ic_fluent_arrow_autofit_height_20_regular <- Fit Height
        { FluentIcon::ArrowLeft,                     0xF15B }, // ic_fluent_arrow_left_20_regular <- Back from the Open With page
        { FluentIcon::ArrowRotateClockwise20,        0xF185 }, // ic_fluent_arrow_rotate_clockwise_20_regular <- Rotate clockwise
        { FluentIcon::ArrowRotateCounterclockwise20, 0xF187 }, // ic_fluent_arrow_rotate_counterclockwise_20_regular <- Rotate counterclockwise
        { FluentIcon::ChevronDown20,                 0xF2A3 }, // ic_fluent_chevron_down_20_regular <- Expand More actions
        { FluentIcon::ChevronUp20,                   0xF2B6 }, // ic_fluent_chevron_up_20_regular <- Collapse More actions
        { FluentIcon::Crop20,                        0xE42B }, // ic_fluent_crop_20_regular <- Crop action
        { FluentIcon::FlipHorizontal20,              0xF02A1 }, // ic_fluent_arrow_bidirectional_left_right_20_regular <- Flip horizontally
        { FluentIcon::FlipVertical20,                0xE084 }, // ic_fluent_arrow_bidirectional_up_down_20_regular <- Flip vertically
        { FluentIcon::OpenWith20,                    0xF582 }, // ic_fluent_open_20_regular <- Open With action
        { FluentIcon::Panorama20,                    0xF08F5 }, // ic_fluent_planet_20_regular <- Panorama mode
        { FluentIcon::Print20,                       0xF62A }, // ic_fluent_print_20_regular <- Print action
        { FluentIcon::Resize20,                      0xF66C }, // ic_fluent_resize_20_regular <- Resize action
        { FluentIcon::Wallpaper20,                   0xF359 }, // ic_fluent_desktop_20_regular <- Set as wallpaper
        { FluentIcon::ZoomIn20,                      0xF8C4 }, // ic_fluent_zoom_in_20_regular <- Zoom in
        { FluentIcon::ZoomOut20,                     0xF8C6 }, // ic_fluent_zoom_out_20_regular <- Zoom out

        // Folder view
        { FluentIcon::ArrowExit20,        0xE0C4 }, // ic_fluent_arrow_exit_20_regular <- Exit folder view
        { FluentIcon::ArrowSort16,        0xF1AB }, // ic_fluent_arrow_sort_16_regular <- File sorting selector
        { FluentIcon::BatchConvert16,     0xF018D }, // ic_fluent_image_stack_16_regular <- Batch-convert toolbar button
        { FluentIcon::BatchConvert20,     0xF018E }, // ic_fluent_image_stack_20_regular <- Batch-convert menu action
        { FluentIcon::BookmarkAdd20,      0xF1E8 }, // ic_fluent_bookmark_add_20_regular <- Add bookmark
        { FluentIcon::BookmarkRemove20,   0xF368 }, // ic_fluent_dismiss_16_regular <- Remove bookmark
        { FluentIcon::Checkmark16,        0xE305 }, // ic_fluent_checkmark_16_regular <- Image-format filter
        { FluentIcon::ChevronUp16,        0xF2B5 }, // ic_fluent_chevron_up_16_regular <- Navigate to parent folder
        { FluentIcon::DocumentView20,     0xF488 }, // ic_fluent_image_20_regular <- Switch to document view
        { FluentIcon::Folder16,           0xE643 }, // ic_fluent_folder_16_regular <- Folder thumbnails and sorting
        { FluentIcon::FolderAdd,          0xF41C }, // ic_fluent_folder_add_20_regular <- Create folder
        { FluentIcon::Home20,             0xF480 }, // ic_fluent_home_20_regular <- Navigate home
        { FluentIcon::OpenOnlySelected20, 0xF697 }, // ic_fluent_select_object_20_regular <- Open only selected files
        { FluentIcon::PanelLeft20,        0xE8B0 }, // ic_fluent_panel_left_20_regular <- Toggle places panel

        // Floating notifications
        { FluentIcon::ArrowNext20,       0xF16A }, // ic_fluent_arrow_next_20_regular <- End-of-directory notification
        { FluentIcon::ArrowPrevious20,   0xF16C }, // ic_fluent_arrow_previous_20_regular <- Start-of-directory notification
        { FluentIcon::CheckmarkCircle20, 0xF298 }, // ic_fluent_checkmark_circle_20_regular <- Success notification
        { FluentIcon::ErrorCircle20,     0xF3F1 }, // ic_fluent_error_circle_20_regular <- Error notification
        { FluentIcon::Warning20,         0xF869 }, // ic_fluent_warning_20_regular <- Warning notification

        // Viewer overlays and panels
        { FluentIcon::ChevronDown12,  0xF2A1 }, // ic_fluent_chevron_down_12_regular <- Styled combo boxes and crop aspect-ratio selector
        { FluentIcon::ChevronLeft48,  0xF2AD }, // ic_fluent_chevron_left_48_regular <- Previous-image click zone
        { FluentIcon::ChevronRight48, 0xF2B3 }, // ic_fluent_chevron_right_48_regular <- Next-image click zone
        { FluentIcon::Crop48,         0xF02F }, // ic_fluent_crop_48_regular <- Crop panel header
        { FluentIcon::Pin20,          0xF601 }, // ic_fluent_pin_20_regular <- Pin main panel
        { FluentIcon::Edit20,         0xF493 }, // ic_fluent_image_edit_20_regular <- Unsaved edits

        // Settings sidebar
        { FluentIcon::BrainSparkle32,      0xF0B3A }, // ic_fluent_brain_sparkle_32_regular <- AI Upscale page
        { FluentIcon::ColorFill32,         0xF0BCB }, // ic_fluent_color_fill_32_regular <- Theme page
        { FluentIcon::Controls32,          0xE74A }, // ic_fluent_keyboard_16_regular <- Controls page
        { FluentIcon::Eye32,               0xEFC9 }, // ic_fluent_eye_32_regular <- View page
        { FluentIcon::Info32,              0xF0059 }, // ic_fluent_info_32_regular <- About page
        { FluentIcon::Scripts32,           0xF339 }, // ic_fluent_code_16_regular <- Scripts page
        { FluentIcon::Settings32,          0xEA94 }, // ic_fluent_settings_32_regular <- General page
        { FluentIcon::WrenchScrewdriver32, 0xF0461 }, // ic_fluent_wrench_screwdriver_32_regular <- Advanced page

        // Shared loading resources
        { FluentIcon::Clock24,        0xF2DE }, // ic_fluent_clock_24_regular <- Loading indicator
        { FluentIcon::ClockDismiss24, 0xE373 }, // ic_fluent_clock_dismiss_24_regular <- Loading-error indicator
    };
    return table;
}

// Fraction of the target box height used as the QFont pixel size. Fluent
// glyphs are drawn with internal padding, similar to most icon fonts; a
// factor of 1.0 undersizes them relative to the source PNGs, which were
// exported edge-to-edge.
constexpr qreal kGlyphFontSizeFactor = 1.0;

} // namespace

QString IconFontManager::fontFamily;
bool IconFontManager::initialized = false;

bool IconFontManager::init() {
    if (initialized)
        return !fontFamily.isEmpty();

    initialized = true;
    int fontId = QFontDatabase::addApplicationFont(":/res/fonts/FluentSystemIcons-Regular.ttf");
    if (fontId == -1) {
        qWarning() << "IconFontManager: failed to load FluentSystemIcons-Regular.ttf from resources";
        return false;
    }
    QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    if (families.isEmpty()) {
        qWarning() << "IconFontManager: font loaded but reports no family name";
        return false;
    }
    fontFamily = families.first();
    return true;
}

QPixmap IconFontManager::pixmap(FluentIcon icon, int sizePx, QColor color, qreal dpr) {
    if (fontFamily.isEmpty()) {
        qWarning() << "IconFontManager::pixmap() called before a successful init()";
        return QPixmap();
    }
    if (!codepointTable().contains(icon)) {
        qWarning() << "IconFontManager: no codepoint registered for this FluentIcon value";
        return QPixmap();
    }

    const QString cacheKey = QStringLiteral("iconfont:%1:%2:%3:%4")
        .arg(static_cast<int>(icon))
        .arg(sizePx)
        .arg(color.rgba(), 0, 16)
        .arg(dpr, 0, 'f', 2);

    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached))
        return cached;

    const int physicalSize = qRound(sizePx * dpr);
    QPixmap result(physicalSize, physicalSize);
    result.fill(Qt::transparent);
    result.setDevicePixelRatio(dpr);

    char32_t codepoint = codepointTable().value(icon);
    QString glyph = QString::fromUcs4(&codepoint, 1);

    QFont font(fontFamily);
    font.setPixelSize(qRound(sizePx * kGlyphFontSizeFactor));

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font);
    painter.setPen(color);

    // Qt::AlignCenter (previously used with drawText(QRectF, flags, text))
    // centers the glyph's font *advance box*, not its visible ink. Fluent
    // glyphs can have asymmetric left/right side bearings within that
    // advance box (chevrons in particular), so advance-box centering left
    // the visible arrow shifted off to one side inside the button. Centering
    // the glyph's tight ink bounding rect instead fixes this regardless of
    // bearing asymmetry.
    QFontMetricsF metrics(font);
    const QRectF inkRect = metrics.tightBoundingRect(glyph);
    const qreal drawX = (sizePx - inkRect.width()) / 2.0 - inkRect.left();
    const qreal drawY = (sizePx - inkRect.height()) / 2.0 - inkRect.top();
    painter.drawText(QPointF(drawX, drawY), glyph);
    painter.end();

    QPixmapCache::insert(cacheKey, result);
    return result;
}
