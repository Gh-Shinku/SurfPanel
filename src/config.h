#ifndef SURFPANEL_CONFIG_H
#define SURFPANEL_CONFIG_H

#include "item.h"
#include <filesystem>
#include <vector>

std::vector<StringItem> loadStringItems(const std::filesystem::path &path);
void load_config(const std::filesystem::path &path);

#endif // SURFPANEL_CONFIG_H