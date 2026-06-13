#include "cmdoptionsrunner.h"

void CmdOptionsRunner::generateThumbs(QString dirPath, int size) {
    if(size <= 50 || size > 400) {
        qWarning() << "Error: Invalid thumbnail size.";
        qInfo() << "Please specify a value between [50, 400].";
        qInfo() << "Example:  qimgv --gen-thumbs=/home/user/Pictures/ --gen-thumbs-size=120";
        QCoreApplication::exit(1);
        return;
    }

    Thumbnailer th;
    DirectoryManager dm;
    if(!dm.setDirectoryRecursive(dirPath)) {
        qWarning() << "Error: Invalid path.";
        QCoreApplication::exit(1);
        return;
    }

    auto list = dm.fileList();

    qInfo() << "\nDirectory:" << dirPath;
    qInfo() << "File count:" << list.size();
    qInfo() << "Size limit:" << size << "x" << size << "px";
    qInfo() << "Generating thumbnails...";

    for(const auto &path : std::as_const(list))
        th.getThumbnailAsync(path, size, false, false);

    th.waitForDone();
    qInfo() << "\nDone.";
    QCoreApplication::quit();
}

void CmdOptionsRunner::showBuildOptions() {
    QStringList features;
#ifdef USE_EXIV2
    features << "USE_EXIV2";
#endif
    qInfo() << "\nEnabled build options:";
    if(!features.count())
        qInfo() << "   --";
    for(int i = 0; i < features.count(); i++)
        qInfo() << "   " << features.at(i);
    QCoreApplication::quit();
}

