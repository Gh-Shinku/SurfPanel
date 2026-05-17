#include "variable_resolver.h"

QString ResolveVariables(const QString &payload) {
  return ResolveVariables(payload, QDateTime::currentDateTime());
}

QString ResolveVariables(const QString &payload, const QDateTime &now) {
  QString resolved = payload;
  resolved.replace("{{datetime}}", now.toString("yyyy/MM/dd HH:mm:ss"));
  resolved.replace("{{date}}", now.toString("yyyy/MM/dd"));
  resolved.replace("{{time}}", now.toString("HH:mm:ss"));
  return resolved;
}
