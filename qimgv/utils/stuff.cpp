#include "stuff.h"

int clamp(int x, int lower, int upper) {
    return qMin(upper, qMax(x, lower));
}

// 0 - mac, 1 - linux, 2 - windows, 3 - other
int probeOS() {
    return 2;
}

StdString toStdString(QString str) {
    return str.toStdWString();
}

QString fromStdString(StdString str) {
    return QString::fromStdWString(str);
}
