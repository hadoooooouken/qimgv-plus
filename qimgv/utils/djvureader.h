#pragma once

#include <QImage>
#include <QSize>
#include <QString>

struct DjvuRenderResult {
    QImage image;
    QSize originalSize;
    int pageCount = 0;
    int pageIndex = 0;
};

class DjvuReader {
public:
    // maxEdge <= 0 renders at native resolution. Otherwise djvudec's integer
    // subsampling is chosen so the decoded page is bounded approximately by
    // maxEdge without first allocating the full-resolution bitmap.
    static DjvuRenderResult renderPage(const QString &path, int page,
                                       int maxEdge = 0);
};
