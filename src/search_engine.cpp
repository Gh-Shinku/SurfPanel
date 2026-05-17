#include "search_engine.h"
#include <algorithm>
#include <optional>

namespace {
struct SearchMatch {
  const StringItem *item;
  int score;
};

struct ParsedQuery {
  QString query;
  std::optional<QString> itemType;
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
  items_ = items;
}

void SearchEngine::setSearchPrefixes(
    const std::vector<SearchPrefixRule> &prefixes) {
  prefixes_ = prefixes;
}

std::vector<const StringItem *> SearchEngine::search(QString query,
                                                     std::size_t k) const {
  query = TrimLeft(query);
  if (query.trimmed().isEmpty() || k == 0) {
    return {};
  }

  ParsedQuery parsed;
  parsed.query = query.trimmed();
  if (const auto spaceIndex = FindFirstSpace(query); spaceIndex.has_value()) {
    const QString prefix = query.left(*spaceIndex);
    for (const auto &rule : prefixes_) {
      if (rule.prefix == prefix) {
        parsed.itemType = rule.itemType.toLower();
        parsed.query = query.mid(*spaceIndex + 1).trimmed();
        break;
      }
    }
  }

  parsed.query = parsed.query.toLower();
  std::vector<SearchMatch> matches;
  for (const auto &item : items_) {
    if (parsed.itemType.has_value() &&
        item.type.compare(*parsed.itemType, Qt::CaseInsensitive) != 0) {
      continue;
    }

    const int score =
        parsed.query.isEmpty() ? 1 : calculateScore(item, parsed.query);
    if (score > 0) {
      matches.push_back({&item, score});
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

int SearchEngine::calculateScore(const StringItem &item,
                                 const QString &fullQuery) const {
  int score = 0;
  const QString nameLower = item.name.toLower();

  if (nameLower == fullQuery) {
    score += 100;
  }
  if (nameLower.contains(fullQuery)) {
    score += 50;
  }

  for (const auto &kw : item.keywords) {
    const QString kwLower = kw.toLower();
    if (kwLower == fullQuery) {
      score += 40;
    } else if (kwLower.contains(fullQuery)) {
      score += 20;
    }
  }

  return score;
}
