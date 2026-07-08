#include "fsentry.h"

namespace fs = std::filesystem;

FSEntry::FSEntry() {
}

FSEntry::FSEntry(const QString &path) {
    std::error_code ec;
    fs::directory_entry stdEntry(toStdString(path), ec);
    if(ec) {
        // File is transiently unreadable (still being written/copied,
        // share violation, etc). Leave entry default-constructed; the
        // watcher will fire again once the write settles.
        return;
    }

    QString name = QString::fromStdString(stdEntry.path().filename().generic_string());

    if(stdEntry.is_directory(ec)) {
        if(ec) return;
        this->name = name;
        this->path = path;
        this->isDirectory = true;
    } else {
        if(ec) return;
        this->name = name;
        this->path = path;
        this->isDirectory = false;
        this->size = stdEntry.file_size(ec);
        if(ec) this->size = 0;
        this->modifyTime = stdEntry.last_write_time(ec);
        // ec left set here just means modifyTime stays default - next
        // watcher event for this file will pick it up correctly
    }
}

FSEntry::FSEntry( QString _path, QString _name, std::uintmax_t _size, std::filesystem::file_time_type _modifyTime, bool _isDirectory)
    : path(_path),
      name(_name),
      size(_size),
      modifyTime(_modifyTime),
      isDirectory(_isDirectory)
{
}
FSEntry::FSEntry( QString _path, QString _name, std::uintmax_t _size, bool _isDirectory)
    : path(_path),
      name(_name),
      size(_size),
      isDirectory(_isDirectory)
{
}
FSEntry::FSEntry( QString _path, QString _name, bool _isDirectory)
    : path(_path),
      name(_name),
      isDirectory(_isDirectory)
{
}
bool FSEntry::operator==(const QString &anotherPath) const {
#if defined(_WIN32) || defined(Q_OS_WIN) || defined(Q_OS_WIN32)
    return this->path.compare(anotherPath, Qt::CaseInsensitive) == 0;
#else
    return this->path == anotherPath;
#endif
}
