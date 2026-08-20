#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <QtTypes>

#include "utils/decodecontext.h"

struct DjvuDecodeLimits {
    quint64 maximumInputBytes = 0;
    quint64 memoryBudgetBytes = 0;
    int maximumEdge = 0;

    static DjvuDecodeLimits fromMemoryLimitMiB(int memoryLimitMiB,
                                                int maximumEdge) noexcept;
    bool isValid() const noexcept;
};

struct DjvuRenderResult {
    QImage image;
    QSize originalSize;
    int pageCount = 0;
    int pageIndex = 0;
};

class DjvuReader {
public:
    static DjvuRenderResult renderPage(const QString &path, int page,
                                       const DjvuDecodeLimits &limits);
    static DjvuRenderResult renderPage(const QString &path, int page,
                                       const DjvuDecodeLimits &limits,
                                       const DecodeContext &context);
};
