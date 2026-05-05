#include "app_logging.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QtGlobal>

namespace {

QString LogDirPath() {
  const QString base = QCoreApplication::applicationDirPath();
  return base + "/log";
}

QString LogFilePath() {
  const QString appName = QCoreApplication::applicationName().isEmpty()
                              ? QStringLiteral("SurfPanel")
                              : QCoreApplication::applicationName();
  return LogDirPath() + "/" + appName + ".log";
}

const char *LogLevelLabel(QtMsgType type) {
  switch (type) {
  case QtDebugMsg:
    return "DEBUG";
  case QtInfoMsg:
    return "INFO";
  case QtWarningMsg:
    return "WARN";
  case QtCriticalMsg:
    return "CRITICAL";
  case QtFatalMsg:
    return "FATAL";
  }
  return "UNKNOWN";
}

void AppMessageHandler(QtMsgType type, const QMessageLogContext &context,
                       const QString &message) {
  static QMutex logMutex;
  QMutexLocker locker(&logMutex);

  QFile file(LogFilePath());
  if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    return;
  }

  const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
  QString contextInfo;
  if (context.file != nullptr && context.line > 0) {
    contextInfo = QString(" (%1:%2)")
                      .arg(QString::fromUtf8(context.file))
                      .arg(context.line);
  }

  QTextStream out(&file);
  out << timestamp << " [" << LogLevelLabel(type) << "] " << message
      << contextInfo << "\n";
  out.flush();
}

} // namespace

void InstallFileLogger() {
  QDir().mkpath(LogDirPath());
  qInstallMessageHandler(AppMessageHandler);
}
