#pragma once
#include <QLineEdit>

class PaletteSearchInput final : public QLineEdit {
public:
  explicit PaletteSearchInput(QWidget *parent = nullptr);
  void setAccentColor(const QColor &color);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QColor accentColor_{"#005FB8"};
};
