#include "stuff.h"

int clamp(int x, int lower, int upper) {
    return qMin(upper, qMax(x, lower));
}

StdString toStdString(QString str) {
    return str.toStdWString();
}

QString fromStdString(StdString str) {
    return QString::fromStdWString(str);
}
