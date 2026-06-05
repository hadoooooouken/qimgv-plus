/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 hadooooouken <https://github.com/hadoooooouken/qimgv-plus>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "jph_p.h"
#include <QImage>
#include <QVariant>
#include <QSize>
#include <QColorSpace>
#include <QLoggingCategory>
#include <vector>
#include <algorithm>

#include <openjph/ojph_arch.h>
#include <openjph/ojph_file.h>
#include <openjph/ojph_codestream.h>
#include <openjph/ojph_params.h>
#include <openjph/ojph_mem.h>

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(LOG_JPHPLUGIN, "kf.imageformats.plugins.jph", QtDebugMsg)
#else
Q_LOGGING_CATEGORY(LOG_JPHPLUGIN, "kf.imageformats.plugins.jph", QtWarningMsg)
#endif

class JPHHandlerPrivate
{
public:
    JPHHandlerPrivate() : header_read(false), codestream_created(false) {}
    ~JPHHandlerPrivate() {
        if (codestream_created) {
            codestream.close();
        }
        mem_file.close();
    }

    QByteArray buffer;
    ojph::mem_infile mem_file;
    ojph::codestream codestream;
    bool header_read;
    bool codestream_created;

    bool readHeader(QIODevice *device)
    {
        if (header_read) {
            return true;
        }
        if (!device) {
            return false;
        }
        
        qint64 pos = device->pos();
        device->seek(0);
        buffer = device->readAll();
        device->seek(pos);
        
        if (buffer.isEmpty()) {
            return false;
        }

        mem_file.open(reinterpret_cast<const ojph::ui8*>(buffer.constData()), buffer.size());
        try {
            codestream.read_headers(&mem_file);
            header_read = true;
            return true;
        } catch (...) {
            mem_file.close();
            buffer.clear();
            return false;
        }
    }
};

JPHHandler::JPHHandler()
    : QImageIOHandler()
    , d(new JPHHandlerPrivate)
{
}

JPHHandler::~JPHHandler()
{
}

bool JPHHandler::canRead() const
{
    if (canRead(device())) {
        setFormat("jph");
        return true;
    }
    return false;
}

bool JPHHandler::canRead(QIODevice *device)
{
    if (!device) {
        qCWarning(LOG_JPHPLUGIN) << "JPHHandler::canRead() called with no device";
        return false;
    }

    QByteArray ba = device->peek(32);
    if (ba.size() < 12) {
        return false;
    }

    // Check for JP2/JPH box signature: 00 00 00 0C 6A 50 20 20 0D 0A 87 0A
    if (ba.left(12) == QByteArray::fromHex("0000000c6a5020200d0a870a")) {
        if (ba.size() >= 24) {
            auto brand = ba.mid(20, 4);
            if (brand == "jph " || brand == "jphb") {
                return true;
            }
        }
        return false; // Reject standard JP2 box files
    }
    // Check for raw codestream signature (SOC marker): FF 4F
    if (ba.startsWith(QByteArray::fromHex("ff4f"))) {
        return true;
    }

    return false;
}

bool JPHHandler::read(QImage *image)
{
    if (!d->readHeader(device())) {
        return false;
    }

    try {
        if (!d->codestream_created) {
            d->codestream.create();
            d->codestream_created = true;
        }

        ojph::param_siz siz = d->codestream.access_siz();
        int width = siz.get_recon_width(0);
        int height = siz.get_recon_height(0);
        int num_comps = siz.get_num_components();
        int bit_depth = siz.get_bit_depth(0);

        if (width <= 0 || height <= 0 || num_comps <= 0 || num_comps > 4) {
            return false;
        }

        QImage::Format fmt;
        if (num_comps == 1) {
            fmt = (bit_depth <= 8) ? QImage::Format_Grayscale8 : QImage::Format_Grayscale16;
        } else {
            fmt = (bit_depth <= 8) ? QImage::Format_RGBA8888 : QImage::Format_RGBA64;
        }

        QImage img(width, height, fmt);
        if (img.isNull()) {
            return false;
        }

        int max_val = (1 << bit_depth) - 1;
        int divisor = (max_val > 255) ? (max_val / 255) : 1;
        int multiplier = (max_val < 65535) ? (65535 / max_val) : 1;

        if (d->codestream.is_planar()) {
            std::vector<std::vector<ojph::si32>> comp_buffers(num_comps, std::vector<ojph::si32>(width * height));
            for (int c = 0; c < num_comps; ++c) {
                for (int y = 0; y < height; ++y) {
                    ojph::ui32 comp_num;
                    ojph::line_buf *line = d->codestream.pull(comp_num);
                    assert(comp_num == (ojph::ui32)c);
                    memcpy(&comp_buffers[c][y * width], line->i32, width * sizeof(ojph::si32));
                }
            }

            for (int y = 0; y < height; ++y) {
                uchar *scanline = img.scanLine(y);
                if (num_comps == 1) {
                    if (bit_depth <= 8) {
                        for (int x = 0; x < width; ++x) {
                            scanline[x] = (uchar)std::clamp(comp_buffers[0][y * width + x] / divisor, 0, 255);
                        }
                    } else {
                        ushort *ptr16 = reinterpret_cast<ushort*>(scanline);
                        for (int x = 0; x < width; ++x) {
                            ptr16[x] = (ushort)std::clamp(comp_buffers[0][y * width + x] * multiplier, 0, 65535);
                        }
                    }
                } else if (num_comps == 3) {
                    if (bit_depth <= 8) {
                        for (int x = 0; x < width; ++x) {
                            scanline[x*4 + 0] = (uchar)std::clamp(comp_buffers[0][y * width + x] / divisor, 0, 255);
                            scanline[x*4 + 1] = (uchar)std::clamp(comp_buffers[1][y * width + x] / divisor, 0, 255);
                            scanline[x*4 + 2] = (uchar)std::clamp(comp_buffers[2][y * width + x] / divisor, 0, 255);
                            scanline[x*4 + 3] = 255;
                        }
                    } else {
                        QRgba64 *ptr64 = reinterpret_cast<QRgba64*>(scanline);
                        for (int x = 0; x < width; ++x) {
                            ushort r = std::clamp(comp_buffers[0][y * width + x] * multiplier, 0, 65535);
                            ushort g = std::clamp(comp_buffers[1][y * width + x] * multiplier, 0, 65535);
                            ushort b = std::clamp(comp_buffers[2][y * width + x] * multiplier, 0, 65535);
                            ptr64[x] = QRgba64::fromRgba64(r, g, b, 65535);
                        }
                    }
                } else if (num_comps == 4) {
                    if (bit_depth <= 8) {
                        for (int x = 0; x < width; ++x) {
                            scanline[x*4 + 0] = (uchar)std::clamp(comp_buffers[0][y * width + x] / divisor, 0, 255);
                            scanline[x*4 + 1] = (uchar)std::clamp(comp_buffers[1][y * width + x] / divisor, 0, 255);
                            scanline[x*4 + 2] = (uchar)std::clamp(comp_buffers[2][y * width + x] / divisor, 0, 255);
                            scanline[x*4 + 3] = (uchar)std::clamp(comp_buffers[3][y * width + x] / divisor, 0, 255);
                        }
                    } else {
                        QRgba64 *ptr64 = reinterpret_cast<QRgba64*>(scanline);
                        for (int x = 0; x < width; ++x) {
                            ushort r = std::clamp(comp_buffers[0][y * width + x] * multiplier, 0, 65535);
                            ushort g = std::clamp(comp_buffers[1][y * width + x] * multiplier, 0, 65535);
                            ushort b = std::clamp(comp_buffers[2][y * width + x] * multiplier, 0, 65535);
                            ushort a = std::clamp(comp_buffers[3][y * width + x] * multiplier, 0, 65535);
                            ptr64[x] = QRgba64::fromRgba64(r, g, b, a);
                        }
                    }
                }
            }
        } else {
            for (int y = 0; y < height; ++y) {
                const ojph::line_buf *lines[4] = {nullptr};
                for (int c = 0; c < num_comps; ++c) {
                    ojph::ui32 comp_num;
                    lines[c] = d->codestream.pull(comp_num);
                    assert(comp_num == (ojph::ui32)c);
                }

                uchar *scanline = img.scanLine(y);
                if (num_comps == 1) {
                    const ojph::si32 *src = lines[0]->i32;
                    if (bit_depth <= 8) {
                        for (int x = 0; x < width; ++x) {
                            scanline[x] = (uchar)std::clamp(src[x] / divisor, 0, 255);
                        }
                    } else {
                        ushort *ptr16 = reinterpret_cast<ushort*>(scanline);
                        for (int x = 0; x < width; ++x) {
                            ptr16[x] = (ushort)std::clamp(src[x] * multiplier, 0, 65535);
                        }
                    }
                } else if (num_comps == 3) {
                    const ojph::si32 *src0 = lines[0]->i32;
                    const ojph::si32 *src1 = lines[1]->i32;
                    const ojph::si32 *src2 = lines[2]->i32;
                    if (bit_depth <= 8) {
                        for (int x = 0; x < width; ++x) {
                            scanline[x*4 + 0] = (uchar)std::clamp(src0[x] / divisor, 0, 255);
                            scanline[x*4 + 1] = (uchar)std::clamp(src1[x] / divisor, 0, 255);
                            scanline[x*4 + 2] = (uchar)std::clamp(src2[x] / divisor, 0, 255);
                            scanline[x*4 + 3] = 255;
                        }
                    } else {
                        QRgba64 *ptr64 = reinterpret_cast<QRgba64*>(scanline);
                        for (int x = 0; x < width; ++x) {
                            ushort r = std::clamp(src0[x] * multiplier, 0, 65535);
                            ushort g = std::clamp(src1[x] * multiplier, 0, 65535);
                            ushort b = std::clamp(src2[x] * multiplier, 0, 65535);
                            ptr64[x] = QRgba64::fromRgba64(r, g, b, 65535);
                        }
                    }
                } else if (num_comps == 4) {
                    const ojph::si32 *src0 = lines[0]->i32;
                    const ojph::si32 *src1 = lines[1]->i32;
                    const ojph::si32 *src2 = lines[2]->i32;
                    const ojph::si32 *src3 = lines[3]->i32;
                    if (bit_depth <= 8) {
                        for (int x = 0; x < width; ++x) {
                            scanline[x*4 + 0] = (uchar)std::clamp(src0[x] / divisor, 0, 255);
                            scanline[x*4 + 1] = (uchar)std::clamp(src1[x] / divisor, 0, 255);
                            scanline[x*4 + 2] = (uchar)std::clamp(src2[x] / divisor, 0, 255);
                            scanline[x*4 + 3] = (uchar)std::clamp(src3[x] / divisor, 0, 255);
                        }
                    } else {
                        QRgba64 *ptr64 = reinterpret_cast<QRgba64*>(scanline);
                        for (int x = 0; x < width; ++x) {
                            ushort r = std::clamp(src0[x] * multiplier, 0, 65535);
                            ushort g = std::clamp(src1[x] * multiplier, 0, 65535);
                            ushort b = std::clamp(src2[x] * multiplier, 0, 65535);
                            ushort a = std::clamp(src3[x] * multiplier, 0, 65535);
                            ptr64[x] = QRgba64::fromRgba64(r, g, b, a);
                        }
                    }
                }
            }
        }

        // Apply default sRGB color space
        img.setColorSpace(QColorSpace(QColorSpace::SRgb));
        *image = img;
        return true;
    } catch (...) {
        return false;
    }
}

bool JPHHandler::write(const QImage &image)
{
    Q_UNUSED(image);
    // Write support not required for the viewer
    return false;
}

bool JPHHandler::supportsOption(QImageIOHandler::ImageOption option) const
{
    return option == QImageIOHandler::Size;
}

void JPHHandler::setOption(ImageOption option, const QVariant &value)
{
    Q_UNUSED(option);
    Q_UNUSED(value);
}

QVariant JPHHandler::option(QImageIOHandler::ImageOption option) const
{
    if (option == QImageIOHandler::Size) {
        if (d->readHeader(device())) {
            ojph::param_siz siz = d->codestream.access_siz();
            return QSize(siz.get_recon_width(0), siz.get_recon_height(0));
        }
    }
    return QVariant();
}

QImageIOPlugin::Capabilities JPHPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "jph" || format == "JPH") {
        return Capabilities(CanRead);
    }
    if (!format.isEmpty()) {
        return {};
    }
    if (!device->isOpen()) {
        return {};
    }

    Capabilities cap;
    if (device->isReadable() && JPHHandler::canRead(device)) {
        cap |= CanRead;
    }
    return cap;
}

QImageIOHandler *JPHPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new JPHHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

#include "moc_jph_p.cpp"
