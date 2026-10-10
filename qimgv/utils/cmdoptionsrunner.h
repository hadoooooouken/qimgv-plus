#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QDebug>
#include <QString>
#include "core.h"

class CmdOptionsRunner : public QObject {
    Q_OBJECT
public slots:
    void generateThumbs(QString dirPath, int size);
    void showBuildOptions();
};
