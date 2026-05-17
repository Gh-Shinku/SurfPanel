#ifndef SURFPANEL_ITEM_H
#define SURFPANEL_ITEM_H

#include <QString>
#include <variant>
#include <vector>

struct UrlPayload {
  QString url;
};

struct SnippetPayload {
  QString snippet;
};

using Payload = std::variant<UrlPayload, SnippetPayload>;

struct StringItem {
  QString name;
  QString type;
  std::vector<QString> keywords;
  Payload payload;
};

struct SearchPrefixRule {
  QString itemType;
  QString prefix;
};

#endif // SURFPANEL_ITEM_H
