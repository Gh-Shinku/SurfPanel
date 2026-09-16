#include "search_result_view.h"

#include <QFont>
#include <QPainter>
#include <QStyleOptionViewItem>

namespace {
QString TypeLabel(const QString &type) {
  if (type == "url") {
    return "Link";
  }
  if (type == "snippet") {
    return "Snippet";
  }
  if (type == "plugin") {
    return "Plugin";
  }
  return "Item";
}
void PaintTypeIcon(QPainter *painter, const QRectF &rect, const QString &type) {
  painter->save();
  painter->translate(rect.topLeft());
  painter->setBrush(Qt::NoBrush);
  if (type == "url") {
    painter->translate(8, 8);
    painter->rotate(-35);
    painter->drawRoundedRect(QRectF(-7, -3, 9, 6), 3, 3);
    painter->drawRoundedRect(QRectF(-2, -3, 9, 6), 3, 3);
  } else if (type == "plugin") {
    QPolygonF bolt;
    bolt << QPointF(9, 1) << QPointF(3, 9) << QPointF(8, 9) << QPointF(6, 15)
         << QPointF(13, 6) << QPointF(8, 6);
    painter->drawPolygon(bolt);
  } else {
    painter->drawRoundedRect(QRectF(2, 1, 12, 14), 1, 1);
    painter->drawLine(QPointF(5, 5), QPointF(11, 5));
    painter->drawLine(QPointF(5, 8), QPointF(11, 8));
    painter->drawLine(QPointF(5, 11), QPointF(9, 11));
  }
  painter->restore();
}
} // namespace

SearchResultListModel::SearchResultListModel(QObject *parent)
    : QAbstractListModel(parent) {}

int SearchResultListModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(results_.size());
}

QVariant SearchResultListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= static_cast<int>(results_.size())) {
    return {};
  }

  const StringItem *item = results_[index.row()];
  if (item == nullptr) {
    return {};
  }

  if (role == Qt::DisplayRole) {
    return item->name;
  }
  if (role == TypeRole) {
    return item->type;
  }
  if (role == TypeLabelRole) {
    return TypeLabel(item->type.toLower());
  }
  return {};
}

void SearchResultListModel::setResults(
    std::vector<const StringItem *> results) {
  beginResetModel();
  results_ = std::move(results);
  endResetModel();
}

const StringItem *SearchResultListModel::itemAt(int row) const {
  if (row < 0 || row >= static_cast<int>(results_.size())) {
    return nullptr;
  }
  return results_[row];
}

SearchResultItemDelegate::SearchResultItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent), darkMode_(false) {}

void SearchResultItemDelegate::setTheme(bool darkMode, const QColor &accent) {
  darkMode_ = darkMode;
  accentColor_ = accent;
}

QSize SearchResultItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                         const QModelIndex &) const {
  return {option.rect.width(), 44};
}

void SearchResultItemDelegate::paint(QPainter *painter,
                                     const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  const QRect rowRect = option.rect.adjusted(0, 2, 0, -2);
  const bool selected = (option.state & QStyle::State_Selected) != 0;
  const bool hovered = (option.state & QStyle::State_MouseOver) != 0;
  if (selected || hovered) {
    const QColor rowBg =
        selected ? (darkMode_ ? QColor(255, 255, 255, 28) : QColor(0, 0, 0, 16))
                 : (darkMode_ ? QColor(255, 255, 255, 14) : QColor(0, 0, 0, 7));
    painter->setPen(Qt::NoPen);
    painter->setBrush(rowBg);
    painter->drawRoundedRect(rowRect, 6, 6);
  }
  if (selected) {
    painter->setPen(Qt::NoPen);
    painter->setBrush(accentColor_);
    painter->drawRoundedRect(
        QRectF(rowRect.left(), rowRect.center().y() - 8, 3, 16), 1.5, 1.5);
  }

  const QString name = index.data(Qt::DisplayRole).toString();
  const QString typeRaw =
      index.data(SearchResultListModel::TypeRole).toString();
  const QString typeText =
      index.data(SearchResultListModel::TypeLabelRole).toString();

  QFont nameFont = option.font;
  nameFont.setFamilies({"Segoe UI Variable Text", "Segoe UI"});
  nameFont.setPixelSize(14);
  nameFont.setWeight(QFont::Medium);
  const QFontMetrics nameMetrics(nameFont);

  QFont typeFont = option.font;
  typeFont.setPixelSize(12);
  typeFont.setWeight(QFont::Medium);
  const QFontMetrics typeMetrics(typeFont);
  const int typeWidth = typeMetrics.horizontalAdvance(typeText);
  const QRect typeRect(rowRect.right() - typeWidth - 14, rowRect.top(),
                       typeWidth, rowRect.height());
  const QRect nameRect = rowRect.adjusted(40, 0, -typeWidth - 32, 0);

  painter->setPen(QPen(darkMode_ ? QColor("#CBD5E1") : QColor("#616161"), 1.5));
  PaintTypeIcon(painter,
                QRectF(rowRect.left() + 12, rowRect.center().y() - 8, 16, 16),
                typeRaw.toLower());

  painter->setFont(nameFont);
  painter->setPen(darkMode_ ? QColor("#F1F5F9") : QColor("#1F1F1F"));
  painter->drawText(
      nameRect, Qt::AlignVCenter | Qt::AlignLeft,
      nameMetrics.elidedText(name, Qt::ElideRight, nameRect.width()));

  painter->setFont(typeFont);
  painter->setPen(darkMode_ ? QColor(148, 163, 184) : QColor(104, 104, 104));
  painter->drawText(typeRect, Qt::AlignVCenter | Qt::AlignRight, typeText);
  painter->restore();
}
