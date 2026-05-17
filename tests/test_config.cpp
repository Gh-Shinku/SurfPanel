#include "config.h"
#include "item.h"
#include "test_harness.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <variant>

namespace fs = std::filesystem;

namespace {

fs::path WriteTomlFile(const std::string &content, const std::string &name) {
  const fs::path path = fs::temp_directory_path() / name;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << content;
  return path;
}

fs::path WriteTomlFile(const fs::path &dir, const std::string &name,
                       const std::string &content) {
  fs::create_directories(dir);
  const fs::path path = dir / name;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << content;
  return path;
}

const StringItem *FindItemByName(const std::vector<StringItem> &items,
                                 const QString &name) {
  for (const auto &item : items) {
    if (item.name == name) {
      return &item;
    }
  }
  return nullptr;
}

const SearchPrefixRule *
FindPrefixByType(const std::vector<SearchPrefixRule> &prefixes,
                 const QString &type) {
  for (const auto &prefix : prefixes) {
    if (prefix.itemType.compare(type, Qt::CaseInsensitive) == 0) {
      return &prefix;
    }
  }
  return nullptr;
}

std::string CaptureRuntimeError(const std::function<void()> &fn) {
  try {
    fn();
  } catch (const std::runtime_error &err) {
    return err.what();
  }
  throw std::runtime_error(
      "Expected std::runtime_error but no exception thrown");
}

} // namespace

TEST(ConfigTest, ParsesValidToml) {
  const auto path = WriteTomlFile(
      R"([[items]]
name = "Open GitHub"
type = "url"
keywords = ["git", "code"]
[items.payload]
url = "https://github.com"

[[items]]
name = "linux"
type = "snippet"
keywords = ["linux", "command"]
[items.payload]
snippet = "ls -lah"
)",
      "surfpanel_config_valid.toml");

  const auto items = loadStringItems(path);

  ASSERT_EQ(std::size_t(2), items.size());

  ASSERT_EQ(QString("Open GitHub"), items[0].name);
  ASSERT_EQ(QString("url"), items[0].type);
  ASSERT_EQ(std::size_t(2), items[0].keywords.size());
  ASSERT_EQ(QString("git"), items[0].keywords[0]);
  ASSERT_TRUE(std::holds_alternative<UrlPayload>(items[0].payload));
  ASSERT_EQ(QString("https://github.com"),
            std::get<UrlPayload>(items[0].payload).url);

  ASSERT_EQ(QString("linux"), items[1].name);
  ASSERT_EQ(QString("snippet"), items[1].type);
  ASSERT_TRUE(std::holds_alternative<SnippetPayload>(items[1].payload));
  ASSERT_EQ(QString("ls -lah"),
            std::get<SnippetPayload>(items[1].payload).snippet);

  fs::remove(path);
}

TEST(ConfigTest, ThrowsOnMissingFile) {
  const fs::path missingPath =
      fs::temp_directory_path() / "surfpanel_config_missing.toml";
  fs::remove(missingPath);

  const auto message = CaptureRuntimeError(
      [&]() { static_cast<void>(loadStringItems(missingPath)); });
  ASSERT_CONTAINS(message, "Configuration file not found");
}

TEST(ConfigTest, ThrowsOnSyntaxError) {
  const auto path = WriteTomlFile(
      R"([[items]]
name = "broken"
type = "url"
keywords = ["syntax"
[items.payload]
url = "https://example.com"
)",
      "surfpanel_config_syntax_error.toml");

  const auto message =
      CaptureRuntimeError([&]() { static_cast<void>(loadStringItems(path)); });
  ASSERT_CONTAINS(message, "TOML parse error");

  fs::remove(path);
}

TEST(ConfigTest, ThrowsOnUnknownType) {
  const auto path = WriteTomlFile(
      R"([[items]]
name = "invalid"
type = "command"
keywords = ["bad"]
[items.payload]
url = "https://example.com"
)",
      "surfpanel_config_unknown_type.toml");

  const auto message =
      CaptureRuntimeError([&]() { static_cast<void>(loadStringItems(path)); });
  ASSERT_CONTAINS(message, "Unknown item type");

  fs::remove(path);
}

TEST(ConfigTest, LegacyLoadConfigWrapperStillWorks) {
  const auto path = WriteTomlFile(
      R"([[items]]
name = "Open GitHub"
type = "url"
keywords = ["git"]
[items.payload]
url = "https://github.com"
)",
      "surfpanel_config_wrapper.toml");

  load_config(path);
  fs::remove(path);
}

TEST(ConfigTest, MainConfigLoadsItemsToml) {
  const fs::path root = fs::temp_directory_path() / "surfpanel_main_root";
  fs::remove_all(root);

  WriteTomlFile(root, "items.toml",
                R"([[items]]
name = "Open GitHub"
type = "url"
keywords = ["git"]
[items.payload]
url = "https://github.com"
)");

  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(std::size_t(1), result.items.size());
  ASSERT_EQ(QString("Open GitHub"), result.items[0].name);

  fs::remove_all(root);
}

TEST(ConfigTest, MainConfigImportsPackagesAndAllowsLocalOverrides) {
  const fs::path root = fs::temp_directory_path() / "surfpanel_import_root";
  fs::remove_all(root);

  WriteTomlFile(root / "packages" / "demo", "items.toml",
                R"([[items]]
name = "Docs"
type = "url"
keywords = ["docs"]
[items.payload]
url = "https://example.com/docs"

[[items]]
name = "Package Snippet"
type = "snippet"
keywords = ["package"]
[items.payload]
snippet = "from package"
)");

  WriteTomlFile(root, "items.toml",
                R"(imports = ["packages/demo/items.toml"]

[[items]]
name = "Docs"
type = "url"
keywords = ["docs", "local"]
[items.payload]
url = "https://example.com/local-docs"
)");

  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(std::size_t(2), result.items.size());

  const auto *docs = FindItemByName(result.items, "Docs");
  ASSERT_NE(nullptr, docs);
  ASSERT_TRUE(std::holds_alternative<UrlPayload>(docs->payload));
  ASSERT_EQ(QString("https://example.com/local-docs"),
            std::get<UrlPayload>(docs->payload).url);

  const auto *snippet = FindItemByName(result.items, "Package Snippet");
  ASSERT_NE(nullptr, snippet);
  ASSERT_TRUE(std::holds_alternative<SnippetPayload>(snippet->payload));

  fs::remove_all(root);
}

TEST(ConfigTest, MainConfigCanDisableImportedItems) {
  const fs::path root =
      fs::temp_directory_path() / "surfpanel_import_disable_root";
  fs::remove_all(root);

  WriteTomlFile(root / "packages" / "demo", "items.toml",
                R"([[items]]
name = "Docs"
type = "url"
keywords = ["docs"]
[items.payload]
url = "https://example.com/docs"
)");

  WriteTomlFile(root, "items.toml",
                R"(imports = ["packages/demo/items.toml"]

[[items]]
name = "Docs"
type = "url"
disabled = true
)");

  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(std::size_t(0), result.items.size());

  const auto *docs = FindItemByName(result.items, "Docs");
  ASSERT_TRUE(docs == nullptr);

  fs::remove_all(root);
}

TEST(ConfigTest, ImportsCanLoadTomlDirectoriesInSortedOrder) {
  const fs::path root = fs::temp_directory_path() / "surfpanel_import_dir_root";
  fs::remove_all(root);

  WriteTomlFile(root / "packages" / "demo", "b.toml",
                R"([[items]]
name = "Second"
type = "url"
keywords = ["second"]
[items.payload]
url = "https://example.com/second"
)");
  WriteTomlFile(root / "packages" / "demo", "a.toml",
                R"([[items]]
name = "First"
type = "url"
keywords = ["first"]
[items.payload]
url = "https://example.com/first"
)");
  WriteTomlFile(root, "items.toml",
                R"(imports = ["packages/demo"]
)");

  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(std::size_t(2), result.items.size());
  ASSERT_EQ(QString("First"), result.items[0].name);
  ASSERT_EQ(QString("Second"), result.items[1].name);

  fs::remove_all(root);
}

TEST(ConfigTest, ParsesMainConfigSearchPrefixes) {
  const fs::path root =
      fs::temp_directory_path() / "surfpanel_prefix_main_root";
  fs::remove_all(root);

  WriteTomlFile(root, "items.toml",
                R"([[items]]
name = "Open GitHub"
type = "url"
keywords = ["git"]
[items.payload]
url = "https://github.com"

[search.prefixes]
snippet = "clip"
url = "web"
)");

  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);

  const auto *snippetPrefix =
      FindPrefixByType(result.searchPrefixes, "snippet");
  const auto *urlPrefix = FindPrefixByType(result.searchPrefixes, "url");
  ASSERT_NE(nullptr, snippetPrefix);
  ASSERT_NE(nullptr, urlPrefix);
  ASSERT_EQ(QString("clip"), snippetPrefix->prefix);
  ASSERT_EQ(QString("web"), urlPrefix->prefix);

  fs::remove_all(root);
}

TEST(ConfigTest, InvalidMainConfigSearchPrefixesWarnAndKeepDefaults) {
  const fs::path root =
      fs::temp_directory_path() / "surfpanel_invalid_prefix_main_root";
  fs::remove_all(root);

  WriteTomlFile(root, "items.toml",
                R"([[items]]
name = "Open GitHub"
type = "url"
keywords = ["git"]
[items.payload]
url = "https://github.com"

[search.prefixes]
snippet = ""
url = "s"
)");

  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);
  ASSERT_CONTAINS(result.message, "Ignoring empty search prefix");
  ASSERT_CONTAINS(result.message, "Ignoring duplicate search prefix");

  const auto *snippetPrefix =
      FindPrefixByType(result.searchPrefixes, "snippet");
  const auto *urlPrefix = FindPrefixByType(result.searchPrefixes, "url");
  ASSERT_NE(nullptr, snippetPrefix);
  ASSERT_NE(nullptr, urlPrefix);
  ASSERT_EQ(QString("s"), snippetPrefix->prefix);
  ASSERT_EQ(QString("u"), urlPrefix->prefix);

  fs::remove_all(root);
}

TEST(ConfigTest, MissingImportsWarnButValidMainConfigStillLoads) {
  const fs::path root =
      fs::temp_directory_path() / "surfpanel_missing_import_root";
  fs::remove_all(root);

  WriteTomlFile(root, "items.toml",
                R"(imports = ["missing/items.toml"]

[[items]]
name = "Local"
type = "url"
keywords = ["local"]
[items.payload]
url = "https://example.com/local"
)");

  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);
  ASSERT_CONTAINS(result.message, "Missing config source");
  ASSERT_EQ(std::size_t(1), result.items.size());
  ASSERT_EQ(QString("Local"), result.items[0].name);

  fs::remove_all(root);
}

TEST(ConfigTest, FallbackUsesLastGoodOnFailure) {
  const fs::path root = fs::temp_directory_path() / "surfpanel_fallback_root";
  fs::remove_all(root);

  WriteTomlFile(root / "cache", "compiled.toml",
                R"([[items]]
name = "Fallback"
type = "url"
keywords = ["fallback"]
[items.payload]
url = "https://example.com"
)");

  WriteTomlFile(root, "items.toml", "[[items]\n");

  const auto result = LoadConfigWithFallback(root);
  ASSERT_TRUE(result.ok);
  ASSERT_TRUE(result.usedFallback);
  ASSERT_EQ(std::size_t(1), result.items.size());
  ASSERT_EQ(QString("Fallback"), result.items[0].name);

  fs::remove_all(root);
}

int main() { return RUN_ALL_TESTS(); }
