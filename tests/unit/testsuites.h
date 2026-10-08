#pragma once

// Entry points of the Qt Test suites linked into qimgv_tests. Each runs one
// test class with QTest::qExec() and returns its exit code.
int runQimgvTests(int argc, char **argv);
int runViewTransformTests(int argc, char **argv);
int runTileGridTests(int argc, char **argv);
