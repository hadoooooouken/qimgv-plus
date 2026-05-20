#include "wallpapersetter.h"

WallpaperSetter::WallpaperSetter() {

}

void WallpaperSetter::setWallpaper(QString path) {
    bool ok = SystemParametersInfo(SPI_SETDESKWALLPAPER,
                                   0,
                                   (PVOID) path.utf16(),
                                   SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE);
    qDebug() << "wallpaper changed:" << (ok ? "true" : "false");
}
