/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 hadooooouken <https://github.com/hadoooooouken/qimgv-plus>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef KIMG_JPH_P_H
#define KIMG_JPH_P_H

#include <QImageIOPlugin>
#include <QScopedPointer>

class JPHHandlerPrivate;
class JPHHandler : public QImageIOHandler
{
public:
    JPHHandler();
    ~JPHHandler();

    bool canRead() const override;
    bool read(QImage *image) override;
    bool write(const QImage &image) override;

    bool supportsOption(QImageIOHandler::ImageOption option) const override;
    void setOption(ImageOption option, const QVariant &value) override;
    QVariant option(QImageIOHandler::ImageOption option) const override;

    static bool canRead(QIODevice *device);

private:
    const QScopedPointer<JPHHandlerPrivate> d;
};

class JPHPlugin : public QImageIOPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface" FILE "jph.json")

public:
    Capabilities capabilities(QIODevice *device, const QByteArray &format) const override;
    QImageIOHandler *create(QIODevice *device, const QByteArray &format = QByteArray()) const override;
};

#endif // KIMG_JPH_P_H
