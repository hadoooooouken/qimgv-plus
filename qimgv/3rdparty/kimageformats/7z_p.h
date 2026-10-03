/*
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIMG_7Z_P_H
#define KIMG_7Z_P_H

#include <QImageIOPlugin>
#include <memory>

class SevenZipHandlerPrivate;

class SevenZipHandler : public QImageIOHandler
{
public:
    SevenZipHandler();
    ~SevenZipHandler() override;

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
    const std::unique_ptr<SevenZipHandlerPrivate> d;
};

class SevenZipPlugin : public QImageIOPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface" FILE "7z.json")

public:
    Capabilities capabilities(QIODevice *device, const QByteArray &format) const override;
    QImageIOHandler *create(QIODevice *device, const QByteArray &format = QByteArray()) const override;
};

#endif // KIMG_7Z_P_H
