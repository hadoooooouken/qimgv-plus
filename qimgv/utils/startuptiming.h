#pragma once

#include <QLoggingCategory>
#include <QStringView>

// Startup milestones, logged with the time since the process was created.
// Off by default; enable with QT_LOGGING_RULES="qimgv.startup.info=true".
Q_DECLARE_LOGGING_CATEGORY(lcStartup)

// Logs milestone and the milliseconds elapsed since process creation. Thread
// safe (may be called from the render thread).
void logStartupMilestone(QStringView milestone);
