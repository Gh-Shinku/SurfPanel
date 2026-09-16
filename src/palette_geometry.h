#pragma once
#include <QRect>
#include <algorithm>

inline QRect PaletteGeometry(const QRect &available, int resultCount) {
  const int rows = std::clamp(resultCount, 1, 6);
  const int width = std::min(660, std::max(1, available.width() - 24));
  const int height =
      std::min(84 + rows * 44, std::max(1, available.height() - 24));
  const int x = available.x() + (available.width() - width) / 2;
  const int preferredY = available.y() + qRound(available.height() * 0.35);
  const int y =
      std::clamp(preferredY, available.top(), available.bottom() - height + 1);
  return {x, y, width, height};
}
