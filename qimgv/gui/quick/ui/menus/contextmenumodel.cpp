#include "contextmenumodel.h"

#include <QCoreApplication>
#include <QDebug>

#include <span>

#include "gui/quick/bridges/actiondispatcher.h"
#include "utils/fluenticon.h"

namespace {
using namespace Qt::StringLiterals;
using Kind = ContextMenuModel::Kind;
using Tone = ContextMenuModel::Tone;

// Translation context of the widget ContextMenu, so its translations apply.
constexpr char kTranslationContext[] = "ContextMenu";
// Actions of scripts are their names with this prefix (ActionManager).
constexpr auto kScriptActionPrefix = "s:"_L1;

// Which rows a state change shows or enables.
enum class Visibility { Always, InMore, CasOnly };

struct EntrySpec {
    Kind kind = Kind::Action;
    const char *action = "";
    const char *text = "";
    FluentIcon icon = FluentIcon::Settings20;
    bool requiresImage = false;
    Visibility visibility = Visibility::Always;
    Tone tone = Tone::Normal;
};

constexpr EntrySpec kZoomButtons[] = {
    {.action = "fitWindow", .text = QT_TRANSLATE_NOOP("ContextMenu", "Fit to window"),
     .icon = FluentIcon::ArrowExpand20},
    {.action = "fitWidth", .text = QT_TRANSLATE_NOOP("ContextMenu", "Fit to width"),
     .icon = FluentIcon::ArrowAutofitWidth20},
    {.action = "fitHeight", .text = QT_TRANSLATE_NOOP("ContextMenu", "Fit to height"),
     .icon = FluentIcon::ArrowAutofitHeight20},
    {.action = "fitNormal", .text = QT_TRANSLATE_NOOP("ContextMenu", "Original size"),
     .icon = FluentIcon::ZoomOriginal20},
    {.action = "zoomIn", .text = QT_TRANSLATE_NOOP("ContextMenu", "Zoom in"),
     .icon = FluentIcon::ZoomIn20},
    {.action = "zoomOut", .text = QT_TRANSLATE_NOOP("ContextMenu", "Zoom out"),
     .icon = FluentIcon::ZoomOut20},
};

constexpr EntrySpec kTransformButtons[] = {
    {.action = "rotateLeft", .text = QT_TRANSLATE_NOOP("ContextMenu", "Rotate left"),
     .icon = FluentIcon::ArrowRotateCounterclockwise20, .requiresImage = true},
    {.action = "rotateRight", .text = QT_TRANSLATE_NOOP("ContextMenu", "Rotate right"),
     .icon = FluentIcon::ArrowRotateClockwise20, .requiresImage = true},
    {.action = "flipV", .text = QT_TRANSLATE_NOOP("ContextMenu", "Flip vertical"),
     .icon = FluentIcon::FlipVertical20, .requiresImage = true},
    {.action = "flipH", .text = QT_TRANSLATE_NOOP("ContextMenu", "Flip horizontal"),
     .icon = FluentIcon::FlipHorizontal20, .requiresImage = true},
    {.action = "crop", .text = QT_TRANSLATE_NOOP("ContextMenu", "Crop"),
     .icon = FluentIcon::Crop20, .requiresImage = true},
    {.action = "resize", .text = QT_TRANSLATE_NOOP("ContextMenu", "Resize"),
     .icon = FluentIcon::Resize20, .requiresImage = true},
};

constexpr EntrySpec kItems[] = {
    {.action = "colorAdjustments", .text = QT_TRANSLATE_NOOP("ContextMenu", "Color adjustments"),
     .icon = FluentIcon::Adjustments20, .requiresImage = true},
    {.action = "togglePanorama", .text = QT_TRANSLATE_NOOP("ContextMenu", "Panorama mode"),
     .icon = FluentIcon::Panorama20, .requiresImage = true},
    {.action = "toggleUpscayl", .text = QT_TRANSLATE_NOOP("ContextMenu", "AI Upscale"),
     .icon = FluentIcon::AiUpscale20, .requiresImage = true},
    {.action = "casSettings", .text = QT_TRANSLATE_NOOP("ContextMenu", "CAS Settings"),
     .icon = FluentIcon::Blur20, .visibility = Visibility::CasOnly},
    {.kind = Kind::Separator},
    {.action = "copyFile", .text = QT_TRANSLATE_NOOP("ContextMenu", "Quick copy"),
     .icon = FluentIcon::CopyAdd20, .requiresImage = true},
    {.action = "moveFile", .text = QT_TRANSLATE_NOOP("ContextMenu", "Quick move"),
     .icon = FluentIcon::Move20, .requiresImage = true},
    {.action = "folderView", .text = QT_TRANSLATE_NOOP("ContextMenu", "Folder View"),
     .icon = FluentIcon::Grid20},
    {.action = "showInDirectory", .text = QT_TRANSLATE_NOOP("ContextMenu", "Show in folder"),
     .icon = FluentIcon::ShowInFolder20, .requiresImage = true},
    {.action = "toggleImageInfo", .text = QT_TRANSLATE_NOOP("ContextMenu", "Image info"),
     .icon = FluentIcon::Info20, .requiresImage = true},
    {.action = "openSettings", .text = QT_TRANSLATE_NOOP("ContextMenu", "Settings"),
     .icon = FluentIcon::Settings20},
    {.kind = Kind::Separator},
    {.kind = Kind::Expander, .text = QT_TRANSLATE_NOOP("ContextMenu", "More"),
     .icon = FluentIcon::ChevronDown20},
    {.kind = Kind::Submenu, .text = QT_TRANSLATE_NOOP("ContextMenu", "Open with..."),
     .icon = FluentIcon::OpenWith20, .requiresImage = true, .visibility = Visibility::InMore},
    {.action = "renameFile", .text = QT_TRANSLATE_NOOP("ContextMenu", "Rename"),
     .icon = FluentIcon::Rename20, .requiresImage = true, .visibility = Visibility::InMore},
    {.action = "setWallpaper", .text = QT_TRANSLATE_NOOP("ContextMenu", "Set as wallpaper"),
     .icon = FluentIcon::Wallpaper20, .requiresImage = true, .visibility = Visibility::InMore},
    {.action = "print", .text = QT_TRANSLATE_NOOP("ContextMenu", "Print"),
     .icon = FluentIcon::Print20, .visibility = Visibility::InMore},
    {.kind = Kind::Separator, .visibility = Visibility::InMore},
    {.action = "moveToTrash", .text = QT_TRANSLATE_NOOP("ContextMenu", "Move to trash"),
     .icon = FluentIcon::Delete20, .requiresImage = true, .visibility = Visibility::InMore,
     .tone = Tone::Trash},
    {.action = "removeFile", .text = QT_TRANSLATE_NOOP("ContextMenu", "Delete permanently"),
     .icon = FluentIcon::Dismiss20, .requiresImage = true, .visibility = Visibility::InMore,
     .tone = Tone::Danger},
};
} // namespace

//------------------------------------------------------------------------------
// ContextMenuEntryList
//------------------------------------------------------------------------------
int ContextMenuEntryList::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(mEntries.size());
}

QVariant ContextMenuEntryList::data(const QModelIndex &index, int role) const {
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const ContextMenuEntry &entry = mEntries.at(static_cast<size_t>(index.row()));
    switch (role) {
    case KindRole: return entry.kind;
    case ActionRole: return entry.action;
    case TextRole: return entry.text;
    case IconRole: return entry.icon;
    case ToneRole: return entry.tone;
    case ShortcutRole: return entry.shortcut;
    case EnabledRole: return entry.enabled;
    case ShownRole: return entry.shown;
    default: return {};
    }
}

QHash<int, QByteArray> ContextMenuEntryList::roleNames() const {
    // Prefixed, so the roles do not shadow properties of the delegates
    // (text, enabled, icon, action).
    return {
        {KindRole, "entryKind"},         {ActionRole, "entryAction"},
        {TextRole, "entryText"},         {IconRole, "entryIcon"},
        {ToneRole, "entryTone"},         {ShortcutRole, "entryShortcut"},
        {EnabledRole, "entryEnabled"},   {ShownRole, "entryShown"},
    };
}

const std::vector<ContextMenuEntry> &ContextMenuEntryList::entries() const {
    return mEntries;
}

void ContextMenuEntryList::update(std::vector<ContextMenuEntry> entries) {
    if (entries.size() != mEntries.size()) {
        beginResetModel();
        mEntries = std::move(entries);
        endResetModel();
        return;
    }
    for (size_t row = 0; row < entries.size(); ++row) {
        if (entries[row] == mEntries[row])
            continue;
        mEntries[row] = std::move(entries[row]);
        const QModelIndex changed = index(static_cast<int>(row));
        emit dataChanged(changed, changed);
    }
}

//------------------------------------------------------------------------------
// ContextMenuModel
//------------------------------------------------------------------------------
ContextMenuModel::ContextMenuModel(IActionDispatcher &dispatcher, QObject *parent)
    : QObject(parent), mDispatcher(dispatcher) {
    rebuild();
}

QAbstractItemModel *ContextMenuModel::zoomButtons() {
    return &mZoomButtons;
}

QAbstractItemModel *ContextMenuModel::transformButtons() {
    return &mTransformButtons;
}

QAbstractItemModel *ContextMenuModel::items() {
    return &mItems;
}

QAbstractItemModel *ContextMenuModel::scripts() {
    return &mScripts;
}

const std::vector<ContextMenuEntry> &ContextMenuModel::zoomEntries() const {
    return mZoomButtons.entries();
}

const std::vector<ContextMenuEntry> &ContextMenuModel::transformEntries() const {
    return mTransformButtons.entries();
}

const std::vector<ContextMenuEntry> &ContextMenuModel::itemEntries() const {
    return mItems.entries();
}

const std::vector<ContextMenuEntry> &ContextMenuModel::scriptEntries() const {
    return mScripts.entries();
}

bool ContextMenuModel::isOpen() const {
    return mOpen;
}

bool ContextMenuModel::isMoreExpanded() const {
    return mMoreExpanded;
}

//------------------------------------------------------------------------------
void ContextMenuModel::setFolderViewActive(bool active) {
    mFolderViewActive = active;
    if (active)
        close();
}

void ContextMenuModel::setInteractionEnabled(bool enabled) {
    mInteractionEnabled = enabled;
    if (!enabled)
        close();
}

void ContextMenuModel::setImageDisplayed(bool displayed) {
    if (mImageDisplayed == displayed)
        return;
    mImageDisplayed = displayed;
    rebuild();
}

void ContextMenuModel::setCasFilterActive(bool active) {
    if (mCasFilterActive == active)
        return;
    mCasFilterActive = active;
    rebuild();
}

void ContextMenuModel::setScripts(const QStringList &names) {
    if (mScriptNames == names)
        return;
    mScriptNames = names;
    rebuild();
}

void ContextMenuModel::refreshShortcuts() {
    rebuild();
}

void ContextMenuModel::toggle() {
    if (mOpen) {
        close();
        return;
    }
    if (mFolderViewActive || !mInteractionEnabled)
        return;
    if (mMoreExpanded) {
        mMoreExpanded = false;
        rebuild();
        emit moreExpandedChanged();
    }
    mOpen = true;
    emit openChanged();
}

//------------------------------------------------------------------------------
void ContextMenuModel::close() {
    if (!mOpen)
        return;
    mOpen = false;
    emit openChanged();
}

void ContextMenuModel::trigger(const QString &action) {
    close();
    if (!mDispatcher.invoke(action))
        qWarning() << "ContextMenuModel: unknown action" << action;
}

void ContextMenuModel::toggleMore() {
    mMoreExpanded = !mMoreExpanded;
    rebuild();
    emit moreExpandedChanged();
}

void ContextMenuModel::configureScripts() {
    close();
    emit scriptSettingsRequested();
}

//------------------------------------------------------------------------------
void ContextMenuModel::rebuild() {
    const auto build = [this](std::span<const EntrySpec> specs) {
        std::vector<ContextMenuEntry> entries;
        entries.reserve(specs.size());
        for (const EntrySpec &spec : specs) {
            const QString action = QString::fromLatin1(spec.action);
            bool shown = true;
            if (spec.visibility == Visibility::InMore)
                shown = mMoreExpanded;
            else if (spec.visibility == Visibility::CasOnly)
                shown = mImageDisplayed && mCasFilterActive;
            FluentIcon icon = spec.icon;
            if (spec.kind == Kind::Expander && mMoreExpanded)
                icon = FluentIcon::ChevronUp20;
            entries.push_back({
                .kind = static_cast<int>(spec.kind),
                .action = action,
                .text = spec.kind == Kind::Separator
                            ? QString()
                            : QCoreApplication::translate(kTranslationContext, spec.text),
                .icon = static_cast<int>(icon),
                .tone = static_cast<int>(spec.tone),
                .shortcut = shortcutFor(action),
                .enabled = !spec.requiresImage || mImageDisplayed,
                .shown = shown,
            });
        }
        return entries;
    };
    mZoomButtons.update(build(kZoomButtons));
    mTransformButtons.update(build(kTransformButtons));
    mItems.update(build(kItems));

    std::vector<ContextMenuEntry> scripts;
    scripts.reserve(static_cast<size_t>(mScriptNames.size()));
    for (const QString &name : std::as_const(mScriptNames)) {
        const QString action = kScriptActionPrefix + name;
        scripts.push_back({
            .kind = static_cast<int>(Kind::Action),
            .action = action,
            .text = name,
            .icon = static_cast<int>(FluentIcon::ShowInFolder20),
            .tone = static_cast<int>(Tone::Normal),
            .shortcut = shortcutFor(action),
        });
    }
    mScripts.update(std::move(scripts));
}

QString ContextMenuModel::shortcutFor(const QString &action) const {
    return action.isEmpty() ? QString() : mDispatcher.shortcutFor(action);
}
