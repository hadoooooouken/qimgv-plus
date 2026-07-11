#include "formatgroups.h"

const QVector<FormatGroup> &allFormatGroups() {
    static const QVector<FormatGroup> groups = {
        { "JPG",        {"jpg", "jpeg"} },
        { "PNG",        {"png", "apng"} },
        { "WEBP",       {"webp"} },
        { "BMP",        {"bmp"} },
        { "GIF",        {"gif"} },
        { "JXL",        {"jxl"} },
        { "AVIF",       {"avif"} },
        { "HEIF/HEIC",  {"heif", "heic"} },
        { "JPEG 2000",  {"jp2", "j2k", "jpf", "jpx", "jpc", "jph"} },
        { "JXR",        {"jxr", "hdp"} },
        { "RAW",        {
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
        { "QOI",        {"qoi"} },
        { "KRA/ORA",    {"kra", "ora"} },
        { "PDF",        {"pdf"} },
        { "SVG",        {"svg"} },
        { "PSD",        {"psd", "psb"} },
        { "AI",         {"ai"} },
        { "EXR",        {"exr"} },
        { "HDR",        {"hdr"} },
        { "DDS/TGA",    {"dds", "tga"} },
        { "ICO",        {"ico"} },
    };
    return groups;
}
