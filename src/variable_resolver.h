#ifndef SURFPANEL_VARIABLE_RESOLVER_H
#define SURFPANEL_VARIABLE_RESOLVER_H

#include <QDateTime>
#include <QString>

QString ResolveVariables(const QString &payload);
QString ResolveVariables(const QString &payload, const QDateTime &now);

#endif // SURFPANEL_VARIABLE_RESOLVER_H
