#include "recent_items_store.h"
#include "test_harness.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

fs::path TempPath(const std::string &name) {
  return fs::temp_directory_path() / name;
}

RecentItemKey Key(const QString &type, const QString &name) {
  return RecentItemKey{type, name};
}

} // namespace

TEST(RecentItemsStoreTest, MissingCacheReturnsEmptyList) {
  const fs::path path = TempPath("surfpanel_recent_missing.toml");
  fs::remove(path);

  RecentItemsStore store(path);

  ASSERT_EQ(std::size_t(0), store.load().size());
}

TEST(RecentItemsStoreTest, LoadsTomlRecordsInOrder) {
  const fs::path path = TempPath("surfpanel_recent_order.toml");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << R"([[recent]]
type = "snippet"
name = "date"

[[recent]]
type = "url"
name = "Docs"
)";
  out.close();

  RecentItemsStore store(path);
  const auto items = store.load();

  ASSERT_EQ(std::size_t(2), items.size());
  ASSERT_EQ(QString("snippet"), items[0].type);
  ASSERT_EQ(QString("date"), items[0].name);
  ASSERT_EQ(QString("url"), items[1].type);
  ASSERT_EQ(QString("Docs"), items[1].name);

  fs::remove(path);
}

TEST(RecentItemsStoreTest, ExistingItemMovesToFront) {
  const fs::path path = TempPath("surfpanel_recent_move.toml");
  RecentItemsStore store(path);
  ASSERT_TRUE(store.save({Key("url", "Docs"), Key("snippet", "date")}));

  ASSERT_TRUE(store.recordUse(Key("snippet", "date"), 6));
  const auto items = store.load();

  ASSERT_EQ(std::size_t(2), items.size());
  ASSERT_EQ(QString("snippet"), items[0].type);
  ASSERT_EQ(QString("date"), items[0].name);
  ASSERT_EQ(QString("url"), items[1].type);
  ASSERT_EQ(QString("Docs"), items[1].name);

  fs::remove(path);
}

TEST(RecentItemsStoreTest, AddingBeyondCapacityEvictsLeastRecent) {
  const fs::path path = TempPath("surfpanel_recent_capacity.toml");
  RecentItemsStore store(path);
  ASSERT_TRUE(store.save({Key("url", "One"), Key("url", "Two")}));

  ASSERT_TRUE(store.recordUse(Key("url", "Three"), 2));
  const auto items = store.load();

  ASSERT_EQ(std::size_t(2), items.size());
  ASSERT_EQ(QString("Three"), items[0].name);
  ASSERT_EQ(QString("One"), items[1].name);

  fs::remove(path);
}

TEST(RecentItemsStoreTest, MalformedCacheIsIgnored) {
  const fs::path path = TempPath("surfpanel_recent_malformed.toml");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << "[[recent]\n";
  out.close();

  RecentItemsStore store(path);

  ASSERT_EQ(std::size_t(0), store.load().size());

  fs::remove(path);
}

int main() { return RUN_ALL_TESTS(); }
