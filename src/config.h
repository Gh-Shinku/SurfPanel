#ifndef SURFPANEL_CONFIG_H
#define SURFPANEL_CONFIG_H

#include "item.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct ConfigLoadResult {
  std::vector<StringItem> items;
  std::filesystem::path configRoot;
  std::filesystem::path profilePath;
  bool usedFallback = false;
  bool ok = true;
  std::string message;
};

std::optional<std::filesystem::path> FindConfigRoot();
ConfigLoadResult LoadConfigFromRoot(const std::filesystem::path &configRoot);
ConfigLoadResult
LoadConfigWithFallback(const std::filesystem::path &configRoot);

std::vector<StringItem> loadStringItems(const std::filesystem::path &path);
void load_config(const std::filesystem::path &path);

#endif // SURFPANEL_CONFIG_H