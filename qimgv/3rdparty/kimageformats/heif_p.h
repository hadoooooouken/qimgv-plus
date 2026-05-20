/*
    HEIF/HEIC image support via FFmpeg for QImage.
    Read-only decoder plugin.

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIMG_HEIF_P_H
#define KIMG_HEIF_P_H

#include <QImage>
#include <QImageIOPlugin>
#include <QSize>

class HEIFHandler : public QImageIOHandler
{
public:
    HEIFHandler();

    bool canRead() const override;
    bool read(QImage *image) override;
    bool write(const QImage &) override { return false; }

    QVariant option(ImageOption option) const override;
    void setOption(ImageOption option, const QVariant &value) override;
    bool supportsOption(ImageOption option) const override;

    static bool isSupportedBMFFType(const QByteArray &header);

private:
    bool ensureParsed() const;
    bool decodeWithFFmpeg();

    enum ParseState {
        NotParsed = 0,
        ParseSuccess = 1,
        ParseError = -1,
    };

    ParseState m_parseState;
    QImage m_current_image;
    int m_quality;
};

class HEIFPlugin : public QImageIOPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface" FILE "heif.json")

public:
    Capabilities capabilities(QIODevice *device, const QByteArray &format) const override;
    QImageIOHandler *create(QIODevice *device, const QByteArray &format = QByteArray()) const override;
};

#endif // KIMG_HEIF_P_H
