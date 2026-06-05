#include "fsentry.h"

FSEntry::FSEntry() {
}

FSEntry::FSEntry(const QString &path) {
    std::filesystem::directory_entry stdEntry(toStdString(path));
    QString name = QString::fromStdString(stdEntry.path().filename().generic_string());
    if(stdEntry.is_directory()) {
        try {
            this->name = name;
            this->path = path;
            this->isDirectory = true;
        } catch (const std::filesystem::filesystem_error &) { }
    } else {
        try {
            this->name = name;
            this->path = path;
            this->isDirectory = false;
            this->size = stdEntry.file_size();
            this->modifyTime = stdEntry.last_write_time();
        } catch (const std::filesystem::filesystem_error &) { }
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
