#include "ui/tray/tray_menu.h"

#include <QAction>
#include <QFontDatabase>
#include <QPainter>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
QFont SystemMenuFont() {
  QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
#ifdef Q_OS_WIN
  // Query at 96 DPI and express the result in points so Qt scales it once on
  // each screen. AutoHotkey's HMENU uses the same Windows menu-font setting.
  using ParametersForDpi = BOOL(WINAPI *)(UINT, UINT, PVOID, UINT, UINT);
  const HMODULE user32 = GetModuleHandleW(L"user32.dll");
  const auto parameters =
      user32 ? reinterpret_cast<ParametersForDpi>(
                   GetProcAddress(user32, "SystemParametersInfoForDpi"))
             : nullptr;
  NONCLIENTMETRICSW metrics{};
  metrics.cbSize = sizeof(metrics);
  if (parameters &&
      parameters(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, 96) &&
      metrics.lfMenuFont.lfHeight < 0) {
    font.setFamily(QString::fromWCharArray(metrics.lfMenuFont.lfFaceName));
    font.setPointSizeF(-metrics.lfMenuFont.lfHeight * 72.0 / 96.0);
    font.setItalic(metrics.lfMenuFont.lfItalic != 0);
  }
#endif
  return font;
}
} // namespace

TrayMenu::TrayMenu(QWidget *parent) : QMenu(parent) {
  setFont(SystemMenuFont());
  setDarkMode(false);
}

void TrayMenu::setDarkMode(bool dark) {
  dark_ = dark;
  setStyleSheet(QString(R"(
    QMenu {
      background: %1;
      color: %2;
      border: 1px solid %3;
      border-radius: 4px;
      padding: 2px 0;
    }
    QMenu::item {
      padding: 2px 16px 2px 8px;
      margin: 0 2px;
      border-radius: 2px;
    }
    QMenu::item:selected { background: %4; color: %2; }
    QMenu::item:default { font-weight: bold; }
    QMenu::item:disabled { color: %5; }
    QMenu::indicator {
      width: 12px;
      height: 12px;
      image: none;
      background: transparent;
      border: 0;
    }
    QMenu::separator {
      height: 1px;
      background: %6;
      margin: 3px 8px;
    }
  )")
                    .arg(dark ? "#202020" : "#F9F9F9")
                    .arg(dark ? "#F1F1F1" : "#202020")
                    .arg(dark ? "#454545" : "#DEDEDE")
                    .arg(dark ? "#353535" : "#EAEAEA")
                    .arg(dark ? "#888888" : "#909090")
                    .arg(dark ? "#3C3C3C" : "#E4E4E4"));
  update();
}

void TrayMenu::paintEvent(QPaintEvent *event) {
  QMenu::paintEvent(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  for (auto *action : actions()) {
    if (!action->isVisible() || !action->isCheckable() ||
        !action->isChecked()) {
      continue;
    }
    const QRect row = actionGeometry(action);
    const QColor color = !action->isEnabled()
                             ? QColor(dark_ ? "#888888" : "#909090")
                             : QColor(dark_ ? "#F1F1F1" : "#202020");
    painter.setPen(
        QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF origin(row.left() + 7, row.center().y() - 6);
    painter.drawPolyline(QPolygonF{origin + QPointF(1, 6),
                                   origin + QPointF(4, 9),
                                   origin + QPointF(10, 2)});
  }
}
