#pragma once

#include <QColor>
#include <algorithm>
#include <cmath>

struct PaletteColors {
  QColor text;
  QColor secondary;
  QColor icon;
  QColor nativeTint;
  QColor inputSurface;
  QColor inputBorder;
  QColor selectedRow;
  QColor hoveredRow;
};

inline PaletteColors ColorsForPalette(bool dark) {
  if (dark) {
    return {QColor("#F1F5F9"),         QColor("#CBD5E1"),
            QColor("#CBD5E1"),         QColor(24, 24, 24, 208),
            QColor(24, 24, 24, 80),    QColor(255, 255, 255, 45),
            QColor(255, 255, 255, 28), QColor(255, 255, 255, 14)};
  }
  return {QColor("#1F1F1F"),          QColor("#525252"),
          QColor("#616161"),          QColor(250, 250, 250, 220),
          QColor(255, 255, 255, 170), QColor(0, 0, 0, 12),
          QColor(0, 0, 0, 16),        QColor(0, 0, 0, 7)};
}

// Alpha compositing in the same sRGB channel space used by the widget surfaces.
inline QColor CompositePaletteColor(const QColor &foreground,
                                    const QColor &background) {
  const qreal alpha = foreground.alphaF();
  return QColor::fromRgbF(
      foreground.redF() * alpha + background.redF() * (1 - alpha),
      foreground.greenF() * alpha + background.greenF() * (1 - alpha),
      foreground.blueF() * alpha + background.blueF() * (1 - alpha));
}

inline double PaletteLuminance(const QColor &color) {
  const auto linear = [](double channel) {
    return channel <= 0.04045 ? channel / 12.92
                              : std::pow((channel + 0.055) / 1.055, 2.4);
  };
  return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) +
         0.0722 * linear(color.blueF());
}

inline double PaletteContrast(const QColor &first, const QColor &second) {
  const double a = PaletteLuminance(first);
  const double b = PaletteLuminance(second);
  return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

inline QColor ReadablePaletteAccent(QColor accent, bool dark) {
  if (!accent.isValid()) {
    accent = QColor("#005FB8");
  }
  accent.setAlpha(255);
  const auto colors = ColorsForPalette(dark);
  const QColor worstBase =
      CompositePaletteColor(colors.nativeTint, dark ? Qt::white : Qt::black);
  const QColor row = CompositePaletteColor(colors.selectedRow, worstBase);
  const QColor input = CompositePaletteColor(colors.inputSurface, worstBase);
  // Preserve the accent's hue, lightening/darkening only as much as required
  // for a visible small focus/selection indicator on the worst-case backdrop.
  for (int step = 0; step <= 100; ++step) {
    QColor target = dark ? QColor(Qt::white) : QColor(Qt::black);
    target.setAlphaF(step / 100.0);
    const QColor candidate = CompositePaletteColor(target, accent);
    if (PaletteContrast(candidate, row) >= 3.0 &&
        PaletteContrast(candidate, input) >= 3.0) {
      return candidate;
    }
  }
  return dark ? QColor(Qt::white) : QColor(Qt::black);
}
