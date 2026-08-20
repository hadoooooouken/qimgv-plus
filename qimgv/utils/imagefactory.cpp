#include "imagefactory.h"

std::shared_ptr<Image> ImageFactory::createImage(QString path,
                                                 DecodeContext context) {
    if (context.isCancellationRequested())
        return nullptr;

    std::unique_ptr<DocumentInfo> docInfo(new DocumentInfo(path));
    if (context.isCancellationRequested())
        return nullptr;

    std::shared_ptr<Image> img = nullptr;
    if(docInfo->type() == NONE) {
        qWarning() << "ImageFactory: cannot load " << docInfo->filePath();
    } else if(docInfo->type() == ANIMATED) {
        img.reset(new ImageAnimated(std::move(docInfo)));
    } else {
        img.reset(new ImageStatic(std::move(docInfo), std::move(context)));
    }
    return img;
}
