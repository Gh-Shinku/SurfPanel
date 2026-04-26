#include "config.h"
#include "toml.hpp"
#include <QDebug>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

std::vector<StringItem> loadStringItems(const fs::path &path) {
  if (!fs::exists(path)) {
    throw std::runtime_error("Configuration file not found: " + path.string());
  }

  std::vector<StringItem> items;

  try {
    auto config = toml::parse(path, toml::spec::v(1, 1, 0));

    const auto tomlItems = toml::find(config, "items").as_array();
    items.reserve(tomlItems.size());
    for (const auto &it : tomlItems) {
      StringItem item;
      item.name = QString::fromStdString(toml::find<std::string>(it, "name"));
      item.type = QString::fromStdString(toml::find<std::string>(it, "type"));

      for (const auto &kw : it.at("keywords").as_array()) {
        item.keywords.push_back(QString::fromStdString(kw.as_string()));
      }

      const auto &payload = toml::find(it, "payload");
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
        throw std::runtime_error(
            "Unknown item type: " + item.type.toStdString() +
            " for item: " + item.name.toStdString());
      }

      items.push_back(item);
    }

    return items;
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

void load_config(const fs::path &path) {
  const auto items = loadStringItems(path);
  qDebug() << "Successfully loaded" << items.size() << "items from"
           << QString::fromStdString(path.string());
}