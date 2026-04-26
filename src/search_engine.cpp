#include "search_engine.h"
#include <algorithm>

namespace {
struct SearchMatch {
  const StringItem *item;
  int score;
};
} // namespace

void SearchEngine::setItems(const std::vector<StringItem> &items) {
  items_ = items;
}

std::vector<const StringItem *> SearchEngine::search(QString query,
                                                     std::size_t k) const {
  query = query.toLower().trimmed();
  if (query.isEmpty() || k == 0) {
    return {};
  }

  std::vector<SearchMatch> matches;
  for (const auto &item : items_) {
    const int score = calculateScore(item, query);
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