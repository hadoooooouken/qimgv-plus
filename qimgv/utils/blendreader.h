#pragma once

#include <QImage>
#include <QString>

class BlendReader {
public:
    // Extract the embedded Blender TEST preview without loading scene data.
    // A valid .blend with no stored preview returns a null image and leaves
    // error empty.
    static QImage readPreview(const QString &path, QString *error = nullptr);
};
