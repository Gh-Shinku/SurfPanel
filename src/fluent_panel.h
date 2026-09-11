#pragma once

#include <QFrame>

class FluentPanel final : public QFrame {
public:
  explicit FluentPanel(QWidget *parent = nullptr);

  void setThemeColors(const QColor &baseColor, const QColor &borderColor,
                      bool darkMode);
  void setCornerRadius(int radius);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QColor baseColor_;
  QColor borderColor_;
  int cornerRadius_;
  bool darkMode_;
};
