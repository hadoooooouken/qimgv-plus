#pragma once

// Entry points of the Qt Test suites linked into qimgv_tests. Each runs one
// test class with QTest::qExec() and returns its exit code.
int runQimgvTests(int argc, char **argv);
int runViewTransformTests(int argc, char **argv);
int runTileGridTests(int argc, char **argv);
int runImageFilterTests(int argc, char **argv);
int runColorTransformTests(int argc, char **argv);
int runAnimationPlayerTests(int argc, char **argv);
int runViewportInteractionTests(int argc, char **argv);
int runImageViewportControllerTests(int argc, char **argv);
int runWindowStateTests(int argc, char **argv);
int runSingleInstanceTests(int argc, char **argv);
int runWindowTitleTests(int argc, char **argv);
int runQuickShellTests(int argc, char **argv);
int runDecodedPixelsTests(int argc, char **argv);
int runUpscaleDecisionTests(int argc, char **argv);
int runUiMetricsTests(int argc, char **argv);
int runOverlayTests(int argc, char **argv);
int runThumbnailStripTests(int argc, char **argv);
int runFolderViewTests(int argc, char **argv);
int runContextMenuTests(int argc, char **argv);
int runCropTests(int argc, char **argv);
int runSvgRasterizerTests(int argc, char **argv);
int runDialogTests(int argc, char **argv);
