#include "item.h"
#include "mainwindow.h"
#include "recent_items_store.h"
#include "search_result_view.h"
#include "test_harness.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QMetaObject>
#include <QThread>

#include <filesystem>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

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
  ASSERT_TRUE((window.windowFlags() & Qt::FramelessWindowHint) ||
              window.property("nativeBackdrop").toBool());
  ASSERT_TRUE(window.windowFlags() & Qt::WindowStaysOnTopHint);
}

TEST(MainWindowTest, SearchProportionsAndNativeFrameStayLightweight) {
  MainWindow window(nullptr, false);
  auto *input = window.findChild<QLineEdit *>("searchInput");
  ASSERT_EQ(44, input->height());
  ASSERT_EQ(nullptr, window.findChild<QWidget *>("panel")->graphicsEffect());
#ifdef Q_OS_WIN
  if (window.property("nativeBackdrop").toBool()) {
    window.show();
    QCoreApplication::processEvents();
    ASSERT_TRUE(!(
        GetWindowLongPtr(reinterpret_cast<HWND>(window.winId()), GWL_EXSTYLE) &
        WS_EX_LAYERED));
    window.hide();
  }
#endif
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

TEST(MainWindowTest, FailedSnippetActivationIsNotRecordedInRecentCache) {
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

  ASSERT_TRUE(
      QMetaObject::invokeMethod(input, "returnPressed", Qt::DirectConnection));
  QCoreApplication::processEvents();

  RecentItemsStore store(DefaultRecentItemsPath());
  const auto recent = store.load();
  ASSERT_EQ(std::size_t(0), recent.size());

  ResetRecentCache();
}

#ifdef Q_OS_WIN
TEST(MainWindowTest, ConfiguredPluginItemFiltersClipboardAndRecordsSuccess) {
  ResetRecentCache();
  MainWindow window(nullptr, false);
  StringItem item;
  item.name = "My custom filter";
  item.type = "plugin";
  item.keywords = {"custom-alias"};
  item.payload = PluginPayload{"clipboard-filter", "filter"};
  window.setItems({item});
  auto *input = window.findChild<QLineEdit *>("searchInput");
  auto *list = window.findChild<QListView *>("resultsList");
  input->setText("custom-alias");
  ASSERT_EQ(1, list->model()->rowCount());
  ASSERT_EQ(QString("plugin"), list->model()
                                   ->index(0, 0)
                                   .data(SearchResultListModel::TypeRole)
                                   .toString());
  auto *clipboard = QGuiApplication::clipboard();
  const auto previous = clipboard->text();
  clipboard->setText("A copied line\ncontinues here.");
  QMetaObject::invokeMethod(input, "returnPressed", Qt::DirectConnection);
  QElapsedTimer timer;
  timer.start();
  while (RecentItemsStore(DefaultRecentItemsPath()).load().empty() &&
         timer.elapsed() < 1000) {
    QCoreApplication::processEvents();
    QThread::msleep(10);
  }
  const auto actual = clipboard->text();
  const auto recent = RecentItemsStore(DefaultRecentItemsPath()).load();
  clipboard->setText(previous);
  ResetRecentCache();
  ASSERT_EQ(QString("A copied line continues here."), actual);
  ASSERT_EQ(std::size_t(1), recent.size());
  ASSERT_EQ(QString("My custom filter"), recent[0].name);
  ASSERT_TRUE(!window.isVisible());
}
#endif

TEST(MainWindowTest, UnknownPluginFunctionDoesNotRecordRecentUse) {
  ResetRecentCache();
  MainWindow window(nullptr, false);
  StringItem item;
  item.name = "Unknown function";
  item.type = "plugin";
  item.keywords = {"unknown-alias"};
  item.payload = PluginPayload{"clipboard-filter", "missing"};
  window.setItems({item});
  auto *input = window.findChild<QLineEdit *>("searchInput");
  input->setText("unknown-alias");
  QMetaObject::invokeMethod(input, "returnPressed", Qt::DirectConnection);
  QCoreApplication::processEvents();
  ASSERT_TRUE(RecentItemsStore(DefaultRecentItemsPath()).load().empty());
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
