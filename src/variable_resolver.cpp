#include "variable_resolver.h"

#include <QDebug>
#include <QRegularExpression>

namespace {

// Matches {{date}}, {{time}}, {{datetime}} and their {{name:pattern}} forms.
// The name must be followed by ':' or the closing braces, so {{datetime}} is
// never truncated to {{date}}.
const QRegularExpression &VariablePattern() {
  static const QRegularExpression pattern(
      QStringLiteral("\\{\\{(date|time|datetime)(?::([^}]*))?\\}\\}"));
  return pattern;
}

bool ContainsOutsideQuotes(const QString &format, const QString &letters) {
  bool quoted = false;
  for (const QChar character : format) {
    if (character == QLatin1Char('\'')) {
      quoted = !quoted;
      continue;
    }
    if (!quoted && letters.contains(character)) {
      return true;
    }
  }
  return false;
}

void SetReason(QString *reason, const QString &text) {
  if (reason != nullptr) {
    *reason = text;
  }
}

QString DefaultFormatFor(const QString &name,
                         const VariableSettings &settings) {
  if (name == QLatin1String("time")) {
    return settings.timeFormat;
  }
  if (name == QLatin1String("datetime")) {
    return settings.dateTimeFormat;
  }
  return settings.dateFormat;
}

QString EffectiveFormat(const QString &name, const QString &inlineFormat,
                        const VariableSettings &settings) {
  const QString fallback = DefaultFormatFor(name, settings);
  if (inlineFormat.isEmpty()) {
    return fallback;
  }

  QString reason;
  const FormatCheck check = CheckDateTimeFormat(inlineFormat, &reason);
  if (check == FormatCheck::Invalid) {
    qWarning().noquote() << "Invalid {{" << name << ":" << inlineFormat
                         << "}} format (" << reason << "); using" << fallback;
    return fallback;
  }
  if (check == FormatCheck::Suspect) {
    qWarning().noquote() << "Suspicious {{" << name << ":" << inlineFormat
                         << "}} format (" << reason << ")";
  }
  return inlineFormat;
}

} // namespace

FormatCheck CheckDateTimeFormat(const QString &format, QString *reason) {
  if (format.trimmed().isEmpty()) {
    SetReason(reason, QStringLiteral("the format is empty"));
    return FormatCheck::Invalid;
  }

  if (format.contains(QLatin1Char('%'))) {
    SetReason(reason, QStringLiteral("strftime placeholders such as %Y are not "
                                     "supported; use Qt patterns like "
                                     "yyyy-MM-dd"));
    return FormatCheck::Invalid;
  }

  if (!ContainsOutsideQuotes(format, QStringLiteral("dMyHhmsz"))) {
    SetReason(reason, QStringLiteral("the format has no date or time fields"));
    return FormatCheck::Invalid;
  }

  const bool hasDateField =
      ContainsOutsideQuotes(format, QStringLiteral("yMd"));
  const bool hasTimeField =
      ContainsOutsideQuotes(format, QStringLiteral("Hhsz"));
  if (hasDateField && !hasTimeField &&
      ContainsOutsideQuotes(format, QStringLiteral("m"))) {
    SetReason(reason,
              QStringLiteral("lowercase 'm' means minutes; use 'MM' for the "
                             "month"));
    return FormatCheck::Suspect;
  }

  return FormatCheck::Ok;
}

QString ResolveVariables(const QString &payload) {
  return ResolveVariables(payload, QDateTime::currentDateTime());
}

QString ResolveVariables(const QString &payload, const QDateTime &now) {
  return ResolveVariables(payload, now, VariableSettings{});
}

QString ResolveVariables(const QString &payload, const QDateTime &now,
                         const VariableSettings &settings) {
  QString resolved;
  resolved.reserve(payload.size());

  qsizetype copiedUntil = 0;

  QRegularExpressionMatchIterator matches =
      VariablePattern().globalMatch(payload);
  while (matches.hasNext()) {
    const QRegularExpressionMatch match = matches.next();
    resolved += payload.mid(copiedUntil, match.capturedStart() - copiedUntil);

    const QString name = match.captured(1);
    const QString pattern = EffectiveFormat(name, match.captured(2), settings);
    resolved += settings.locale.toString(now, pattern);

    copiedUntil = match.capturedEnd();
  }

  resolved += payload.mid(copiedUntil);
  return resolved;
}
