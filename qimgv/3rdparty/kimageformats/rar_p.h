/*
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIMG_RAR_P_H
#define KIMG_RAR_P_H

#include <QImageIOPlugin>
#include <memory>

class RarHandlerPrivate;

class RarHandler : public QImageIOHandler
{
public:
    RarHandler();
    ~RarHandler() override;

    bool canRead() const override;
    bool read(QImage *image) override;

    int imageCount() const override;
    int currentImageNumber() const override;
    bool jumpToImage(int imageNumber) override;
    bool jumpToNextImage() override;

    bool supportsOption(ImageOption option) const override;
    QVariant option(ImageOption option) const override;
    void setOption(ImageOption option, const QVariant &value) override;

private:
    const std::unique_ptr<RarHandlerPrivate> d;
};

class RarPlugin : public QImageIOPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface" FILE "rar.json")

public:
    Capabilities capabilities(QIODevice *device, const QByteArray &format) const override;
    QImageIOHandler *create(QIODevice *device, const QByteArray &format = QByteArray()) const override;
};

#endif // KIMG_RAR_P_H
