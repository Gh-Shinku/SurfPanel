#pragma once

#include <QString>
#include <vector>

struct ClipboardFilterConfig {
  bool enabled = false;
  std::vector<QString> sourceProcesses;
};
