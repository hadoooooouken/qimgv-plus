/*
    HEIF/HEIC image support via FFmpeg for QImage.
    Read-only decoder plugin.

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "heif_p.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <QColorSpace>
#include <QIODevice>
#include <QLoggingCategory>
#include <cstring>
#include <vector>

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(LOG_HEIFPLUGIN, "kf.imageformats.plugins.heif", QtDebugMsg)
#else
Q_LOGGING_CATEGORY(LOG_HEIFPLUGIN, "kf.imageformats.plugins.heif", QtWarningMsg)
#endif

// ─── Custom AVIO for reading from memory ───────────────────────────

struct MemBuffer {
    const uint8_t *data;
    int64_t size;
    int64_t pos;
};

static int memRead(void *opaque, uint8_t *buf, int buf_size)
{
    auto *mb = static_cast<MemBuffer *>(opaque);
    int64_t remaining = mb->size - mb->pos;
    if (remaining <= 0)
        return AVERROR_EOF;
    int toRead = static_cast<int>(qMin(static_cast<int64_t>(buf_size), remaining));
    memcpy(buf, mb->data + mb->pos, toRead);
    mb->pos += toRead;
    return toRead;
}

static int64_t memSeek(void *opaque, int64_t offset, int whence)
{
    auto *mb = static_cast<MemBuffer *>(opaque);
    switch (whence) {
    case SEEK_SET: mb->pos = offset; break;
    case SEEK_CUR: mb->pos += offset; break;
    case SEEK_END: mb->pos = mb->size + offset; break;
    case AVSEEK_SIZE: return mb->size;
    default: return AVERROR(EINVAL);
    }
    if (mb->pos < 0) mb->pos = 0;
    if (mb->pos > mb->size) mb->pos = mb->size;
    return mb->pos;
}

// ─── HEIFHandler ───────────────────────────────────────────────────

HEIFHandler::HEIFHandler()
    : m_parseState(NotParsed)
    , m_quality(100)
{
}

bool HEIFHandler::isSupportedBMFFType(const QByteArray &header)
{
    if (header.size() < 12)
        return false;
    const char *buf = header.constData();
    if (memcmp(buf + 4, "ftyp", 4) != 0)
        return false;
    // HEIC/HEIF brand detection
    static const char *brands[] = { "heic", "heis", "heix", "mif1", "mif2", "msf1" };
    for (auto brand : brands) {
        if (memcmp(buf + 8, brand, 4) == 0) {
            // exclude AVIF (mif1 with avif compatible brand)
            if (memcmp(buf + 8, "mif1", 4) == 0 && header.size() >= 28) {
                for (int off = 16; off <= 24; off += 4) {
                    if (memcmp(buf + off, "avif", 4) == 0)
                        return false;
                }
            }
            return true;
        }
    }
    return false;
}

bool HEIFHandler::canRead() const
{
    if (m_parseState == NotParsed) {
        QIODevice *dev = device();
        if (dev) {
            const QByteArray header = dev->peek(28);
            if (isSupportedBMFFType(header)) {
                const_cast<HEIFHandler *>(this)->setFormat("heif");
                return true;
            }
        }
        return false;
    }
    return m_parseState == ParseSuccess;
}

bool HEIFHandler::read(QImage *outImage)
{
    if (!ensureParsed())
        return false;
    *outImage = m_current_image;
    return true;
}

QVariant HEIFHandler::option(ImageOption option) const
{
    if (option == Quality)
        return m_quality;
    if (option == Size && ensureParsed())
        return m_current_image.size();
    return QVariant();
}

void HEIFHandler::setOption(ImageOption option, const QVariant &value)
{
    if (option == Quality) {
        m_quality = qBound(0, value.toInt(), 100);
    }
}

bool HEIFHandler::supportsOption(ImageOption option) const
{
    return option == Quality || option == Size;
}

bool HEIFHandler::ensureParsed() const
{
    if (m_parseState == ParseSuccess) return true;
    if (m_parseState == ParseError) return false;
    return const_cast<HEIFHandler *>(this)->decodeWithFFmpeg();
}

bool HEIFHandler::decodeWithFFmpeg()
{
    // Read all data from device
    QByteArray fileData = device()->readAll();
    if (fileData.isEmpty()) {
        qCWarning(LOG_HEIFPLUGIN) << "Empty input data";
        m_parseState = ParseError;
        return false;
    }

    // Set up custom AVIO context for in-memory reading
    MemBuffer mb;
    mb.data = reinterpret_cast<const uint8_t *>(fileData.constData());
    mb.size = fileData.size();
    mb.pos = 0;

    const int avioBufSize = 32768;
    auto *avioBuf = static_cast<uint8_t *>(av_malloc(avioBufSize));
    if (!avioBuf) {
        m_parseState = ParseError;
        return false;
    }

    AVIOContext *avioCtx = avio_alloc_context(avioBuf, avioBufSize, 0, &mb, memRead, nullptr, memSeek);
    if (!avioCtx) {
        av_free(avioBuf);
        m_parseState = ParseError;
        return false;
    }

    AVFormatContext *fmtCtx = avformat_alloc_context();
    fmtCtx->pb = avioCtx;

    int ret = avformat_open_input(&fmtCtx, nullptr, nullptr, nullptr);
    if (ret < 0) {
        qCWarning(LOG_HEIFPLUGIN) << "avformat_open_input failed:" << ret;
        avio_context_free(&avioCtx);
        m_parseState = ParseError;
        return false;
    }

    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0) {
        qCWarning(LOG_HEIFPLUGIN) << "avformat_find_stream_info failed:" << ret;
        avformat_close_input(&fmtCtx);
        avio_context_free(&avioCtx);
        m_parseState = ParseError;
        return false;
    }

    // Look for Tile Grid stream group
    AVStreamGroup *tileGridGroup = nullptr;
    AVStreamGroupTileGrid *tileGrid = nullptr;
    for (unsigned i = 0; i < fmtCtx->nb_stream_groups; i++) {
        AVStreamGroup *group = fmtCtx->stream_groups[i];
        if (group->type == AV_STREAM_GROUP_PARAMS_TILE_GRID) {
            tileGridGroup = group;
            tileGrid = group->params.tile_grid;
            break;
        }
    }

    if (tileGridGroup && tileGrid && tileGrid->nb_tiles > 0) {
        // 1. Allocate codec contexts for all tiles
        std::vector<AVCodecContext *> codecContexts(tileGrid->nb_tiles, nullptr);
        bool hasCodecs = false;

        for (unsigned i = 0; i < tileGrid->nb_tiles; i++) {
            AVStream *stream = tileGridGroup->streams[i];
            const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
            if (!codec) continue;

            AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
            if (!codecCtx) continue;

            if (avcodec_parameters_to_context(codecCtx, stream->codecpar) < 0 ||
                avcodec_open2(codecCtx, codec, nullptr) < 0) {
                avcodec_free_context(&codecCtx);
                continue;
            }
            codecContexts[i] = codecCtx;
            hasCodecs = true;
        }

        if (!hasCodecs) {
            qCWarning(LOG_HEIFPLUGIN) << "Failed to open any decoders for tile grid";
            for (auto *ctx : codecContexts) {
                if (ctx) avcodec_free_context(&ctx);
            }
            avformat_close_input(&fmtCtx);
            avio_context_free(&avioCtx);
            m_parseState = ParseError;
            return false;
        }

        // 2. Allocate canvas QImage (coded_width x coded_height)
        const int canvasW = tileGrid->coded_width;
        const int canvasH = tileGrid->coded_height;

        QImage canvasImage(canvasW, canvasH, QImage::Format_ARGB32);
        if (canvasImage.isNull()) {
            qCWarning(LOG_HEIFPLUGIN) << "Failed to allocate canvas QImage" << canvasW << "x" << canvasH;
            for (auto *ctx : codecContexts) {
                if (ctx) avcodec_free_context(&ctx);
            }
            avformat_close_input(&fmtCtx);
            avio_context_free(&avioCtx);
            m_parseState = ParseError;
            return false;
        }

        canvasImage.fill(Qt::transparent);

        // 3. Read and send all packets to the respective codecs
        AVPacket *pkt = av_packet_alloc();
        while (av_read_frame(fmtCtx, pkt) >= 0) {
            int tileIdx = -1;
            for (unsigned i = 0; i < tileGrid->nb_tiles; i++) {
                if (tileGridGroup->streams[i]->index == pkt->stream_index) {
                    tileIdx = static_cast<int>(i);
                    break;
                }
            }

            if (tileIdx != -1 && codecContexts[tileIdx]) {
                avcodec_send_packet(codecContexts[tileIdx], pkt);
            }
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);

        // 4. Decode all frames and copy them onto the canvas
        bool success = true;
        for (unsigned i = 0; i < tileGrid->nb_tiles; i++) {
            AVCodecContext *codecCtx = codecContexts[i];
            if (!codecCtx) {
                success = false;
                continue;
            }

            // Flush decoder
            avcodec_send_packet(codecCtx, nullptr);

            AVFrame *frame = av_frame_alloc();
            int decodeRet = avcodec_receive_frame(codecCtx, frame);
            if (decodeRet != 0) {
                qCWarning(LOG_HEIFPLUGIN) << "Failed to decode frame for tile" << i << "error:" << decodeRet;
                av_frame_free(&frame);
                success = false;
                continue;
            }

            int posX = tileGrid->offsets[i].horizontal;
            int posY = tileGrid->offsets[i].vertical;

            if (posX >= 0 && posY >= 0 &&
                posX + frame->width <= canvasW &&
                posY + frame->height <= canvasH) {

                SwsContext *swsCtx = sws_getContext(
                    frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
                    frame->width, frame->height, AV_PIX_FMT_BGRA,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);

                if (swsCtx) {
                    uint8_t *dstData[1] = { canvasImage.bits() + posY * canvasImage.bytesPerLine() + posX * 4 };
                    int dstLinesize[1] = { static_cast<int>(canvasImage.bytesPerLine()) };
                    sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height, dstData, dstLinesize);
                    sws_freeContext(swsCtx);
                } else {
                    success = false;
                }
            } else {
                qCWarning(LOG_HEIFPLUGIN) << "Tile" << i << "is out of canvas bounds:" << posX << posY;
                success = false;
            }

            av_frame_free(&frame);
        }

        // Free decoders
        for (auto *ctx : codecContexts) {
            if (ctx) avcodec_free_context(&ctx);
        }

        // 5. Crop canvas to final presentation size
        int cropX = tileGrid->horizontal_offset;
        int cropY = tileGrid->vertical_offset;
        int cropW = tileGrid->width;
        int cropH = tileGrid->height;

        if (cropW > 0 && cropH > 0 && cropX >= 0 && cropY >= 0 &&
            cropX + cropW <= canvasW && cropY + cropH <= canvasH) {
            m_current_image = canvasImage.copy(cropX, cropY, cropW, cropH);
        } else {
            m_current_image = canvasImage;
        }

        m_current_image.setColorSpace(QColorSpace(QColorSpace::SRgb));

        // Cleanup format context
        avformat_close_input(&fmtCtx);
        avio_context_free(&avioCtx);

        m_parseState = ParseSuccess;
        return true;
    }

    // Fallback to standard single stream decoding
    int videoIdx = -1;
    for (unsigned i = 0; i < fmtCtx->nb_streams; i++) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIdx = static_cast<int>(i);
            break;
        }
    }
    if (videoIdx < 0) {
        qCWarning(LOG_HEIFPLUGIN) << "No video stream found";
        avformat_close_input(&fmtCtx);
        avio_context_free(&avioCtx);
        m_parseState = ParseError;
        return false;
    }

    AVCodecParameters *codecpar = fmtCtx->streams[videoIdx]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        qCWarning(LOG_HEIFPLUGIN) << "No decoder found for codec id" << codecpar->codec_id;
        avformat_close_input(&fmtCtx);
        avio_context_free(&avioCtx);
        m_parseState = ParseError;
        return false;
    }

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecpar);
    ret = avcodec_open2(codecCtx, codec, nullptr);
    if (ret < 0) {
        qCWarning(LOG_HEIFPLUGIN) << "avcodec_open2 failed:" << ret;
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        avio_context_free(&avioCtx);
        m_parseState = ParseError;
        return false;
    }

    // Decode the first frame
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    bool decoded = false;

    while (av_read_frame(fmtCtx, pkt) >= 0) {
        if (pkt->stream_index != videoIdx) {
            av_packet_unref(pkt);
            continue;
        }
        ret = avcodec_send_packet(codecCtx, pkt);
        av_packet_unref(pkt);
        if (ret < 0) break;

        ret = avcodec_receive_frame(codecCtx, frame);
        if (ret == 0) {
            decoded = true;
            break;
        }
    }

    // Flush decoder if we haven't got a frame yet
    if (!decoded) {
        avcodec_send_packet(codecCtx, nullptr);
        if (avcodec_receive_frame(codecCtx, frame) == 0)
            decoded = true;
    }

    if (!decoded || frame->width <= 0 || frame->height <= 0) {
        qCWarning(LOG_HEIFPLUGIN) << "Failed to decode HEIF frame";
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        avio_context_free(&avioCtx);
        m_parseState = ParseError;
        return false;
    }

    // Convert to BGRA → QImage::Format_ARGB32
    const int w = frame->width;
    const int h = frame->height;

    m_current_image = QImage(w, h, QImage::Format_ARGB32);
    if (m_current_image.isNull()) {
        qCWarning(LOG_HEIFPLUGIN) << "Failed to allocate QImage" << w << "x" << h;
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        avio_context_free(&avioCtx);
        m_parseState = ParseError;
        return false;
    }

    SwsContext *swsCtx = sws_getContext(
        w, h, static_cast<AVPixelFormat>(frame->format),
        w, h, AV_PIX_FMT_BGRA,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!swsCtx) {
        qCWarning(LOG_HEIFPLUGIN) << "sws_getContext failed";
        m_current_image = QImage();
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        avio_context_free(&avioCtx);
        m_parseState = ParseError;
        return false;
    }

    uint8_t *dstData[1] = { m_current_image.bits() };
    int dstLinesize[1] = { static_cast<int>(m_current_image.bytesPerLine()) };
    sws_scale(swsCtx, frame->data, frame->linesize, 0, h, dstData, dstLinesize);
    sws_freeContext(swsCtx);

    // Try to set sRGB color space
    m_current_image.setColorSpace(QColorSpace(QColorSpace::SRgb));

    // Cleanup
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);
    avio_context_free(&avioCtx);

    m_parseState = ParseSuccess;
    return true;
}

// ─── Plugin ────────────────────────────────────────────────────────

QImageIOPlugin::Capabilities HEIFPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "heif" || format == "heic")
        return CanRead;

    if (!format.isEmpty())
        return {};
    if (!device || !device->isOpen())
        return {};

    Capabilities cap;
    if (device->isReadable()) {
        const QByteArray header = device->peek(28);
        if (HEIFHandler::isSupportedBMFFType(header))
            cap |= CanRead;
    }
    return cap;
}

QImageIOHandler *HEIFPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new HEIFHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

#include "moc_heif_p.cpp"
