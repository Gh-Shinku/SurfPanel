#ifndef SURFPANEL_RECENT_ITEMS_STORE_H
#define SURFPANEL_RECENT_ITEMS_STORE_H

#include "item.h"
#include <cstddef>
#include <filesystem>
#include <vector>

struct RecentItemKey {
  QString type;
  QString name;
};

bool SameRecentItemKey(const RecentItemKey &lhs, const RecentItemKey &rhs);
RecentItemKey RecentKeyForItem(const StringItem &item);
std::filesystem::path DefaultRecentItemsPath();

class RecentItemsStore {
public:
  explicit RecentItemsStore(std::filesystem::path path =
                                DefaultRecentItemsPath());

  std::vector<RecentItemKey> load() const;
  bool save(const std::vector<RecentItemKey> &items) const;
  bool recordUse(const RecentItemKey &item, std::size_t capacity) const;

private:
  std::filesystem::path path_;
};

#endif // SURFPANEL_RECENT_ITEMS_STORE_H
