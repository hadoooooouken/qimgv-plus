#include "styledcombobox.h"
#include "settings.h"

StyledComboBox::StyledComboBox(QWidget *parent) : QComboBox(parent), hiResPixmap(false)
{
    dpr = this->devicePixelRatioF();
    connect(settings, &Settings::settingsChanged, [this]() {
        refreshIconColor();
    });
}

void StyledComboBox::setIconPath(QString path) {
    iconResourcePath = path; // remembered so we can reload on DPI change

    if(dpr >= (1.0 + 0.001)) {
        path.replace(".", "@2x.");
        hiResPixmap = true;
        downArrow.load(path);
        if(dpr >= (2.0 - 0.001))
            pixmapDrawScale = dpr;
        else
            pixmapDrawScale = 2.0;
        downArrow.setDevicePixelRatio(pixmapDrawScale);
    } else {
        hiResPixmap = false;
        downArrow.load(path);
        pixmapDrawScale = dpr;
    }
    refreshIconColor();
}

int StyledComboBox::iconAreaWidth() const {
    if (downArrow.isNull())
        return kIconRightMargin;

    qreal logicalWidth = hiResPixmap ? downArrow.width() / pixmapDrawScale
                                      : downArrow.width();
    return kIconRightMargin + qRound(logicalWidth);
}

QColor StyledComboBox::iconColor() const {
    return settings->colorScheme().icons;
}

void StyledComboBox::refreshIconColor() {
    if(downArrow.isNull())
        return;
    ImageLib::recolor(downArrow, iconColor());
    update();
}

void StyledComboBox::paintEvent(QPaintEvent *e) {
    QComboBox::paintEvent(e);
    QPainter p(this);
    QPointF pos(0,0);

    if(hiResPixmap) {
        pos = QPointF(width() - kIconRightMargin - downArrow.width() / pixmapDrawScale,
                      height() / 2 - downArrow.height() / (2 * pixmapDrawScale));
    } else {
        pos = QPointF(width() - downArrow.width() - kIconRightMargin,
                      (height() - downArrow.height()) / 2);
    }
    p.drawPixmap(pos, downArrow);
}

void StyledComboBox::keyPressEvent(QKeyEvent *event) {
    event->ignore();
}

bool StyledComboBox::event(QEvent *e) {
    if (e->type() == QEvent::DevicePixelRatioChange) {
        qreal newDpr = devicePixelRatioF();
        if (!qFuzzyCompare(newDpr, dpr)) {
            dpr = newDpr;
            if (!iconResourcePath.isEmpty())
                setIconPath(iconResourcePath); // re-pick @1x/@2x asset, recompute pixmapDrawScale
        }
    }
    return QComboBox::event(e);
}
