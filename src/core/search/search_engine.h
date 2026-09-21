#ifndef SURFPANEL_SEARCH_ENGINE_H
#define SURFPANEL_SEARCH_ENGINE_H

#include "core/search/item.h"
#include <QString>
#include <cstddef>
#include <vector>

struct SearchQueryInfo {
  QString query;
  QString itemType;
  bool prefixMode = false;
};

class SearchEngine {
public:
  void setItems(const std::vector<StringItem> &items);
  void setSearchPrefixes(const std::vector<SearchPrefixRule> &prefixes);
  SearchQueryInfo parseQuery(QString query) const;
  std::vector<const StringItem *> search(QString query,
                                         std::size_t k = 5) const;

private:
  struct IndexedItem {
    StringItem item;
    QString normalizedName;
    std::vector<QString> normalizedKeywords;
  };

  int calculateScore(const IndexedItem &item, const QString &fullQuery) const;

  std::vector<IndexedItem> items_;
  std::vector<SearchPrefixRule> prefixes_;
};

#endif // SURFPANEL_SEARCH_ENGINE_H
