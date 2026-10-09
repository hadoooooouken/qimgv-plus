#pragma once

#include <QString>
#include <QStringList>

// Destination folders of the copy / move overlay, with the rules of the
// widget UI (CopyOverlay).

// Number of destinations; each has a digit shortcut 1 - 9.
inline constexpr int kMaxCopyTargets = 9;

// Destinations from the saved list (Settings::savedPaths()). While fewer than
// kMaxCopyTargets are saved and the list is empty or starts with an unset
// entry, it is replaced by homePath and the visible, writable folders in it
// (Windows profile folders without user files excluded), up to
// kMaxCopyTargets. Lists the folders of homePath on the calling thread.
[[nodiscard]] QStringList copyTargetsFrom(const QStringList &saved, const QString &homePath);

// targets without empty entries and repeated folders, in order, for saving.
[[nodiscard]] QStringList savableCopyTargets(const QStringList &targets);
