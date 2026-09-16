#include "fluent_panel.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>

FluentPanel::FluentPanel(QWidget *parent)
    : QFrame(parent), baseColor_(Qt::transparent),
      borderColor_(Qt::transparent), cornerRadius_(8), darkMode_(false) {
  setAttribute(Qt::WA_TranslucentBackground, true);
  setAutoFillBackground(false);
}

void FluentPanel::setThemeColors(const QColor &baseColor,
                                 const QColor &borderColor, bool darkMode) {
  baseColor_ = baseColor;
  borderColor_ = borderColor;
  darkMode_ = darkMode;
  update();
}

void FluentPanel::setCornerRadius(int radius) {
  if (cornerRadius_ == radius) {
    return;
  }
  cornerRadius_ = radius;
  update();
}

void FluentPanel::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QRectF rect = this->rect();
  rect.adjust(0.5, 0.5, -0.5, -0.5);
  QPainterPath path;
  path.addRoundedRect(rect, cornerRadius_, cornerRadius_);

  painter.setPen(Qt::NoPen);
  painter.setBrush(baseColor_);
  painter.drawPath(path);

  painter.setPen(QPen(borderColor_, 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(path);
}
