#include "toml.hpp"
#include <QDebug>
#include <QString>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

enum class ItemType { SNIPPET, URL };

// payload should contain different resource depends on ItemType
struct ItemPayload {
  QString url;
};

struct Item {
  uint32_t id;
  QString name;
  ItemType type;
  std::vector<QString> keywords;
  ItemPayload payload;
};

void load_config(const fs::path &path) {
  if (!fs::exists(path)) {
    throw std::runtime_error("Configuration file not found: " + path.string());
  }

  try {
    auto config = toml::parse(path, toml::spec::v(1, 1, 0));

    const auto items = toml::find(config, "items").as_array();
    for (const auto it : items) {
      qDebug() << it.at("name").as_string() << " ";
    }

    qDebug() << "Successfully loaded config from:" << path.string();
  } catch (const toml::syntax_error &err) {
    throw std::runtime_error("TOML parse error in " + path.string() + ":\n" +
                             std::string(err.what()));
  } catch (const std::exception &e) {
    throw std::runtime_error("Failed to load config from " + path.string() +
                             ": " + e.what());
  }
}