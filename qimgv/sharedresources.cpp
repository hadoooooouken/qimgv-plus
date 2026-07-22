#include "sharedresources.h"
#include "utils/iconfontmanager.h"

namespace {
constexpr int kLoadingIconSizePx = 72;
}

SharedResources *shrRes = nullptr;

SharedResources::SharedResources()
{
}

SharedResources::~SharedResources() {
    if (shrRes == this) {
        shrRes = nullptr;
    }
}

QPixmap SharedResources::getPixmap(ShrIcon icon, qreal dpr) {
    const FluentIcon fluentIcon = icon == ShrIcon::SHR_ICON_ERROR
                                      ? FluentIcon::ClockDismiss24
                                      : FluentIcon::Clock24;
    return IconFontManager::pixmap(fluentIcon, kLoadingIconSizePx, Qt::white,
                                   dpr);
}

SharedResources *SharedResources::getInstance() {
    if(!shrRes) {
        shrRes = new SharedResources();
    }
    return shrRes;
}
