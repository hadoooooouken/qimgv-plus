#include <QGuiApplication>

#include "testsuites.h"

// Runs every suite, so one failing suite does not hide the others; the exit
// code is non-zero when any suite failed. QMovie (AnimationPlayer) needs a
// QGuiApplication; CTest runs the suites on the offscreen platform.
int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  int status = 0;
  status |= runQimgvTests(argc, argv);
  status |= runViewTransformTests(argc, argv);
  status |= runTileGridTests(argc, argv);
  status |= runImageFilterTests(argc, argv);
  status |= runColorTransformTests(argc, argv);
  status |= runAnimationPlayerTests(argc, argv);
  status |= runViewportInteractionTests(argc, argv);
  status |= runImageViewportControllerTests(argc, argv);
  status |= runWindowStateTests(argc, argv);
  status |= runSingleInstanceTests(argc, argv);
  status |= runWindowTitleTests(argc, argv);
  status |= runQuickShellTests(argc, argv);
  status |= runDecodedPixelsTests(argc, argv);
  status |= runUpscaleDecisionTests(argc, argv);
  status |= runUiMetricsTests(argc, argv);
  status |= runOverlayTests(argc, argv);
  status |= runThumbnailStripTests(argc, argv);
  status |= runContextMenuTests(argc, argv);
  status |= runCropTests(argc, argv);
  status |= runSvgRasterizerTests(argc, argv);
  return status;
}
