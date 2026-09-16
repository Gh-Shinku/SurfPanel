#pragma once

#include <QMenu>

// Compact desktop-menu metrics with deterministic, DPI-aware checkmarks.
class TrayMenu : public QMenu {
public:
  explicit TrayMenu(QWidget *parent = nullptr);
  void setDarkMode(bool dark);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  bool dark_ = false;
};
