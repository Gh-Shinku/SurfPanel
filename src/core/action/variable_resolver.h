#ifndef SURFPANEL_VARIABLE_RESOLVER_H
#define SURFPANEL_VARIABLE_RESOLVER_H

#include <QDateTime>
#include <QLocale>
#include <QString>

// User-configurable datetime variable formats. Patterns use Qt date/time
// syntax ("yyyy-MM-dd HH:mm"); weekday and month names follow `locale` while
// numeric fields stay locale independent. The defaults reproduce the formats
// that were previously hardcoded.
struct VariableSettings {
  QString dateFormat = QStringLiteral("yyyy/MM/dd");
  QString timeFormat = QStringLiteral("HH:mm:ss");
  QString dateTimeFormat = QStringLiteral("yyyy/MM/dd HH:mm:ss");
  QLocale locale = QLocale::system();
};

enum class FormatCheck {
  Ok,
  // Usable, but likely not what the user intended.
  Suspect,
  // Not usable; callers keep their default format.
  Invalid,
};

// Validates a user supplied datetime pattern. `reason` receives a lowercase,
// standalone explanation when the result is not `Ok`.
FormatCheck CheckDateTimeFormat(const QString &format, QString *reason);

QString ResolveVariables(const QString &payload);
QString ResolveVariables(const QString &payload, const QDateTime &now);
QString ResolveVariables(const QString &payload, const QDateTime &now,
                         const VariableSettings &settings);

#endif // SURFPANEL_VARIABLE_RESOLVER_H
