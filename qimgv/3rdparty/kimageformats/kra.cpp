/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2013 Boudewijn Rempt <boud@valdyas.org>

    SPDX-License-Identifier: LGPL-2.0-or-later

    This code is based on Thacher Ulrich PSD loading code released
    on public domain. See: http://tulrich.com/geekstuff/
*/

#include "kra.h"
#include "kraora_p.h"

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
    return KraOraInternal::readMergedImage(device(), image);
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
