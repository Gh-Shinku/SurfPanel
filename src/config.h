#ifndef SURFPANEL_CONFIG_H
#define SURFPANEL_CONFIG_H

#include "item.h"
#include "variable_resolver.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct ConfigLoadResult {
  std::vector<StringItem> items;
  std::vector<SearchPrefixRule> searchPrefixes;
  VariableSettings variableSettings;
  std::filesystem::path configRoot;
  bool usedFallback = false;
  bool ok = true;
  std::string message;
};

std::vector<SearchPrefixRule> DefaultSearchPrefixes();
bool InitializeConfigRoot(const std::filesystem::path &configRoot);
std::optional<std::filesystem::path> FindConfigRoot();
ConfigLoadResult LoadConfigFromRoot(const std::filesystem::path &configRoot);
ConfigLoadResult
LoadConfigWithFallback(const std::filesystem::path &configRoot);

std::vector<StringItem> loadStringItems(const std::filesystem::path &path);
void load_config(const std::filesystem::path &path);

#endif // SURFPANEL_CONFIG_H
