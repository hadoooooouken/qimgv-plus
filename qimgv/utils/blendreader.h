#pragma once

#include <QImage>
#include <QString>

#include "utils/decodecontext.h"

class BlendReader {
public:
    // Extract the embedded Blender TEST preview without loading scene data.
    // A valid .blend with no stored preview returns a null image and leaves
    // error empty.
    static QImage readPreview(const QString &path, QString *error = nullptr);
    static QImage readPreview(const QString &path,
                              const DecodeContext &context,
                              QString *error = nullptr);
};
