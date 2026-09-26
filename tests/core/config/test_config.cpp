#include "core/config/config.h"
#include "core/search/item.h"
#include "core/storage/app_paths.h"
#include "support/test_harness.h"
#include <QStandardPaths>
#include <QTemporaryDir>
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

TEST(ConfigTest, UsesStableOrganizationIndependentConfigPath) {
  const QString genericConfig =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#ifdef Q_OS_WIN
  const fs::path expected =
      fs::path(genericConfig.toStdWString()) / "SurfPanel" / "config";
#else
  const fs::path expected =
      fs::u8path(genericConfig.toUtf8().toStdString()) / "SurfPanel" / "config";
#endif
  ASSERT_EQ(expected.lexically_normal(), UserConfigRoot().lexically_normal());
}

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

TEST(ConfigTest, PluginItemsSupportOverridesDisabledEntriesAndFallback) {
  const auto root = fs::temp_directory_path() / "surfpanel_plugin_items_root";
  fs::remove_all(root);
  WriteTomlFile(root / "packages" / "demo", "items.toml", R"(
[[items]]
name = "My Filter"
type = "plugin"
[items.payload]
plugin = "clipboard-filter"
function = "other"
[[items]]
name = "Disabled Filter"
type = "plugin"
[items.payload]
plugin = "clipboard-filter"
function = "filter"
)");
  WriteTomlFile(root, "items.toml", R"(
imports = ["packages/demo/items.toml"]
[search.prefixes]
plugin = "p"
[[items]]
name = "My Filter"
type = "plugin"
keywords = ["my-alias"]
[items.payload]
plugin = "clipboard-filter"
function = "filter"
[[items]]
name = "Disabled Filter"
type = "plugin"
disabled = true
)");
  const auto loaded = LoadConfigWithFallback(root);
  ASSERT_TRUE(loaded.ok);
  ASSERT_EQ(std::size_t(1), loaded.items.size());
  ASSERT_EQ(QString("my-alias"), loaded.items[0].keywords[0]);
  ASSERT_EQ(QString("filter"),
            std::get<PluginPayload>(loaded.items[0].payload).function);
  ASSERT_EQ(QString("p"),
            FindPrefixByType(loaded.searchPrefixes, "plugin")->prefix);
  WriteTomlFile(root, "items.toml", "[[items]\n");
  const auto fallback = LoadConfigWithFallback(root);
  ASSERT_TRUE(fallback.usedFallback);
  ASSERT_EQ(std::size_t(1), fallback.items.size());
  const auto target = std::get<PluginPayload>(fallback.items[0].payload);
  ASSERT_EQ(QString("clipboard-filter"), target.plugin);
  ASSERT_EQ(QString("filter"), target.function);
  fs::remove_all(root);
}

TEST(ConfigTest, PluginTargetsRequireNonEmptyStringFields) {
  for (const auto &payload : std::vector<std::string>{
           "plugin = \"\"\nfunction = \"filter\"\n",
           "plugin = \"clipboard-filter\"\nfunction = \" \"\n",
           "plugin = \"clipboard-filter\"\n",
           "plugin = 42\nfunction = \"filter\"\n"}) {
    const auto path = WriteTomlFile(
        "[[items]]\nname = \"Filter\"\ntype = \"plugin\"\n[items.payload]\n" +
            payload,
        "surfpanel_invalid_plugin_item.toml");
    CaptureRuntimeError([&]() { static_cast<void>(loadStringItems(path)); });
    fs::remove(path);
  }
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

TEST(ConfigTest, ImportsUnicodePaths) {
  const fs::path root =
      fs::temp_directory_path() / "surfpanel_unicode_import_root_配置";
  fs::remove_all(root);

  WriteTomlFile(root / "packages", "项目.toml",
                R"([[items]]
name = "Unicode"
type = "url"
[items.payload]
url = "https://example.com/unicode"
)");
  WriteTomlFile(root, "items.toml",
                R"(imports = ["packages/项目.toml"]
)");

  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(std::size_t(1), result.items.size());
  ASSERT_EQ(QString("Unicode"), result.items[0].name);

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

TEST(ConfigTest, WhitespaceAndCaseInsensitiveDuplicatePrefixesAreIgnored) {
  const fs::path root =
      fs::temp_directory_path() / "surfpanel_invalid_prefix_spacing_root";
  fs::remove_all(root);

  WriteTomlFile(root, "items.toml",
                R"([[items]]
name = "Open GitHub"
type = "url"
keywords = ["git"]
[items.payload]
url = "https://github.com"

[search.prefixes]
snippet = "clip mode"
url = "S"
)");

  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);
  ASSERT_CONTAINS(result.message, "must not contain whitespace");
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

TEST(ConfigTest, ImportsOutsideConfigRootAreIgnored) {
  const fs::path root =
      fs::temp_directory_path() / "surfpanel_import_safe_root";
  const fs::path outside =
      fs::temp_directory_path() / "surfpanel_import_outside.toml";
  fs::remove_all(root);
  fs::remove(outside);

  WriteTomlFile(fs::temp_directory_path(), outside.filename().string(),
                R"([[items]]
name = "Outside"
type = "url"
[items.payload]
url = "https://example.com/outside"
)");
  WriteTomlFile(root, "items.toml",
                R"(imports = ["../surfpanel_import_outside.toml"]

[[items]]
name = "Local"
type = "url"
[items.payload]
url = "https://example.com/local"
)");

  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);
  ASSERT_CONTAINS(result.message, "Ignoring config import outside root");
  ASSERT_EQ(std::size_t(1), result.items.size());
  ASSERT_EQ(QString("Local"), result.items[0].name);

  fs::remove_all(root);
  fs::remove(outside);
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

TEST(ConfigTest, ParsesDateTimeFormats) {
  const fs::path root = fs::temp_directory_path() / "surfpanel_datetime_root";
  fs::remove_all(root);

  WriteTomlFile(root, "items.toml",
                R"([[items]]
name = "Date"
type = "snippet"
[items.payload]
snippet = "{{date}}"

[datetime]
date_format = "yyyy-MM-dd"
time_format = "HH:mm"
datetime_format = "yyyy-MM-dd HH:mm"
)");

  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(QString("yyyy-MM-dd"), result.variableSettings.dateFormat);
  ASSERT_EQ(QString("HH:mm"), result.variableSettings.timeFormat);
  ASSERT_EQ(QString("yyyy-MM-dd HH:mm"),
            result.variableSettings.dateTimeFormat);

  fs::remove_all(root);
}

TEST(ConfigTest, UnsetDateTimeFormatsKeepDefaults) {
  const fs::path root =
      fs::temp_directory_path() / "surfpanel_datetime_default_root";
  fs::remove_all(root);

  WriteTomlFile(root, "items.toml",
                R"([[items]]
name = "Date"
type = "snippet"
[items.payload]
snippet = "{{date}}"
)");

  const VariableSettings defaults;
  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(defaults.dateFormat, result.variableSettings.dateFormat);
  ASSERT_EQ(defaults.timeFormat, result.variableSettings.timeFormat);
  ASSERT_EQ(defaults.dateTimeFormat, result.variableSettings.dateTimeFormat);

  fs::remove_all(root);
}

TEST(ConfigTest, InvalidDateTimeFormatsWarnAndKeepDefaults) {
  const fs::path root =
      fs::temp_directory_path() / "surfpanel_datetime_invalid_root";
  fs::remove_all(root);

  WriteTomlFile(root, "items.toml",
                R"([[items]]
name = "Date"
type = "snippet"
[items.payload]
snippet = "{{date}}"

[datetime]
date_format = "%Y-%m-%d"
time_format = ""
datetime_format = "literal"
unknown_setting = "value"
)");

  const VariableSettings defaults;
  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);
  ASSERT_CONTAINS(result.message, "Ignoring datetime.date_format");
  ASSERT_CONTAINS(result.message, "strftime");
  ASSERT_CONTAINS(result.message, "Ignoring datetime.time_format");
  ASSERT_CONTAINS(result.message, "Ignoring datetime.datetime_format");
  ASSERT_CONTAINS(result.message, "no date or time fields");
  ASSERT_CONTAINS(result.message, "Ignoring unknown datetime setting");
  ASSERT_EQ(defaults.dateFormat, result.variableSettings.dateFormat);
  ASSERT_EQ(defaults.timeFormat, result.variableSettings.timeFormat);
  ASSERT_EQ(defaults.dateTimeFormat, result.variableSettings.dateTimeFormat);

  fs::remove_all(root);
}

TEST(ConfigTest, NonStringDateTimeFormatWarnsAndKeepsDefault) {
  const fs::path root =
      fs::temp_directory_path() / "surfpanel_datetime_type_root";
  fs::remove_all(root);

  WriteTomlFile(root, "items.toml",
                R"([[items]]
name = "Date"
type = "snippet"
[items.payload]
snippet = "{{date}}"

[datetime]
date_format = 2026
)");

  const VariableSettings defaults;
  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);
  ASSERT_CONTAINS(result.message, "value must be a string");
  ASSERT_EQ(defaults.dateFormat, result.variableSettings.dateFormat);

  fs::remove_all(root);
}

TEST(ConfigTest, SuspectDateTimeFormatWarnsButIsUsed) {
  const fs::path root =
      fs::temp_directory_path() / "surfpanel_datetime_suspect_root";
  fs::remove_all(root);

  WriteTomlFile(root, "items.toml",
                R"([[items]]
name = "Date"
type = "snippet"
[items.payload]
snippet = "{{date}}"

[datetime]
date_format = "yyyy-mm-dd"
)");

  const auto result = LoadConfigFromRoot(root);
  ASSERT_TRUE(result.ok);
  ASSERT_CONTAINS(result.message, "lowercase 'm' means minutes");
  ASSERT_EQ(QString("yyyy-mm-dd"), result.variableSettings.dateFormat);

  fs::remove_all(root);
}

TEST(ConfigTest, FallbackKeepsCachedDateTimeFormats) {
  const fs::path root =
      fs::temp_directory_path() / "surfpanel_datetime_fallback_root";
  fs::remove_all(root);

  WriteTomlFile(root, "items.toml",
                R"([[items]]
name = "Date"
type = "snippet"
[items.payload]
snippet = "{{date}}"

[datetime]
date_format = "yyyy-MM-dd"
)");

  const auto cached = LoadConfigFromRoot(root);
  ASSERT_TRUE(cached.ok);

  WriteTomlFile(root, "items.toml", "[[items]\n");

  const auto result = LoadConfigWithFallback(root);
  ASSERT_TRUE(result.ok);
  ASSERT_TRUE(result.usedFallback);
  ASSERT_TRUE(result.variableSettings.dateFormat == QString("yyyy-MM-dd"));

  fs::remove_all(root);
}

TEST(ConfigTest, ThemeModesOverrideSystemAndRoundTripThroughFallback) {
  QTemporaryDir temporary;
  const fs::path root = fs::u8path(temporary.path().toUtf8().toStdString());
  for (const auto mode :
       {ThemeMode::System, ThemeMode::Light, ThemeMode::Dark}) {
    const char *name = mode == ThemeMode::System  ? "system"
                       : mode == ThemeMode::Light ? "light"
                                                  : "dark";
    WriteTomlFile(root, "items.toml",
                  std::string("items = []\n[appearance]\ntheme = \"") + name +
                      "\"\n");
    const auto result = LoadConfigFromRoot(root);
    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(result.themeMode == mode);
    ASSERT_TRUE(result.message.empty());
    for (bool systemDark : {false, true}) {
      ASSERT_EQ(mode == ThemeMode::Dark ||
                    (mode == ThemeMode::System && systemDark),
                ResolveDarkTheme(result.themeMode, systemDark));
    }
    WriteTomlFile(root, "items.toml", "invalid = [");
    const auto cached = LoadConfigWithFallback(root);
    ASSERT_TRUE(cached.usedFallback);
    ASSERT_TRUE(cached.themeMode == mode);
  }
}

TEST(ConfigTest, MissingAndInvalidThemesFollowSystem) {
  QTemporaryDir temporary;
  const fs::path root = fs::u8path(temporary.path().toUtf8().toStdString());
  WriteTomlFile(root, "items.toml", "items = []\n");
  const auto defaults = LoadConfigFromRoot(root);
  ASSERT_TRUE(defaults.ok);
  ASSERT_TRUE(defaults.themeMode == ThemeMode::System);
  ASSERT_TRUE(defaults.message.empty());
  for (const std::string appearance :
       {"appearance = 42", "[appearance]\ntheme = 42",
        "[appearance]\ntheme = \"unknown\"",
        "[appearance]\ntheme = \"Dark\""}) {
    WriteTomlFile(root, "items.toml", "items = []\n" + appearance);
    const auto invalid = LoadConfigFromRoot(root);
    ASSERT_TRUE(invalid.ok);
    ASSERT_TRUE(invalid.themeMode == ThemeMode::System);
    ASSERT_TRUE(invalid.message.find("appearance.theme") != std::string::npos);
  }
}

TEST(ConfigTest, NewUserConfigurationIsEmptyAndExistingFilesArePreserved) {
  QTemporaryDir temporary;
  const fs::path root =
      fs::u8path(temporary.path().toUtf8().toStdString()) / "config";
  ASSERT_TRUE(InitializeConfigRoot(root));
  const auto fresh = LoadConfigFromRoot(root);
  ASSERT_TRUE(fresh.ok);
  ASSERT_TRUE(fresh.items.empty());
  ASSERT_TRUE(!fs::exists(root / "packages"));
  ASSERT_TRUE(!fs::exists(root / "plugins" / "clipboard-filter.toml"));
  WriteTomlFile(root, "items.toml", "# User config\nitems = []\n");
  WriteTomlFile(root / "plugins", "clipboard-filter.toml", "enabled = true\n");
  ASSERT_TRUE(InitializeConfigRoot(root));
  std::ifstream input(root / "items.toml");
  std::string firstLine;
  std::getline(input, firstLine);
  ASSERT_EQ(std::string("# User config"), firstLine);
  ASSERT_TRUE(fs::exists(root / "plugins" / "clipboard-filter.toml"));
  input.close();
  fs::remove(root / "items.toml");
  ASSERT_TRUE(InitializeConfigRoot(root));
  ASSERT_TRUE(!fs::exists(root / "items.toml"));
}

int main() { return RUN_ALL_TESTS(); }
