#ifndef SCALERREQUEST_H
#define SCALERREQUEST_H

#include <QPixmap>
#include <QMetaType>
#include "sourcecontainers/image.h"
#include "settings_types.h"

class ScalerRequest {
public:
    ScalerRequest() : image(nullptr), size(QSize(0,0)), filter(QI_FILTER_BILINEAR) { }
    ScalerRequest(std::shared_ptr<Image> _image, QSize _size, QString _path, ScalingFilter _filter) : image(_image), size(_size), path(_path), filter(_filter) {}

    ScalerRequest(const ScalerRequest &other) = default;
    ScalerRequest(ScalerRequest &&other) noexcept = default;
    ScalerRequest &operator=(const ScalerRequest &other) = default;
    ScalerRequest &operator=(ScalerRequest &&other) noexcept = default;
    ~ScalerRequest() = default;

    std::shared_ptr<Image> image;
    QSize size;
    QString path;
    ScalingFilter filter;

    bool operator==(const ScalerRequest &another) const {
        if(another.image == image && another.size == size && another.filter == filter)
            return true;
        return false;
    }
};

Q_DECLARE_METATYPE(ScalerRequest)

#endif // SCALERREQUEST_H
