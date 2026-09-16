#include "search_result_view.h"

#include <QFont>
#include <QPainter>
#include <QStyleOptionViewItem>

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

void SearchResultItemDelegate::setTheme(bool darkMode) { darkMode_ = darkMode; }

QSize SearchResultItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                         const QModelIndex &) const {
  return {option.rect.width(), 44};
}

void SearchResultItemDelegate::paint(QPainter *painter,
                                     const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  const QRect rowRect = option.rect.adjusted(6, 2, -6, -2);
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

  const QString name = index.data(Qt::DisplayRole).toString();
  const QString typeRaw =
      index.data(SearchResultListModel::TypeRole).toString();
  const QString typeText =
      typeRaw.compare("url", Qt::CaseInsensitive) == 0
          ? "URL"
          : (typeRaw.compare("plugin", Qt::CaseInsensitive) == 0 ? "PLUGIN"
                                                                 : "SNIPPET");

  QFont nameFont = option.font;
  nameFont.setPointSizeF(11.0);
  nameFont.setWeight(QFont::Medium);
  const QFontMetrics nameMetrics(nameFont);

  QFont typeFont = option.font;
  typeFont.setPointSizeF(9.0);
  typeFont.setWeight(QFont::Medium);
  const QFontMetrics typeMetrics(typeFont);
  const int typeWidth = typeMetrics.horizontalAdvance(typeText);
  const QRect typeRect(rowRect.right() - typeWidth - 14, rowRect.top(),
                       typeWidth, rowRect.height());
  const QRect nameRect = rowRect.adjusted(14, 0, -typeWidth - 32, 0);

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
