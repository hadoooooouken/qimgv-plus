#include "copytargetlist.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

namespace {
using namespace Qt::StringLiterals;

// First character of a saved entry that is a placeholder, not a folder.
constexpr QChar kPlaceholderPrefix = u'@';
// First character of hidden folder names.
constexpr QChar kHiddenPrefix = u'.';

// Windows profile folders that hold no user files.
bool isExcludedFolder(const QString &name) {
    static const QStringList excluded{u"3D Objects"_s, u"Contacts"_s, u"Favorites"_s,
                                      u"Links"_s,      u"Saved Games"_s, u"Searches"_s};
    return excluded.contains(name);
}

bool isUnset(const QStringList &paths) {
    return paths.isEmpty() || paths.constFirst().isEmpty() ||
           paths.constFirst().front() == kPlaceholderPrefix;
}
} // namespace

QStringList copyTargetsFrom(const QStringList &saved, const QString &homePath) {
    QStringList paths = saved;
    if (paths.size() >= kMaxCopyTargets)
        return paths;
    if (isUnset(paths))
        paths = {homePath};
    if (paths.size() != 1 || paths.constFirst() != homePath)
        return paths;

    const QFileInfoList entries =
        QDir(homePath).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                     QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &entry : entries) {
        if (paths.size() >= kMaxCopyTargets)
            break;
        const QString name = entry.fileName();
        if (name.startsWith(kHiddenPrefix) || isExcludedFolder(name))
            continue;
        const QString path = homePath + u'/' + name;
        if (QFileInfo(path).permission(QFile::WriteUser | QFile::ReadGroup))
            paths << path;
    }
    return paths;
}

QStringList savableCopyTargets(const QStringList &targets) {
    QStringList result;
    QSet<QString> seen;
    for (const QString &target : targets) {
        if (target.isEmpty() || seen.contains(target))
            continue;
        seen.insert(target);
        result << target;
    }
    return result;
}
