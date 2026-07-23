#pragma once

#include <QtGlobal>

namespace ComfyMetadataLimits {
// These are hard security ceilings for metadata supplied by untrusted image
// files. They are intentionally independent of the image dimensions.
inline constexpr quint32 kMaxPngChunkBytes = 64u * 1024u * 1024u;
inline constexpr qsizetype kMaxMetadataBytes = 8 * 1024 * 1024;
inline constexpr qsizetype kMaxTextChunkCount = 64;
inline constexpr qsizetype kMaxJsonNestingDepth = 128;
inline constexpr qsizetype kMaxJsonValueCount = 65'536;
inline constexpr qsizetype kMaxGraphNodeCount = 4'096;
inline constexpr qsizetype kMaxGraphEdgeCount = 32'768;
inline constexpr qsizetype kMaxGraphDepth = 128;
inline constexpr qsizetype kMaxGraphTraversalSteps = 65'536;
inline constexpr qsizetype kMaxResolvedTextCharacters = 1024 * 1024;
}
