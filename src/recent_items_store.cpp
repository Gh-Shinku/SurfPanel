#include "recent_items_store.h"

#include "toml.hpp"
#include <QCoreApplication>
#include <QString>
#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace {

QString NormalizeKeyPart(const QString &value) { return value.toLower(); }

} // namespace

bool SameRecentItemKey(const RecentItemKey &lhs, const RecentItemKey &rhs) {
  return NormalizeKeyPart(lhs.type) == NormalizeKeyPart(rhs.type) &&
         NormalizeKeyPart(lhs.name) == NormalizeKeyPart(rhs.name);
}

RecentItemKey RecentKeyForItem(const StringItem &item) {
  return RecentItemKey{item.type, item.name};
}

std::filesystem::path DefaultRecentItemsPath() {
  const QString appDir = QCoreApplication::applicationDirPath();
#ifdef _WIN32
  return fs::path(appDir.toStdWString()) / "recent_items.toml";
#else
  return fs::path(appDir.toStdString()) / "recent_items.toml";
#endif
}

RecentItemsStore::RecentItemsStore(std::filesystem::path path)
    : path_(std::move(path)) {}

std::vector<RecentItemKey> RecentItemsStore::load() const {
  if (!fs::exists(path_)) {
    return {};
  }

  try {
    const auto root = toml::parse(path_, toml::spec::v(1, 1, 0));
    const auto recent =
        toml::find_or(root, "recent", toml::value{toml::array{}});
    if (!recent.is_array()) {
      return {};
    }

    std::vector<RecentItemKey> items;
    for (const auto &entry : recent.as_array()) {
      if (!entry.is_table()) {
        continue;
      }

      const std::string type = toml::find_or(entry, "type", std::string{});
      const std::string name = toml::find_or(entry, "name", std::string{});
      if (type.empty() || name.empty()) {
        continue;
      }

      items.push_back(RecentItemKey{QString::fromStdString(type),
                                    QString::fromStdString(name)});
    }
    return items;
  } catch (...) {
    return {};
  }
}

bool RecentItemsStore::save(const std::vector<RecentItemKey> &items) const {
  std::error_code ec;
  if (!path_.parent_path().empty()) {
    fs::create_directories(path_.parent_path(), ec);
  }

  toml::array recent;
  for (const auto &item : items) {
    toml::table entry;
    entry["type"] = item.type.toStdString();
    entry["name"] = item.name.toStdString();
    recent.push_back(entry);
  }

  toml::table root;
  root["recent"] = recent;

  std::ofstream out(path_, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }

  out << toml::format(toml::value(root));
  return true;
}

bool RecentItemsStore::recordUse(const RecentItemKey &item,
                                 std::size_t capacity) const {
  if (capacity == 0 || item.type.isEmpty() || item.name.isEmpty()) {
    return save({});
  }

  std::vector<RecentItemKey> items = load();
  items.erase(std::remove_if(items.begin(), items.end(),
                             [&](const RecentItemKey &existing) {
                               return SameRecentItemKey(existing, item);
                             }),
              items.end());
  items.insert(items.begin(), item);
  if (items.size() > capacity) {
    items.resize(capacity);
  }

  return save(items);
}
