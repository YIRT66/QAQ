#pragma once

#include <QString>

class QApplication;

namespace RuntimeDiagnostics {

struct StartupState {
    QString logFile;
    QString logDirectory;
    bool previousCrashDetected = false;
    QString previousCrashSummary;
};

bool isWatchdogInvocation(int argc, char *argv[]);
int runWatchdog(int argc, char *argv[]);

StartupState initialize(QApplication &app);
void markCleanExit();
void startWatchdogIfEnabled();
void showPreviousCrashWarning(const StartupState &state);

QString currentLogFile();
QString logDirectory();

} // namespace RuntimeDiagnostics
