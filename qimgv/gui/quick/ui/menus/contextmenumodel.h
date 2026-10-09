#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

#include <vector>

class IActionDispatcher;

// One row of the context menu as QML shows it.
struct ContextMenuEntry {
    // ContextMenuModel::Kind and ContextMenuModel::Tone values.
    int kind = 0;
    // Action run by the row ("s:<name>" for scripts); empty for rows that
    // run none.
    QString action;
    QString text;
    // FluentIcons::FluentIcon value.
    int icon = 0;
    int tone = 0;
    // The action's shortcut as ActionManager stores it; empty when none.
    QString shortcut;
    bool enabled = true;
    bool shown = true;

    friend bool operator==(const ContextMenuEntry &, const ContextMenuEntry &) = default;
};

// Rows of one part of the context menu. Roles: entryKind, entryAction,
// entryText, entryIcon, entryTone, entryShortcut, entryEnabled, entryShown. Rows are updated in place while their
// number stays the same, so open menus keep their items.
class ContextMenuEntryList final : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        KindRole = Qt::UserRole + 1,
        ActionRole,
        TextRole,
        IconRole,
        ToneRole,
        ShortcutRole,
        EnabledRole,
        ShownRole,
    };

    using QAbstractListModel::QAbstractListModel;

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] const std::vector<ContextMenuEntry> &entries() const;
    void update(std::vector<ContextMenuEntry> entries);

private:
    std::vector<ContextMenuEntry> mEntries;
};

// Structure and state of the Qt Quick context menu
// (ViewerContextMenu.qml), with the rows and rules of the widget ContextMenu:
// - a row of zoom buttons and a row of transform buttons, then the action
//   rows with their shortcuts; "More" expands the rarely used rows in place
//   (collapsed again whenever the menu opens); "Open with..." lists the
//   scripts that have a command, followed by "Configure menu";
// - rows that act on the current image are disabled while none is shown;
//   CAS settings is listed only while an image is shown with the CAS filter;
//   trash and delete use the destructive tones;
// - the menu action (ActionManager::contextMenu) opens the menu at the
//   pointer in the document view, or closes an open one; it does not open
//   while the viewer takes no input (crop mode), and the folder view closes
//   it.
// Shortcuts come from the action system (IActionDispatcher::shortcutFor),
// so they are always the ones ActionManager uses; refreshShortcuts() reads
// them again after edits.
//
// Owned by the Quick UI host; the application side (QuickContextMenuActions)
// feeds it the viewer state and the scripts. GUI thread only.
class ContextMenuModel final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the Quick UI host")
    Q_PROPERTY(QAbstractItemModel *zoomButtons READ zoomButtons CONSTANT FINAL)
    Q_PROPERTY(QAbstractItemModel *transformButtons READ transformButtons CONSTANT FINAL)
    Q_PROPERTY(QAbstractItemModel *items READ items CONSTANT FINAL)
    Q_PROPERTY(QAbstractItemModel *scripts READ scripts CONSTANT FINAL)
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged FINAL)
    Q_PROPERTY(bool moreExpanded READ isMoreExpanded NOTIFY moreExpandedChanged FINAL)

public:
    enum class Kind { Action, Separator, Expander, Submenu };
    Q_ENUM(Kind)
    enum class Tone { Normal, Trash, Danger };
    Q_ENUM(Tone)

    // dispatcher must outlive the model.
    explicit ContextMenuModel(IActionDispatcher &dispatcher, QObject *parent = nullptr);

    [[nodiscard]] QAbstractItemModel *zoomButtons();
    [[nodiscard]] QAbstractItemModel *transformButtons();
    [[nodiscard]] QAbstractItemModel *items();
    [[nodiscard]] QAbstractItemModel *scripts();
    [[nodiscard]] const std::vector<ContextMenuEntry> &zoomEntries() const;
    [[nodiscard]] const std::vector<ContextMenuEntry> &transformEntries() const;
    [[nodiscard]] const std::vector<ContextMenuEntry> &itemEntries() const;
    [[nodiscard]] const std::vector<ContextMenuEntry> &scriptEntries() const;

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] bool isMoreExpanded() const;

    // --- state of the window and the viewer -------------------------------
    void setFolderViewActive(bool active);
    void setInteractionEnabled(bool enabled);
    void setImageDisplayed(bool displayed);
    void setCasFilterActive(bool active);
    // Names of the scripts that have a command, in menu order.
    void setScripts(const QStringList &names);
    void refreshShortcuts();

    // The menu action: opens the menu (QML shows it at the pointer) or
    // closes the open one.
    void toggle();

    // --- QML ----------------------------------------------------------------
    // The menu was closed (by a row, a click outside or Escape).
    Q_INVOKABLE void close();
    // Closes the menu and runs action.
    Q_INVOKABLE void trigger(const QString &action);
    Q_INVOKABLE void toggleMore();
    // "Configure menu" of the scripts submenu.
    Q_INVOKABLE void configureScripts();

signals:
    void openChanged();
    void moreExpandedChanged();
    void scriptSettingsRequested();

private:
    void rebuild();
    [[nodiscard]] QString shortcutFor(const QString &action) const;

    IActionDispatcher &mDispatcher;
    ContextMenuEntryList mZoomButtons;
    ContextMenuEntryList mTransformButtons;
    ContextMenuEntryList mItems;
    ContextMenuEntryList mScripts;
    QStringList mScriptNames;
    bool mOpen = false;
    bool mMoreExpanded = false;
    bool mFolderViewActive = false;
    bool mInteractionEnabled = true;
    bool mImageDisplayed = false;
    bool mCasFilterActive = false;
};
