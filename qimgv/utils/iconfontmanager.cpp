#include "iconfontmanager.h"

#include <QDebug>
#include <QFontDatabase>
#include <QHash>
#include <QPainter>
#include <QPixmapCache>

namespace {

// Codepoints from microsoft/fluentui-system-icons, Regular weight
// (fonts/FluentSystemIcons-Regular.json @ main). Each entry names the
// upstream icon id it was picked from and the PNG file it replaces, so the
// mapping can be re-pointed to a different glyph without re-deriving the
// codepoint from scratch.
//
// NOTE: this is a draft mapping - semantically "close enough" picks were
// made for concepts without an exact Fluent equivalent (e.g. "fit window",
// "move file", "panorama mode"). Expect this table to be edited.
const QHash<FluentIcon, char32_t> &codepointTable() {
    static const QHash<FluentIcon, char32_t> table = {
        { FluentIcon::FolderAdd,                    0x0F41C }, // ic_fluent_folder_add_20_regular          <- add-folder.png
        { FluentIcon::ArrowMaximize,                0x0F15F }, // ic_fluent_arrow_maximize_20_regular       <- fit-window18.png
        { FluentIcon::ArrowAutofitWidth,            0x0E07C }, // ic_fluent_arrow_autofit_width_20_regular  <- fit-width18.png
        { FluentIcon::ArrowAutofitHeight,           0x0E077 }, // ic_fluent_arrow_autofit_height_20_regular <- fit-height-stretch18.png
        { FluentIcon::ZoomIn,                       0x0F8C4 }, // ic_fluent_zoom_in_20_regular               <- zoom-in18.png
        { FluentIcon::ZoomOriginal,                 0x0F70A }, // ic_fluent_ratio_one_to_one_20_regular      <- zoom-original18.png
        { FluentIcon::ZoomOut,                      0x0F8C6 }, // ic_fluent_zoom_out_20_regular              <- zoom-out18.png
        { FluentIcon::Color,                        0x0E3BC }, // ic_fluent_color_16_regular                 <- appearance16.png
        { FluentIcon::ArrowLeft,                    0x0F184 }, // ic_fluent_arrow_left_16_regular            <- back16.png
        { FluentIcon::Copy,                         0x0F32A }, // ic_fluent_copy_16_regular                  <- copy16.png (menuitem + overlay)
        { FluentIcon::Image,                        0x0F487 }, // ic_fluent_image_16_regular                 <- document-view16.png
        { FluentIcon::FlipHorizontal,                0x0E62B }, // ic_fluent_flip_horizontal_16_regular      <- flip-h16.png
        { FluentIcon::FlipVertical,                 0x0E631 }, // ic_fluent_flip_vertical_16_regular         <- flip-v16.png
        { FluentIcon::Folder,                       0x0E643 }, // ic_fluent_folder_16_regular                <- folder16.png
        { FluentIcon::Grid16,                       0x0E6C3 }, // ic_fluent_grid_16_regular                  <- folderview16.png
        { FluentIcon::Crop16,                       0x0F02C }, // ic_fluent_crop_16_regular                  <- image-crop16.png
        { FluentIcon::FolderArrowRight,              0x0E64B }, // ic_fluent_folder_arrow_right_16_regular   <- move16.png
        { FluentIcon::FolderOpen16,                 0x0F42D }, // ic_fluent_folder_open_16_regular           <- open16.png
        { FluentIcon::DocumentPrint,                 0xF002E }, // ic_fluent_document_print_20_regular       <- print16.png
        { FluentIcon::Resize16,                      0xF04B4 }, // ic_fluent_resize_16_regular               <- resize16.png
        { FluentIcon::ArrowRotateCounterclockwise,  0x0F187 }, // ic_fluent_arrow_rotate_counterclockwise_20_regular <- rotate-left16.png
        { FluentIcon::ArrowRotateClockwise,         0x0E0EC }, // ic_fluent_arrow_rotate_clockwise_16_regular <- rotate-right16.png
        { FluentIcon::Apps16,                       0x0F132 }, // ic_fluent_apps_16_regular                  <- run16.png (Open with)
        { FluentIcon::Settings16,                   0x0F6A8 }, // ic_fluent_settings_16_regular              <- settings16.png
        { FluentIcon::Delete16,                     0x0E47B }, // ic_fluent_delete_16_regular                <- trash16.png
        { FluentIcon::ImageMultiple16,               0x0E724 }, // ic_fluent_image_multiple_16_regular       <- view16.png (Panorama mode)
        { FluentIcon::Dismiss16,                    0x0F368 }, // ic_fluent_dismiss_16_regular               <- close-dim16.png / close16.png
        { FluentIcon::Edit16,                       0x0F3DC }, // ic_fluent_edit_16_regular                  <- edit16.png (rename)
        { FluentIcon::Info16,                       0x0F4A2 }, // ic_fluent_info_16_regular                  <- info16.png
        { FluentIcon::ArrowMove,                    0x0F8E5 }, // ic_fluent_arrow_move_20_regular            <- move16.png (copy/move overlay)
        { FluentIcon::ChevronLeft48,                0x0F2AD }, // ic_fluent_chevron_left_48_regular          <- arrow_left_50.png
        { FluentIcon::ChevronRight48,               0x0F2B3 }, // ic_fluent_chevron_right_48_regular         <- arrow_right_50.png
        { FluentIcon::Grid20,                       0x0F462 }, // ic_fluent_grid_20_regular                  <- folderview20.png
        { FluentIcon::FolderOpen20,                 0x0F42E }, // ic_fluent_folder_open_20_regular           <- open20.png
        { FluentIcon::Pin20,                        0x0F601 }, // ic_fluent_pin_20_regular                   <- pin-panel20.png
        { FluentIcon::ArrowExit20,                  0x0E0C4 }, // ic_fluent_arrow_exit_20_regular            <- quit16.png
        { FluentIcon::Settings20,                   0x0F6A9 }, // ic_fluent_settings_20_regular              <- settings20.png
        { FluentIcon::PanelLeft20,                  0x0E8B0 }, // ic_fluent_panel_left_20_regular            <- toggle-panel20.png
        { FluentIcon::ArrowUp16,                    0x0F1B4 }, // ic_fluent_arrow_up_16_regular              <- up16.png
        { FluentIcon::Add12,                        0x0F107 }, // ic_fluent_add_12_regular                   <- add-new12.png
        { FluentIcon::Home12,                       0x0E70E }, // ic_fluent_home_12_regular                  <- home12.png
        { FluentIcon::Subtract12,                   0x0EBCE }, // ic_fluent_subtract_12_regular              <- remove12.png
        { FluentIcon::Info24,                       0x0F4A4 }, // ic_fluent_info_24_regular                  <- about32.png
        { FluentIcon::Wrench24,                     0x0F8C1 }, // ic_fluent_wrench_24_regular                <- advanced32.png
        { FluentIcon::Color24,                      0x0F2F6 }, // ic_fluent_color_24_regular                 <- appearance32.png
        { FluentIcon::Settings24,                   0x0F6AA }, // ic_fluent_settings_24_regular              <- general32.png
        { FluentIcon::ResizeLarge24,                0x0EA17 }, // ic_fluent_resize_large_24_regular          <- scale32.png (AI Upscale)
        { FluentIcon::Keyboard24,                   0x0F4B9 }, // ic_fluent_keyboard_24_regular              <- shortcuts32.png
        { FluentIcon::WindowDevTools24,             0x0F8B9 }, // ic_fluent_window_dev_tools_24_regular      <- terminal32.png (Scripts)
        { FluentIcon::Eye24,                        0x0E5F3 }, // ic_fluent_eye_24_regular                   <- view32.png
        { FluentIcon::ChevronDown12,                0x0F2A1 }, // ic_fluent_chevron_down_12_regular          <- dropDownArrow.png
        { FluentIcon::ArrowSort16,                  0x0F1AB }, // ic_fluent_arrow_sort_16_regular            <- sorting-mode16.png
        { FluentIcon::Crop48,                       0x0F02F }, // ic_fluent_crop_48_regular                  <- image-crop48.png
        { FluentIcon::ArrowPrevious20,              0x0F16C }, // ic_fluent_arrow_previous_20_regular        <- dir_start20.png
        { FluentIcon::ArrowNext20,                  0x0F16A }, // ic_fluent_arrow_next_20_regular            <- dir_end20.png
        { FluentIcon::CheckmarkCircle16,            0x0F297 }, // ic_fluent_checkmark_circle_16_regular      <- success16.png
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
    painter.drawText(QRectF(0, 0, sizePx, sizePx), Qt::AlignCenter, glyph);
    painter.end();

    QPixmapCache::insert(cacheKey, result);
    return result;
}
