#include "config.h"
#include "toml.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QString>
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct ParsedItem {
  StringItem item;
  std::string key;
  bool disabled = false;
};

struct MainConfig {
  std::vector<fs::path> imports;
  std::vector<SearchPrefixRule> searchPrefixes;
};

std::vector<SearchPrefixRule> MakeDefaultSearchPrefixes() {
  return {
      SearchPrefixRule{QString("snippet"), QString("s")},
      SearchPrefixRule{QString("url"), QString("u")},
  };
}

std::string NormalizeKeyPart(const std::string &value) {
  return QString::fromStdString(value).toLower().toStdString();
}

std::string NormalizeKeyPart(const QString &value) {
  return value.toLower().toStdString();
}

std::string BuildItemKey(const std::string &id, const QString &type,
                         const QString &name) {
  if (!id.empty()) {
    return "id:" + NormalizeKeyPart(id);
  }
  return "type:" + NormalizeKeyPart(type) + "|name:" + NormalizeKeyPart(name);
}

bool HasPrefixConflict(const std::vector<SearchPrefixRule> &rules,
                       const QString &type, const QString &prefix) {
  for (const auto &rule : rules) {
    if (rule.itemType.compare(type, Qt::CaseInsensitive) != 0 &&
        rule.prefix == prefix) {
      return true;
    }
  }
  return false;
}

void UpsertPrefixRule(std::vector<SearchPrefixRule> *rules, const QString &type,
                      const QString &prefix) {
  for (auto &rule : *rules) {
    if (rule.itemType.compare(type, Qt::CaseInsensitive) == 0) {
      rule.prefix = prefix;
      return;
    }
  }
  rules->push_back(SearchPrefixRule{type.toLower(), prefix});
}

std::vector<QString> ParseKeywords(const toml::value &item) {
  std::vector<QString> keywords;
  const auto keywordsValue =
      toml::find_or(item, "keywords", toml::value{toml::array{}});
  if (!keywordsValue.is_array()) {
    throw std::runtime_error("keywords must be an array of strings");
  }

  for (const auto &kw : keywordsValue.as_array()) {
    keywords.push_back(QString::fromStdString(kw.as_string()));
  }
  return keywords;
}

ParsedItem ParseItem(const toml::value &entry) {
  if (!entry.is_table()) {
    throw std::runtime_error("item entry is not a table");
  }

  const bool disabled = toml::find_or(entry, "disabled", false);
  const std::string id = toml::find_or(entry, "id", std::string{});
  const std::string name = toml::find_or(entry, "name", std::string{});
  const std::string type = toml::find_or(entry, "type", std::string{});

  ParsedItem parsed;
  parsed.disabled = disabled;
  parsed.key = BuildItemKey(id, QString::fromStdString(type),
                            QString::fromStdString(name));

  if (disabled) {
    if (id.empty() && (name.empty() || type.empty())) {
      throw std::runtime_error(
          "disabled item requires id or both name and type");
    }
    return parsed;
  }

  if (name.empty() || type.empty()) {
    throw std::runtime_error("item requires name and type");
  }

  StringItem item;
  item.name = QString::fromStdString(name);
  item.type = QString::fromStdString(type).toLower();
  item.keywords = ParseKeywords(entry);

  const auto &payload = toml::find(entry, "payload");
  if (item.type == "url") {
    UrlPayload urlPayload;
    urlPayload.url =
        QString::fromStdString(toml::find<std::string>(payload, "url"));
    item.payload = urlPayload;
  } else if (item.type == "snippet") {
    SnippetPayload snippetPayload;
    snippetPayload.snippet =
        QString::fromStdString(toml::find<std::string>(payload, "snippet"));
    item.payload = snippetPayload;
  } else {
    throw std::runtime_error("Unknown item type: " + item.type.toStdString() +
                             " for item: " + item.name.toStdString());
  }

  parsed.item = std::move(item);
  return parsed;
}

std::vector<ParsedItem> ParseItemsFile(const fs::path &path) {
  if (!fs::exists(path)) {
    throw std::runtime_error("Configuration file not found: " + path.string());
  }

  try {
    auto config = toml::parse(path, toml::spec::v(1, 1, 0));
    const auto tomlItems =
        toml::find_or(config, "items", toml::value{toml::array{}});
    if (!tomlItems.is_array()) {
      throw std::runtime_error("items must be an array");
    }

    std::vector<ParsedItem> parsed;
    const auto &itemsArray = tomlItems.as_array();
    parsed.reserve(itemsArray.size());
    for (const auto &it : itemsArray) {
      parsed.push_back(ParseItem(it));
    }

    return parsed;
  } catch (const toml::syntax_error &err) {
    throw std::runtime_error("TOML parse error in " + path.string() + ":\n" +
                             std::string(err.what()));
  } catch (const std::runtime_error &) {
    throw;
  } catch (const std::exception &e) {
    throw std::runtime_error("Failed to load config from " + path.string() +
                             ":\n" + e.what());
  }
}

void RebuildIndex(const std::vector<std::string> &keys,
                  std::unordered_map<std::string, std::size_t> *index) {
  index->clear();
  for (std::size_t i = 0; i < keys.size(); ++i) {
    index->emplace(keys[i], i);
  }
}

void ApplyParsedItems(std::vector<StringItem> *items,
                      std::vector<std::string> *keys,
                      std::unordered_map<std::string, std::size_t> *index,
                      const std::vector<ParsedItem> &parsed) {
  for (const auto &entry : parsed) {
    if (entry.disabled) {
      auto it = index->find(entry.key);
      if (it != index->end()) {
        const std::size_t target = it->second;
        items->erase(items->begin() + static_cast<std::ptrdiff_t>(target));
        keys->erase(keys->begin() + static_cast<std::ptrdiff_t>(target));
        RebuildIndex(*keys, index);
      }
      continue;
    }

    auto it = index->find(entry.key);
    if (it != index->end()) {
      (*items)[it->second] = entry.item;
      (*keys)[it->second] = entry.key;
    } else {
      items->push_back(entry.item);
      keys->push_back(entry.key);
      index->emplace(entry.key, items->size() - 1);
    }
  }
}

std::vector<fs::path> ListTomlFiles(const fs::path &dir) {
  std::vector<fs::path> files;
  std::error_code ec;
  if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
    return files;
  }

  for (const auto &entry : fs::directory_iterator(dir, ec)) {
    if (entry.is_regular_file(ec) && entry.path().extension() == ".toml") {
      files.push_back(entry.path());
    }
  }

  std::sort(files.begin(), files.end());
  return files;
}

std::vector<fs::path> ExpandSourcePath(const fs::path &root,
                                       const fs::path &source,
                                       std::vector<std::string> *warnings) {
  const fs::path resolved = source.is_absolute() ? source : (root / source);
  std::error_code ec;
  if (!fs::exists(resolved, ec)) {
    warnings->push_back("Missing config source: " + resolved.string());
    return {};
  }

  if (fs::is_directory(resolved, ec)) {
    const auto files = ListTomlFiles(resolved);
    if (files.empty()) {
      warnings->push_back("No TOML files found in: " + resolved.string());
    }
    return files;
  }

  return {resolved};
}

void ReadSearchPrefixes(const toml::value &root,
                        std::vector<SearchPrefixRule> *rules,
                        std::vector<std::string> *warnings) {
  const auto searchValue =
      toml::find_or(root, "search", toml::value{toml::table{}});
  if (!searchValue.is_table()) {
    warnings->push_back("search must be a table; using default search config");
    return;
  }

  const auto prefixesValue =
      toml::find_or(searchValue, "prefixes", toml::value{toml::table{}});
  if (!prefixesValue.is_table()) {
    warnings->push_back(
        "search.prefixes must be a table; using default search prefixes");
    return;
  }

  for (const auto &entry : prefixesValue.as_table()) {
    const QString type = QString::fromStdString(entry.first).toLower();
    if (!entry.second.is_string()) {
      warnings->push_back("Ignoring search prefix for " + entry.first +
                          ": value must be a string");
      continue;
    }

    const QString prefix = QString::fromStdString(entry.second.as_string());
    if (prefix.isEmpty()) {
      warnings->push_back("Ignoring empty search prefix for " + entry.first);
      continue;
    }
    if (HasPrefixConflict(*rules, type, prefix)) {
      warnings->push_back("Ignoring duplicate search prefix '" +
                          prefix.toStdString() + "' for " + entry.first);
      continue;
    }

    UpsertPrefixRule(rules, type, prefix);
  }
}

MainConfig ReadMainConfig(const fs::path &mainPath,
                          std::vector<std::string> *warnings) {
  auto root = toml::parse(mainPath, toml::spec::v(1, 1, 0));
  MainConfig config;
  config.searchPrefixes = MakeDefaultSearchPrefixes();

  const auto importsValue =
      toml::find_or(root, "imports", toml::value{toml::array{}});
  if (!importsValue.is_array()) {
    throw std::runtime_error("imports must be an array of strings");
  }

  for (const auto &importEntry : importsValue.as_array()) {
    if (!importEntry.is_string()) {
      warnings->push_back("Ignoring config import: value must be a string");
      continue;
    }
    config.imports.push_back(fs::path(importEntry.as_string()));
  }

  ReadSearchPrefixes(root, &config.searchPrefixes, warnings);

  return config;
}

std::string JoinMessages(const std::vector<std::string> &messages) {
  if (messages.empty()) {
    return {};
  }

  std::ostringstream oss;
  for (std::size_t i = 0; i < messages.size(); ++i) {
    if (i > 0) {
      oss << '\n';
    }
    oss << messages[i];
  }
  return oss.str();
}

bool WriteItemsToToml(const fs::path &path,
                      const std::vector<StringItem> &items,
                      std::string *error) {
  toml::array itemsArray;
  for (const auto &item : items) {
    toml::table itemTable;
    itemTable["name"] = item.name.toStdString();
    itemTable["type"] = item.type.toStdString();

    toml::array keywords;
    for (const auto &kw : item.keywords) {
      keywords.push_back(kw.toStdString());
    }
    itemTable["keywords"] = keywords;

    toml::table payload;
    if (item.type == "url" &&
        std::holds_alternative<UrlPayload>(item.payload)) {
      payload["url"] = std::get<UrlPayload>(item.payload).url.toStdString();
    } else if (item.type == "snippet" &&
               std::holds_alternative<SnippetPayload>(item.payload)) {
      payload["snippet"] =
          std::get<SnippetPayload>(item.payload).snippet.toStdString();
    } else {
      continue;
    }
    itemTable["payload"] = payload;
    itemsArray.push_back(itemTable);
  }

  toml::table root;
  root["items"] = itemsArray;

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    if (error != nullptr) {
      *error = "Failed to write config cache: " + path.string();
    }
    return false;
  }

  out << toml::format(toml::value(root));
  return true;
}

ConfigLoadResult TryLoadFallback(const fs::path &configRoot,
                                 const std::string &previousMessage) {
  ConfigLoadResult fallback;
  fallback.configRoot = configRoot;
  fallback.searchPrefixes = MakeDefaultSearchPrefixes();
  fallback.ok = false;

  const std::vector<fs::path> candidates = {
      configRoot / "cache" / "compiled.toml",
  };

  for (const auto &candidate : candidates) {
    if (!fs::exists(candidate)) {
      continue;
    }
    try {
      fallback.items = loadStringItems(candidate);
      fallback.usedFallback = true;
      fallback.ok = true;
      fallback.message = "Config load failed; using fallback: " +
                         candidate.filename().string();
      if (!previousMessage.empty()) {
        fallback.message += "\n" + previousMessage;
      }
      return fallback;
    } catch (const std::exception &e) {
      fallback.message = std::string("Fallback config failed: ") + e.what();
    }
  }

  if (!previousMessage.empty()) {
    fallback.message = previousMessage;
  }
  return fallback;
}

} // namespace

std::vector<SearchPrefixRule> DefaultSearchPrefixes() {
  return MakeDefaultSearchPrefixes();
}

std::optional<fs::path> FindConfigRoot() {
  const QString appDir = QCoreApplication::applicationDirPath();
  const std::vector<fs::path> candidates = {
      fs::path(appDir.toStdString()) / "config",
      fs::path(appDir.toStdString()) / ".." / "config", // for debug
  };

  for (const auto &candidate : candidates) {
    std::error_code ec;
    if (fs::exists(candidate, ec) && fs::is_directory(candidate, ec)) {
      return candidate;
    }
  }

  return std::nullopt;
}

ConfigLoadResult LoadConfigFromRoot(const fs::path &configRoot) {
  ConfigLoadResult result;
  result.configRoot = configRoot;
  result.searchPrefixes = MakeDefaultSearchPrefixes();

  std::vector<std::string> warnings;
  std::vector<fs::path> sourceEntries;
  const fs::path mainPath = configRoot / "items.toml";

  if (!fs::exists(mainPath)) {
    result.ok = false;
    result.message = "Config file not found: " + mainPath.string();
    return result;
  }

  try {
    const MainConfig mainConfig = ReadMainConfig(mainPath, &warnings);
    sourceEntries = mainConfig.imports;
    sourceEntries.push_back(mainPath);
    result.searchPrefixes = mainConfig.searchPrefixes;
  } catch (const toml::syntax_error &err) {
    result.ok = false;
    result.message = "TOML parse error in " + mainPath.string() + ":\n" +
                     std::string(err.what());
    return result;
  } catch (const std::exception &e) {
    result.ok = false;
    result.message =
        "Failed to read config: " + mainPath.string() + "\n" + e.what();
    return result;
  }

  std::vector<StringItem> mergedItems;
  std::vector<std::string> mergedKeys;
  std::unordered_map<std::string, std::size_t> index;
  int loadedSources = 0;

  for (const auto &entry : sourceEntries) {
    const auto expanded = ExpandSourcePath(configRoot, entry, &warnings);
    for (const auto &sourceFile : expanded) {
      try {
        const auto parsed = ParseItemsFile(sourceFile);
        ApplyParsedItems(&mergedItems, &mergedKeys, &index, parsed);
        ++loadedSources;
      } catch (const std::exception &e) {
        warnings.push_back("Failed to load source: " + sourceFile.string() +
                           "\n" + e.what());
      }
    }
  }

  if (loadedSources == 0) {
    result.ok = false;
    result.message = "No config sources loaded. Check items.toml and imports.";
    return result;
  }

  result.items = std::move(mergedItems);
  result.message = JoinMessages(warnings);

  const fs::path cacheDir = configRoot / "cache";
  std::error_code ec;
  fs::create_directories(cacheDir, ec);

  const fs::path lastGoodPath = cacheDir / "compiled.toml";

  std::string cacheError;
  if (!WriteItemsToToml(lastGoodPath, result.items, &cacheError)) {
    warnings.push_back(cacheError);
  }

  result.message = JoinMessages(warnings);
  return result;
}

ConfigLoadResult LoadConfigWithFallback(const fs::path &configRoot) {
  ConfigLoadResult result = LoadConfigFromRoot(configRoot);
  if (result.ok) {
    return result;
  }

  const std::string previousMessage = result.message;
  return TryLoadFallback(configRoot, previousMessage);
}

std::vector<StringItem> loadStringItems(const fs::path &path) {
  const auto parsed = ParseItemsFile(path);
  std::vector<StringItem> items;
  items.reserve(parsed.size());
  for (const auto &entry : parsed) {
    if (!entry.disabled) {
      items.push_back(entry.item);
    }
  }
  return items;
}

void load_config(const fs::path &path) {
  const auto items = loadStringItems(path);
  qDebug() << "Successfully loaded" << items.size() << "items from"
           << QString::fromStdString(path.string());
}
