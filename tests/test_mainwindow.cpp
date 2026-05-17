#include "item.h"
#include "mainwindow.h"
#include "recent_items_store.h"
#include "test_harness.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QMetaObject>

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace {

void ResetRecentCache() { fs::remove(DefaultRecentItemsPath()); }

void WriteRecentCache(const std::vector<RecentItemKey> &items) {
  RecentItemsStore store(DefaultRecentItemsPath());
  ASSERT_TRUE(store.save(items));
}

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

StringItem MakeSnippetItem(const QString &name, const QString &snippet) {
  StringItem item;
  item.name = name;
  item.type = "snippet";
  item.keywords = {name.toLower()};
  item.payload = SnippetPayload{snippet};
  return item;
}

} // namespace

TEST(MainWindowTest, StartsHiddenFramelessAndOnTop) {
  ResetRecentCache();
  MainWindow window(nullptr, false);

  ASSERT_TRUE(!window.isVisible());
  ASSERT_TRUE(window.windowFlags() & Qt::FramelessWindowHint);
  ASSERT_TRUE(window.windowFlags() & Qt::WindowStaysOnTopHint);
}

TEST(MainWindowTest, TextChangedQueriesSearchAndAppliesTopK) {
  ResetRecentCache();
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

TEST(MainWindowTest, PrefixQueryShowsScrollableResultWindow) {
  ResetRecentCache();
  MainWindow window(nullptr, false);
  window.setItems(MakeRankedItems(8));

  QLineEdit *input = window.findChild<QLineEdit *>("searchInput");
  QListView *list = window.findChild<QListView *>("resultsList");

  ASSERT_NE(nullptr, input);
  ASSERT_NE(nullptr, list);
  ASSERT_EQ(Qt::ScrollBarAsNeeded, list->verticalScrollBarPolicy());

  input->setText("u git");
  QCoreApplication::processEvents();

  ASSERT_EQ(8, list->model()->rowCount());
}

TEST(MainWindowTest, PrefixQueryIsCappedAtWindowLimit) {
  ResetRecentCache();
  MainWindow window(nullptr, false);
  window.setItems(MakeRankedItems(140));

  QLineEdit *input = window.findChild<QLineEdit *>("searchInput");
  QListView *list = window.findChild<QListView *>("resultsList");

  ASSERT_NE(nullptr, input);
  ASSERT_NE(nullptr, list);

  input->setText("u git");
  QCoreApplication::processEvents();

  ASSERT_EQ(128, list->model()->rowCount());
}

TEST(MainWindowTest, EmptyQueryShowsRecentItemsFromCache) {
  ResetRecentCache();
  WriteRecentCache({RecentItemKey{QString("url"), QString("Git Tool 3")},
                    RecentItemKey{QString("url"), QString("Git Tool 1")}});

  MainWindow window(nullptr, false);
  window.setItems(MakeRankedItems(8));

  QLineEdit *input = window.findChild<QLineEdit *>("searchInput");
  QListView *list = window.findChild<QListView *>("resultsList");
  ASSERT_NE(nullptr, input);
  ASSERT_NE(nullptr, list);

  input->clear();
  QCoreApplication::processEvents();

  ASSERT_EQ(2, list->model()->rowCount());
  ASSERT_EQ(QString("Git Tool 3"),
            list->model()->index(0, 0).data(Qt::DisplayRole).toString());
  ASSERT_EQ(QString("Git Tool 1"),
            list->model()->index(1, 0).data(Qt::DisplayRole).toString());

  ResetRecentCache();
}

TEST(MainWindowTest, EmptyQuerySkipsStaleRecentItems) {
  ResetRecentCache();
  WriteRecentCache({RecentItemKey{QString("url"), QString("Missing")},
                    RecentItemKey{QString("url"), QString("Git Tool 2")}});

  MainWindow window(nullptr, false);
  window.setItems(MakeRankedItems(4));

  QLineEdit *input = window.findChild<QLineEdit *>("searchInput");
  QListView *list = window.findChild<QListView *>("resultsList");
  ASSERT_NE(nullptr, input);
  ASSERT_NE(nullptr, list);

  input->clear();
  QCoreApplication::processEvents();

  ASSERT_EQ(1, list->model()->rowCount());
  ASSERT_EQ(QString("Git Tool 2"),
            list->model()->index(0, 0).data(Qt::DisplayRole).toString());

  ResetRecentCache();
}

TEST(MainWindowTest, ShowPanelRefreshesEmptyQueryHomepage) {
  ResetRecentCache();

  MainWindow window(nullptr, false);
  window.setItems(MakeRankedItems(4));

  QLineEdit *input = window.findChild<QLineEdit *>("searchInput");
  QListView *list = window.findChild<QListView *>("resultsList");
  ASSERT_NE(nullptr, input);
  ASSERT_NE(nullptr, list);
  ASSERT_EQ(0, list->model()->rowCount());

  WriteRecentCache({RecentItemKey{QString("url"), QString("Git Tool 2")}});

  QAction *showAction = nullptr;
  for (QAction *action : window.findChildren<QAction *>()) {
    if (action->text() == QString("Show Panel")) {
      showAction = action;
      break;
    }
  }
  ASSERT_NE(nullptr, showAction);

  showAction->trigger();
  QCoreApplication::processEvents();

  ASSERT_EQ(1, list->model()->rowCount());
  ASSERT_EQ(QString("Git Tool 2"),
            list->model()->index(0, 0).data(Qt::DisplayRole).toString());

  ResetRecentCache();
}

TEST(MainWindowTest, SnippetActivationIsRecordedInRecentCache) {
  ResetRecentCache();

  MainWindow window(nullptr, false);
  window.setItems({MakeSnippetItem("Today", "{{date}}")});

  QLineEdit *input = window.findChild<QLineEdit *>("searchInput");
  QListView *list = window.findChild<QListView *>("resultsList");
  ASSERT_NE(nullptr, input);
  ASSERT_NE(nullptr, list);

  input->setText("today");
  QCoreApplication::processEvents();
  ASSERT_EQ(1, list->model()->rowCount());

  ASSERT_TRUE(QMetaObject::invokeMethod(input, "returnPressed",
                                        Qt::DirectConnection));
  QCoreApplication::processEvents();

  RecentItemsStore store(DefaultRecentItemsPath());
  const auto recent = store.load();
  ASSERT_EQ(std::size_t(1), recent.size());
  ASSERT_EQ(QString("snippet"), recent[0].type);
  ASSERT_EQ(QString("Today"), recent[0].name);

  ResetRecentCache();
}

TEST(MainWindowTest, EscapeShortcutHidesPanel) {
  ResetRecentCache();
  MainWindow window(nullptr, false);
  window.show();

  QLineEdit *input = window.findChild<QLineEdit *>("searchInput");
  ASSERT_NE(nullptr, input);

  input->setFocus();
  QCoreApplication::processEvents();
  ASSERT_TRUE(window.isVisible());

  QKeyEvent keyPress(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QCoreApplication::sendEvent(input, &keyPress);
  QKeyEvent keyRelease(QEvent::KeyRelease, Qt::Key_Escape, Qt::NoModifier);
  QCoreApplication::sendEvent(input, &keyRelease);
  QCoreApplication::processEvents();

  ASSERT_TRUE(!window.isVisible());
}

TEST(MainWindowTest, ArrowKeysSwitchPresentedItems) {
  ResetRecentCache();
  MainWindow window(nullptr, false);
  window.setItems(MakeRankedItems(8));

  QLineEdit *input = window.findChild<QLineEdit *>("searchInput");
  QListView *list = window.findChild<QListView *>("resultsList");
  ASSERT_NE(nullptr, input);
  ASSERT_NE(nullptr, list);

  input->setText("git");
  input->setFocus();
  QCoreApplication::processEvents();

  ASSERT_EQ(0, list->currentIndex().row());

  QKeyEvent downPress(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
  QCoreApplication::sendEvent(input, &downPress);
  QCoreApplication::processEvents();
  ASSERT_EQ(1, list->currentIndex().row());

  QKeyEvent upPress(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
  QCoreApplication::sendEvent(input, &upPress);
  QCoreApplication::processEvents();
  ASSERT_EQ(0, list->currentIndex().row());
}

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  ResetRecentCache();
  return RUN_ALL_TESTS();
}
