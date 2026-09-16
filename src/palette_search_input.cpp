#include "palette_search_input.h"
#include <QPainter>

PaletteSearchInput::PaletteSearchInput(QWidget *parent) : QLineEdit(parent) {
  setFixedHeight(44);
  setTextMargins(28, 0, 0, 0);
  QFont font;
  font.setFamilies({"Segoe UI Variable Text", "Segoe UI"});
  font.setPixelSize(14);
  setFont(font);
}
void PaletteSearchInput::setAccentColor(const QColor &color) {
  accentColor_ = color;
  update();
}
void PaletteSearchInput::paintEvent(QPaintEvent *event) {
  QLineEdit::paintEvent(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(palette().color(QPalette::Text), 1.5));
  const qreal y = height() / 2.0;
  painter.drawEllipse(QRectF(12, y - 7, 11, 11));
  painter.drawLine(QPointF(21, y + 2), QPointF(26, y + 7));
  if (hasFocus()) {
    painter.setPen(QPen(accentColor_, 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(6, height() - 1),
                     QPointF(width() - 6, height() - 1));
  }
}
