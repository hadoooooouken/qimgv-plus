#include "savefilefilters.h"

#include <QFileInfo>
#include <array>

namespace {
using namespace Qt::StringLiterals;

struct WritableFormat {
    QByteArrayView writerFormat;
    QLatin1StringView filter;
};

// The filters of MW::getSaveFileName(), kept verbatim (including the JPEG
// filter's "*jpe" pattern) so that existing translations and habits match.
constexpr std::array kWritableFormats{
    WritableFormat{"jpg", "JPEG (*.jpg *.jpeg *jpe *jfif)"_L1},
    WritableFormat{"png", "PNG (*.png)"_L1},
    WritableFormat{"webp", "WebP (*.webp)"_L1},
    WritableFormat{"jxl", "JPEG-XL (*.jxl)"_L1},
    WritableFormat{"avif", "AVIF (*.avif *.avifs)"_L1},
    WritableFormat{"qoi", "QOI (*.qoi)"_L1},
    WritableFormat{"bmp", "BMP (*.bmp)"_L1},
    WritableFormat{"tif", "TIFF (*.tif *.tiff)"_L1},
};

constexpr qsizetype kFallbackFormat = 0;
} // namespace

int SaveFileFilters::selectedIndex() const {
    const qsizetype index = filters.indexOf(selected);
    return index < 0 ? 0 : static_cast<int>(index);
}

SaveFileFilters saveFileFiltersFor(const QList<QByteArray> &writerFormats,
                                   const QString &suggestedPath) {
    SaveFileFilters result;
    for (const WritableFormat &format : kWritableFormats) {
        if (writerFormats.contains(format.writerFormat))
            result.filters.append(format.filter);
    }

    result.selected = kWritableFormats[kFallbackFormat].filter;
    const QString suffix = QFileInfo(suggestedPath).suffix().toLower();
    for (const QString &filter : std::as_const(result.filters)) {
        if (filter.contains(suffix)) {
            result.selected = filter;
            break;
        }
    }
    return result;
}
