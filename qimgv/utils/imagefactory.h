#pragma once

#include "utils/imagelib.h"
#include "sourcecontainers/documentinfo.h"
#include "sourcecontainers/image.h"
#include "sourcecontainers/imageanimated.h"
#include "sourcecontainers/imagestatic.h"
#include "utils/decodecontext.h"

class ImageFactory {
public:
    static std::shared_ptr<Image> createImage(QString path,
                                              DecodeContext context = {});
};
