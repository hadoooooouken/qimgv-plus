#include "formatgroups.h"

QVector<FormatCategory> allFormatCategories() {
    return {
        { QCoreApplication::translate("FormatFilterComboBox", "Common"), {
            { "JPG",       {"jpg", "jpeg"} },
            { "PNG",       {"png", "apng"} },
            { "WEBP",      {"webp"} },
            { "GIF",       {"gif"} },
            { "BMP",       {"bmp"} },
            { "AVIF",      {"avif"} },
            { "HEIF",      {"heif"} },
            { "HEIC",      {"heic"} },
            { "JXL",       {"jxl"} },
            { "QOI",       {"qoi"} },
        } },
        { QCoreApplication::translate("FormatFilterComboBox", "Specialized"), {
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
            { "EXR",       {"exr"} },
            { "HDR",       {"hdr"} },
            { "DDS",       {"dds"} },
            { "TGA",       {"tga"} },
            { "JPEG 2000", {"jp2", "j2k", "jpf", "jpx", "jpc", "jph"} },
            { "JXR",       {"jxr", "hdp"} },
        } },
        { QCoreApplication::translate("FormatFilterComboBox", "Vector / Design"), {
            { "SVG",       {"svg"} },
            { "AI",        {"ai"} },
            { "PSD",       {"psd"} },
            { "PSB",       {"psb"} },
            { "KRA",       {"kra"} },
            { "ORA",       {"ora"} },
        } },
        { QCoreApplication::translate("FormatFilterComboBox", "Documents / Archives"), {
            { "PDF",       {"pdf"} },
            { "DjVu",      {"djvu", "djv"} },
            { "CBZ",       {"cbz"} },
            { "ZIP",       {"zip"} },
        } },
        { QCoreApplication::translate("FormatFilterComboBox", "Other"), {
            { "ICO",       {"ico"} },
            { "BLEND",     {"blend"} },
            { QCoreApplication::translate("FormatFilterComboBox", "Fonts"), {"ttf", "otf", "ttc"} },
        } },
    };
}
