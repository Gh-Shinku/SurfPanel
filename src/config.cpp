#include "config.h"
#include "toml.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QString>
#include <algorithm>
#include <cstddef>
#include <cstdlib>
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

std::vector<fs::path> ReadProfileSources(const fs::path &root,
                                         const fs::path &profilePath) {
  auto profile = toml::parse(profilePath, toml::spec::v(1, 1, 0));
  const auto sourcesValue =
      toml::find_or(profile, "sources", toml::value{toml::array{}});
  if (!sourcesValue.is_array()) {
    throw std::runtime_error("sources must be an array of strings");
  }

  std::vector<fs::path> sources;
  for (const auto &src : sourcesValue.as_array()) {
    sources.push_back(fs::path(src.as_string()));
  }
  return sources;
}

std::optional<fs::path> FindProfilePath(const fs::path &root) {
  const fs::path active = root / "profiles" / "active.profile.toml";
  if (fs::exists(active)) {
    return active;
  }

  const fs::path fallback = root / "profiles" / "default.profile.toml";
  if (fs::exists(fallback)) {
    return fallback;
  }

  return std::nullopt;
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
  fallback.ok = false;

  const std::vector<fs::path> candidates = {
      configRoot / "cache" / "last_good.toml",
      configRoot / "defaults" / "items.toml",
      configRoot / "test.toml",
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

std::optional<fs::path> FindConfigRoot() {
  const char *env = std::getenv("SURFPANEL_CONFIG_DIR");
  if (env != nullptr && *env != '\0') {
    fs::path envPath(env);
    if (fs::exists(envPath) && fs::is_directory(envPath)) {
      return envPath;
    }
  }

  const QString appDir = QCoreApplication::applicationDirPath();
  const std::vector<fs::path> candidates = {
      fs::path("config"),
      fs::path("..") / "config",
      fs::path(appDir.toStdString()) / "config",
      fs::path(appDir.toStdString()) / ".." / "config",
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

  std::vector<std::string> warnings;
  std::vector<fs::path> sourceEntries;

  if (const auto profilePath = FindProfilePath(configRoot); profilePath) {
    result.profilePath = *profilePath;
    try {
      sourceEntries = ReadProfileSources(configRoot, *profilePath);
    } catch (const std::exception &e) {
      warnings.push_back("Failed to read profile: " + profilePath->string() +
                         "\n" + e.what());
    }
  }

  if (sourceEntries.empty()) {
    const fs::path defaultsPath = configRoot / "defaults" / "items.toml";
    if (fs::exists(defaultsPath)) {
      sourceEntries.push_back(defaultsPath);
      warnings.push_back("Profile missing or empty; using defaults/items.toml");
    } else {
      const fs::path legacyPath = configRoot / "test.toml";
      if (fs::exists(legacyPath)) {
        sourceEntries.push_back(legacyPath);
        warnings.push_back("Profile missing; using legacy test.toml");
      }
    }
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
    result.message =
        "No config sources loaded. Check your profile and defaults.";
    return result;
  }

  const fs::path patchDir = configRoot / "user" / "patches";
  const auto patchFiles = ListTomlFiles(patchDir);
  for (const auto &patchFile : patchFiles) {
    try {
      const auto parsed = ParseItemsFile(patchFile);
      ApplyParsedItems(&mergedItems, &mergedKeys, &index, parsed);
    } catch (const std::exception &e) {
      warnings.push_back("Failed to load patch: " + patchFile.string() + "\n" +
                         e.what());
    }
  }

  const fs::path overrideDir = configRoot / "user" / "overrides";
  const auto overrideFiles = ListTomlFiles(overrideDir);
  for (const auto &overrideFile : overrideFiles) {
    try {
      const auto parsed = ParseItemsFile(overrideFile);
      ApplyParsedItems(&mergedItems, &mergedKeys, &index, parsed);
    } catch (const std::exception &e) {
      warnings.push_back("Failed to load override: " + overrideFile.string() +
                         "\n" + e.what());
    }
  }

  result.items = std::move(mergedItems);
  result.message = JoinMessages(warnings);

  const fs::path cacheDir = configRoot / "cache";
  std::error_code ec;
  fs::create_directories(cacheDir, ec);

  const fs::path compiledPath = cacheDir / "compiled.toml";
  const fs::path lastGoodPath = cacheDir / "last_good.toml";

  std::string cacheError;
  if (!WriteItemsToToml(compiledPath, result.items, &cacheError)) {
    warnings.push_back(cacheError);
  }
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