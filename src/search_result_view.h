#pragma once

#include "item.h"

#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <vector>

class SearchResultListModel final : public QAbstractListModel {
public:
  enum Roles {
    TypeRole = Qt::UserRole + 1,
  };

  explicit SearchResultListModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role) const override;

  void setResults(std::vector<const StringItem *> results);
  const StringItem *itemAt(int row) const;

private:
  std::vector<const StringItem *> results_;
};

class SearchResultItemDelegate final : public QStyledItemDelegate {
public:
  explicit SearchResultItemDelegate(QObject *parent = nullptr);

  void setTheme(bool darkMode);
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &) const override;
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;

private:
  bool darkMode_;
};
