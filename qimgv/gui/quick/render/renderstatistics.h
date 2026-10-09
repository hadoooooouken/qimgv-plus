#pragma once

#include <QtGlobal>
#include <atomic>

// Counters of an ImageRenderer's GPU work, written on the render thread and
// read on any thread (tests, diagnostics). Shared like RenderErrorChannel:
// created by ImageRenderItem, copied by the renderer in synchronize().
struct RenderStatistics {
  // Image textures created (displayed textures and conversion sources).
  std::atomic<quint64> imageTexturesCreated{0};
  // Images uploaded into new or reused textures.
  std::atomic<quint64> imageUploads{0};
};

// Values of RenderStatistics at one point in time.
struct RenderStatisticsSnapshot {
  quint64 imageTexturesCreated = 0;
  quint64 imageUploads = 0;
};
