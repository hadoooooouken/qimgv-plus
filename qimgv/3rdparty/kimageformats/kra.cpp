/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2013 Boudewijn Rempt <boud@valdyas.org>

    SPDX-License-Identifier: LGPL-2.0-or-later

    This code is based on Thacher Ulrich PSD loading code released
    on public domain. See: http://tulrich.com/geekstuff/
*/

#include "kra.h"

extern "C" {
#include "miniz.h"
}

#include <QFile>
#include <QIODevice>
#include <QImage>

static constexpr char s_magic[] = "application/x-krita";
static constexpr int s_magic_size = sizeof(s_magic) - 1; // -1 to remove the last \0

KraHandler::KraHandler()
{
}

bool KraHandler::canRead() const
{
    if (canRead(device())) {
        setFormat("kra");
        return true;
    }
    return false;
}

bool KraHandler::read(QImage *image)
{
    device()->seek(0);
    QByteArray data = device()->readAll();
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_reader_init_mem(&zip_archive, data.constData(), data.size(), 0)) {
        return false;
    }

    int file_index = mz_zip_reader_locate_file(&zip_archive, "mergedimage.png", nullptr, 0);
    if (file_index < 0) {
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    size_t uncompressed_size = 0;
    void *pData = mz_zip_reader_extract_to_heap(&zip_archive, file_index, &uncompressed_size, 0);
    if (!pData) {
        mz_zip_reader_end(&zip_archive);
        return false;
    }

    bool success = image->loadFromData(static_cast<const uchar*>(pData), static_cast<int>(uncompressed_size), "PNG");

    mz_free(pData);
    mz_zip_reader_end(&zip_archive);

    return success;
}

bool KraHandler::canRead(QIODevice *device)
{
    if (!device) {
        qWarning("KraHandler::canRead() called with no device");
        return false;
    }
    if (device->isSequential()) {
        return false;
    }

    char buff[128];
    qint64 bytesRead = device->peek(buff, sizeof(buff));
    if (bytesRead < 4) {
        return false;
    }
    if (memcmp(buff, "PK\x03\x04", 4) != 0) {
        return false;
    }

    QByteArray peeked(buff, static_cast<int>(bytesRead));
    return peeked.contains(s_magic);
}

QImageIOPlugin::Capabilities KraPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "kra" || format == "KRA") {
        return Capabilities(CanRead);
    }
    if (!format.isEmpty()) {
        return {};
    }
    if (!device->isOpen()) {
        return {};
    }

    Capabilities cap;
    if (device->isReadable() && KraHandler::canRead(device)) {
        cap |= CanRead;
    }
    return cap;
}

QImageIOHandler *KraPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new KraHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}

#include "moc_kra.cpp"
