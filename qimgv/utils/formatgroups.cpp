#include "formatgroups.h"

const QVector<FormatCategory> &allFormatCategories() {
    static const QVector<FormatCategory> categories = {
        { "COMMON", {
            { "JPG", {"jpg", "jpeg"} },
            { "PNG", {"png", "apng"} },
            { "WEBP", {"webp"} },
            { "GIF", {"gif"} },
            { "BMP", {"bmp"} },
            { "AVIF", {"avif"} },
            { "HEIF", {"heif"} },
            { "HEIC", {"heic"} },
            { "JXL", {"jxl"} },
            { "QOI", {"qoi"} },
        } },
        { "SPECIALIZED", {
            { "RAW", {
                "3fr",
                "arw",
                "crw", "cr2", "cr3",
                "dcr", "dng",
                "erf",
                "fff",
                "iiq",
                "k25", "kdc",
                "mdc", "mef", "mos", "mrw",
                "nef", "nrw",
                "orf",
                "pef",
                "raf", "raw", "rwl", "rw2",
                "sr2", "srf", "srw", "sti",
                "x3f"
            } },
            { "EXR", {"exr"} },
            { "HDR", {"hdr"} },
            { "DDS", {"dds"} },
            { "TGA", {"tga"} },
            { "JPEG 2000", {"jp2", "j2k", "jpf", "jpx", "jpc", "jph"} },
            { "JXR", {"jxr", "hdp"} },
        } },
        { "VECTOR / DESIGN", {
            { "SVG", {"svg"} },
            { "AI", {"ai"} },
            { "PSD", {"psd"} },
            { "PSB", {"psb"} },
            { "KRA", {"kra"} },
            { "ORA", {"ora"} },
        } },
        { "DOCUMENTS / ARCHIVES", {
            { "PDF", {"pdf"} },
            { "DjVu", {"djvu", "djv"} },
            { "CBZ", {"cbz"} },
            { "ZIP", {"zip"} },
        } },
        { "OTHER", {
            { "ICO", {"ico"} },
            { "BLEND", {"blend"} },
            { "Fonts", {"ttf", "otf", "ttc"} },
        } },
    };
    return categories;
}
