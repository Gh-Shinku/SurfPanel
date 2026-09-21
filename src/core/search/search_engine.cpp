#include "core/search/search_engine.h"
#include <algorithm>
#include <optional>

namespace {
struct SearchMatch {
  const StringItem *item;
  int score;
};

std::optional<int> FindFirstSpace(const QString &query) {
  for (int i = 0; i < query.size(); ++i) {
    if (query.at(i).isSpace()) {
      return i;
    }
  }
  return std::nullopt;
}

QString TrimLeft(QString query) {
  while (!query.isEmpty() && query.front().isSpace()) {
    query.remove(0, 1);
  }
  return query;
}
} // namespace

void SearchEngine::setItems(const std::vector<StringItem> &items) {
  items_.clear();
  items_.reserve(items.size());
  for (const auto &item : items) {
    IndexedItem indexed;
    indexed.item = item;
    indexed.normalizedName = item.name.toLower();
    indexed.normalizedKeywords.reserve(item.keywords.size());
    for (const auto &keyword : item.keywords) {
      indexed.normalizedKeywords.push_back(keyword.toLower());
    }
    items_.push_back(std::move(indexed));
  }
}

void SearchEngine::setSearchPrefixes(
    const std::vector<SearchPrefixRule> &prefixes) {
  prefixes_ = prefixes;
}

SearchQueryInfo SearchEngine::parseQuery(QString query) const {
  query = TrimLeft(query);

  SearchQueryInfo parsed;
  parsed.query = query.trimmed();
  if (const auto spaceIndex = FindFirstSpace(query); spaceIndex.has_value()) {
    const QString prefix = query.left(*spaceIndex);
    for (const auto &rule : prefixes_) {
      if (rule.prefix.compare(prefix, Qt::CaseInsensitive) == 0) {
        parsed.prefixMode = true;
        parsed.itemType = rule.itemType.toLower();
        parsed.query = query.mid(*spaceIndex + 1).trimmed();
        break;
      }
    }
  }

  parsed.query = parsed.query.toLower();
  return parsed;
}

std::vector<const StringItem *> SearchEngine::search(QString query,
                                                     std::size_t k) const {
  const SearchQueryInfo parsed = parseQuery(query);
  if ((!parsed.prefixMode && parsed.query.isEmpty()) || k == 0) {
    return {};
  }

  std::vector<SearchMatch> matches;
  for (const auto &indexed : items_) {
    const StringItem &item = indexed.item;
    if (parsed.prefixMode &&
        item.type.compare(parsed.itemType, Qt::CaseInsensitive) != 0) {
      continue;
    }

    const int score =
        parsed.query.isEmpty() ? 1 : calculateScore(indexed, parsed.query);
    if (score > 0) {
      matches.push_back({&indexed.item, score});
    }
  }

  const std::size_t resultSize = std::min(matches.size(), k);
  auto comparator = [](const SearchMatch &lhs, const SearchMatch &rhs) {
    if (lhs.score != rhs.score) {
      return lhs.score > rhs.score;
    }
    return lhs.item->name < rhs.item->name;
  };
  std::partial_sort(matches.begin(), matches.begin() + resultSize,
                    matches.end(), comparator);

  std::vector<const StringItem *> results;
  results.reserve(resultSize);
  for (std::size_t i = 0; i < resultSize; ++i) {
    results.push_back(matches[i].item);
  }
  return results;
}

int SearchEngine::calculateScore(const IndexedItem &item,
                                 const QString &fullQuery) const {
  int score = 0;

  if (item.normalizedName == fullQuery) {
    score += 100;
  }
  if (item.normalizedName.contains(fullQuery)) {
    score += 50;
  }

  for (const auto &keyword : item.normalizedKeywords) {
    if (keyword == fullQuery) {
      score += 40;
    } else if (keyword.contains(fullQuery)) {
      score += 20;
    }
  }

  return score;
}
