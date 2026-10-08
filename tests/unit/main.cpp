#include <QCoreApplication>

#include "testsuites.h"

// Runs every suite, so one failing suite does not hide the others; the exit
// code is non-zero when any suite failed.
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  int status = 0;
  status |= runQimgvTests(argc, argv);
  status |= runViewTransformTests(argc, argv);
  return status;
}
