#include "startuptiming.h"

#include <QDebug>

#include <windows.h>

// Info messages are disabled unless enabled by the logging rules.
Q_LOGGING_CATEGORY(lcStartup, "qimgv.startup", QtWarningMsg)

namespace {
// FILETIME counts 100-nanosecond intervals.
constexpr double kFileTimeTicksPerMs = 10'000.0;

quint64 fileTimeTicks(const FILETIME &time) {
    return (static_cast<quint64>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}
} // namespace

void logStartupMilestone(QStringView milestone) {
    if (!lcStartup().isInfoEnabled())
        return;

    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
        qCWarning(lcStartup) << "Cannot read the process creation time; error"
                             << GetLastError();
        return;
    }
    FILETIME now{};
    GetSystemTimePreciseAsFileTime(&now);
    const double elapsedMs =
        static_cast<double>(fileTimeTicks(now) - fileTimeTicks(creation)) /
        kFileTimeTicksPerMs;
    qCInfo(lcStartup).noquote() << milestone << "at" << elapsedMs << "ms";
}
