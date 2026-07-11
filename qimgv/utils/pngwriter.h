#ifndef PNGWRITER_H
#define PNGWRITER_H

#include <QImage>
#include <QString>

bool savePngWithLibdeflate(const QImage &image, const QString &filePath, int compressionLevel);

#endif // PNGWRITER_H
