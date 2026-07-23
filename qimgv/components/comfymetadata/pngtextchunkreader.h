#pragma once

#include <QString>
#include <QByteArray>
#include <QMap>
#include <expected>

// Reads PNG tEXt / zTXt chunks without decoding the image.
// ComfyUI (via PIL PngInfo) stores JSON in chunks with the keys "prompt" and
// "workflow" right after IHDR, before IDAT — so reading is normally very fast.
class PngTextChunkReader
{
public:
    // Returns a map: keyword -> text (already decoded if the chunk was zTXt),
    // or an error for invalid input, I/O failure, or a metadata budget violation.
    // stopAtIDAT: stop parsing as soon as the first IDAT chunk is reached
    // (comfy's text chunks always come before IDAT, so this is a safe optimization).
    static std::expected<QMap<QString, QByteArray>, QString>
        readTextChunks(const QString &pngPath, bool stopAtIDAT = true);

private:
    static std::expected<QByteArray, QString>
        zlibInflate(const QByteArray &compressed, qsizetype maxOutputBytes);
};
