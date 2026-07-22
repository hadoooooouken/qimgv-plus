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
        { FluentIcon::BookmarkAdd20,                 0xF1E8 }, // ic_fluent_bookmark_add_20_regular          <- add-new12.png
        { FluentIcon::OpenWith20,                    0xF582 }, // ic_fluent_open_20_regular                  <- run16.png (Open with) (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::ArrowAutofitHeight20,         0x0E077 }, // ic_fluent_arrow_autofit_height_20_regular  <- fit-height-stretch18.png
        { FluentIcon::ArrowAutofitWidth20,          0x0E07C }, // ic_fluent_arrow_autofit_width_20_regular   <- fit-width18.png
        { FluentIcon::ArrowExit20,                  0x0E0C4 }, // ic_fluent_arrow_exit_20_regular            <- quit16.png
        { FluentIcon::ArrowLeft,                    0x0F15B }, // ic_fluent_arrow_left_20_regular            <- back16.png (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::ArrowExpand20,                 0xE0C5 }, // ic_fluent_arrow_expand_20_regular          <- fit-window18.png
        { FluentIcon::ArrowMove,                    0x0F8E5 }, // ic_fluent_arrow_move_20_regular            <- move16.png (copy/move overlay)
        { FluentIcon::ArrowNext20,                  0x0F16A }, // ic_fluent_arrow_next_20_regular            <- dir_end20.png
        { FluentIcon::ArrowPrevious20,              0x0F16C }, // ic_fluent_arrow_previous_20_regular        <- dir_start20.png
        { FluentIcon::ArrowRotateClockwise20,       0x0F185 }, // ic_fluent_arrow_rotate_clockwise_20_regular <- rotate-right16.png (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::ArrowRotateCounterclockwise20,0x0F187 }, // ic_fluent_arrow_rotate_counterclockwise_20_regular <- rotate-left16.png
        { FluentIcon::ArrowSort16,                  0x0F1AB }, // ic_fluent_arrow_sort_16_regular            <- sorting-mode16.png
        { FluentIcon::ArrowUp16,                    0x0F1B4 }, // ic_fluent_arrow_up_16_regular              <- up16.png
        { FluentIcon::Blur20,                        0xF8FA }, // ic_fluent_blur_20_regular                  <- cas settings
        { FluentIcon::Checkmark16,                   0xE305 }, // ic_fluent_checkmark_16_regular             <- folderview image formats
        { FluentIcon::CheckmarkCircle16,            0x0F297 }, // ic_fluent_checkmark_circle_16_regular      <- success16.png
        { FluentIcon::ChevronDown12,                0x0F2A1 }, // ic_fluent_chevron_down_12_regular          <- dropDownArrow.png
        { FluentIcon::ChevronLeft48,                0x0F2AD }, // ic_fluent_chevron_left_48_regular          <- arrow_left_50.png
        { FluentIcon::ChevronRight48,               0x0F2B3 }, // ic_fluent_chevron_right_48_regular         <- arrow_right_50.png
        { FluentIcon::Adjustments20,                 0xE00B }, // ic_fluent_dark_theme_20_regular  0xE452    <- appearance16.png (bumped 16->20, only used at 20px in contextmenu.cpp)
                                                               // ic_fluent_add_subtract_circle_20_regular 0xE00B
        { FluentIcon::Color24,                       0xF2F6 }, // ic_fluent_color_24_regular                 <- appearance32.png
        { FluentIcon::CopyAdd20,                     0xE41F }, // ic_fluent_copy_add_20_regular              <- copy16.png (menuitem + overlay) (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::Crop20,                       0x0E42B }, // ic_fluent_crop_20_regular                  <- image-crop16.png (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::Crop48,                       0x0F02F }, // ic_fluent_crop_48_regular                  <- image-crop48.png
        { FluentIcon::Delete16,                     0x0F34C }, // ic_fluent_delete_20_regular                <- trash16.png (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::Dismiss16,                    0x0F368 }, // ic_fluent_dismiss_16_regular               <- close-dim16.png / close16.png
        { FluentIcon::Dismiss20,                    0x0F369 }, // ic_fluent_dismiss_20_regular               <- close-dim16.png / close16.png (20px sibling of Dismiss16, for call sites rendered at 20px)
        { FluentIcon::DocumentView20,                0xF488 }, // ic_fluent_image_20_regular                 <- folder view / document view / to image viewer
        { FluentIcon::Eye24,                        0x0E5F3 }, // ic_fluent_eye_24_regular                   <- view32.png
        { FluentIcon::FlipHorizontal20,             0xF02A1 }, // ic_fluent_arrow_bidirectional_left_right_20_regular      <- flip-h16.png (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::FlipVertical20,                0xE084 }, // ic_fluent_arrow_bidirectional_up_down_20_regular         <- flip-v16.png (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::Folder16,                      0xE643 }, // ic_fluent_folder_16_regular                <- folderview - folder thumb
        { FluentIcon::Folder20,                      0xF418 }, // ic_fluent_folder_20_regular                <- folder16.png (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::ShowInFolder20,                0xF42E }, // ic_fluent_folder_open_20_regular           <- contextmenu.cpp Show in folder
        { FluentIcon::FolderAdd,                    0x0F41C }, // ic_fluent_folder_add_20_regular            <- add-folder.png
        { FluentIcon::Move20,                        0xE422 }, // ic_fluent_copy_arrow_right_20_regular    <- move16.png (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::FolderOpen16,                 0x0F42E }, // ic_fluent_folder_open_20_regular           <- open16.png (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::FolderOpen20,                 0x0F42E }, // ic_fluent_folder_open_20_regular           <- open20.png
        { FluentIcon::Grid16,                        0xE6C3 }, // ic_fluent_grid_16_regular                  <- folderview16.png (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::Grid20,                       0x0F462 }, // ic_fluent_grid_20_regular                  <- folderview20.png
        { FluentIcon::Home20,                        0xF480 }, // ic_fluent_home_20_regular                  <- home12.png
        { FluentIcon::Wallpaper20,                   0xF359 }, // ic_fluent_desktop_20_regular               <- document-view16.png (2 of 3 call sites render at 20px; see Image16 for the 16px one)
        { FluentIcon::Image16,                      0x0F487 }, // ic_fluent_image_16_regular                 <- document-view16.png (16px sibling of Image, for the one call site rendered at 16px)
        { FluentIcon::ImageInfo20,                   0xF4A3 }, // ic_fluent_info_20_regular                  <- contextmenu.cpp Image info
        { FluentIcon::Panorama20,                   0xF08F5 }, // ic_fluent_planet_20_regular                <- view16.png (Panorama mode) (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::Info16,                       0x0F4A2 }, // ic_fluent_info_16_regular                  <- info16.png
        { FluentIcon::Info24,                       0x0F4A4 }, // ic_fluent_info_24_regular                  <- about32.png
        { FluentIcon::Keyboard24,                   0x0F4B9 }, // ic_fluent_keyboard_24_regular              <- shortcuts32.png
        { FluentIcon::OpenOnlySelected20,            0xF697 }, // ic_fluent_select_object_20_regular         <- folderview Open only selected
        { FluentIcon::PanelLeft20,                  0x0E8B0 }, // ic_fluent_panel_left_20_regular            <- toggle-panel20.png
        { FluentIcon::Pin20,                        0x0F601 }, // ic_fluent_pin_20_regular                   <- pin-panel20.png
        { FluentIcon::Print20,                       0xF62A }, // ic_fluent_print_20_regular                 <- print16.png
        { FluentIcon::Rename20,                     0xF0A39 }, // ic_fluent_rename_a_20_regular              <- edit16.png (rename) (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::Resize20,                      0xF66C }, // ic_fluent_resize_20_regular                <- resize16.png (bumped 16->20, only used at 20px in contextmenu.cpp)
        { FluentIcon::AiUpscale20,                  0xF0799 }, // ic_fluent_brain_sparkle_20_regular         <- contextmenu.cpp (AI Upscale)
        { FluentIcon::AiUpscale24,                  0xF0B38 }, // ic_fluent_brain_sparkle_24_regular         <- scale32.png (AI Upscale)
        { FluentIcon::Settings16,                   0x0F6A8 }, // ic_fluent_settings_16_regular              <- settings16.png
        { FluentIcon::Settings20,                   0x0F6A9 }, // ic_fluent_settings_20_regular              <- settings20.png
        { FluentIcon::Settings24,                   0x0F6AA }, // ic_fluent_settings_24_regular              <- general32.png
        { FluentIcon::BookmarkRemove20,              0xF368 }, // ic_fluent_dismiss_16_regular               <- remove16.png
        { FluentIcon::WindowDevTools24,             0x0F8B9 }, // ic_fluent_window_dev_tools_24_regular      <- terminal32.png (Scripts)
        { FluentIcon::Wrench24,                     0x0F8C1 }, // ic_fluent_wrench_24_regular                <- advanced32.png
        { FluentIcon::ZoomIn20,                     0x0F8C4 }, // ic_fluent_zoom_in_20_regular               <- zoom-in18.png
        { FluentIcon::ZoomOriginal20,               0x0F70A }, // ic_fluent_ratio_one_to_one_20_regular      <- zoom-original18.png
        { FluentIcon::ZoomOut20,                    0x0F8C6 }, // ic_fluent_zoom_out_20_regular              <- zoom-out18.png
        { FluentIcon::ChevronDown20,                 0xF2A3 }, // ic_fluent_chevron_down_20_regular          <- contextmenu.cpp more
        { FluentIcon::ChevronUp20,                   0xF2B6 }, // ic_fluent_chevron_up_20_regular            <- contextmenu.cpp more (expanded state)
        { FluentIcon::BatchConvert16,               0xF018D }, // ic_fluent_image_stack_16_regular           <- folderview button
        { FluentIcon::BatchConvert20,               0xF018E }, // ic_fluent_image_stack_20_regular           <- folderview menu
        { FluentIcon::Settings32,                    0xEA94 }, // ic_fluent_settings_32_regular              <- general32.png
        { FluentIcon::Eye32,                         0xEFC9 }, // ic_fluent_eye_32_regular                   <- view32.png
        { FluentIcon::ColorFill32,                  0xF0BCB }, // ic_fluent_color_fill_32_regular            <- appearance32.png
        { FluentIcon::Controls32,                    0xE74A }, // ic_fluent_keyboard_16_regular              <- shortcuts32.png
        { FluentIcon::Scripts32,                     0xF339 }, // ic_fluent_code_16_regular                  <- terminal32.png
        { FluentIcon::WrenchScrewdriver32,          0xF0461 }, // ic_fluent_wrench_screwdriver_32_regular    <- advanced32.png
        { FluentIcon::BrainSparkle32,               0xF0B3A }, // ic_fluent_brain_sparkle_32_regular         <- scale32.png
        { FluentIcon::Info32,                       0xF0059 }, // ic_fluent_info_32_regular                  <- about32.png
        { FluentIcon::ChevronUp16,                   0xF2B5 }, // ic_fluent_chevron_up_16_regular            <- up16.png
        { FluentIcon::CheckmarkCircle20,             0xF298 }, // ic_fluent_checkmark_circle_20_regular      <- floating message success

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
