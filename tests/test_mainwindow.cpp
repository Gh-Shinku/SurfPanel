#include "item.h"
#include "mainwindow.h"
#include "test_harness.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QCoreApplication>
#include <QLineEdit>
#include <QListView>

#include <vector>

namespace {

std::vector<StringItem> MakeRankedItems(int count) {
  std::vector<StringItem> items;
  items.reserve(static_cast<std::size_t>(count));

  for (int i = 0; i < count; ++i) {
    StringItem item;
    item.name = QString("Git Tool %1").arg(i);
    item.type = "url";
    item.keywords = {"git", "tool"};
    item.payload = UrlPayload{QString("https://example.com/%1").arg(i)};
    items.push_back(item);
  }

  return items;
}

} // namespace

TEST(MainWindowTest, StartsHiddenFramelessAndOnTop) {
  MainWindow window(nullptr, false);

  ASSERT_TRUE(!window.isVisible());
  ASSERT_TRUE(window.windowFlags() & Qt::FramelessWindowHint);
  ASSERT_TRUE(window.windowFlags() & Qt::WindowStaysOnTopHint);
}

TEST(MainWindowTest, TextChangedQueriesSearchAndAppliesTopK) {
  MainWindow window(nullptr, false);
  window.setItems(MakeRankedItems(8));

  QLineEdit *input = window.findChild<QLineEdit *>("searchInput");
  QListView *list = window.findChild<QListView *>("resultsList");

  ASSERT_NE(nullptr, input);
  ASSERT_NE(nullptr, list);

  input->setText("git");
  QCoreApplication::processEvents();

  ASSERT_EQ(6, list->model()->rowCount());
  ASSERT_EQ(QString("Git Tool 0"),
            list->model()->index(0, 0).data(Qt::DisplayRole).toString());

  input->clear();
  QCoreApplication::processEvents();
  ASSERT_EQ(0, list->model()->rowCount());
}

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  return RUN_ALL_TESTS();
}