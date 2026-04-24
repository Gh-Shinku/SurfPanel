#include <QDebug>
#include <QString>
#include <filesystem>
#include <iostream>

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

int main() {
  qDebug() << "Hello, World!\n";
  return 0;
}