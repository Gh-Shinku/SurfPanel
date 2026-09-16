#include "item.h"
#include "mainwindow.h"
#include "palette_geometry.h"
#include "recent_items_store.h"
#include "search_result_view.h"
#include "test_harness.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMetaObject>
#include <QPropertyAnimation>
#include <QScreen>
#include <QStyle>
#include <QStyleHints>
#include <QSystemTrayIcon>
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
              window.property("nativeFrame").toBool());
  ASSERT_TRUE(window.windowFlags() & Qt::WindowStaysOnTopHint);
}

TEST(MainWindowTest, TrayMenuUsesIndependentClassicDesktopStyle) {
  MainWindow window(nullptr, false);
  auto *menu = window.findChild<QMenu *>("trayMenu");
  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    ASSERT_EQ(nullptr, menu);
    return;
  }
  ASSERT_NE(nullptr, menu);
  ASSERT_EQ(QString("Show Panel"), menu->defaultAction()->text());
  ASSERT_TRUE(menu->actions()[3]->isSeparator());
  ASSERT_TRUE(menu->actions()[4]->isCheckable());
#ifdef Q_OS_WIN
  auto *classicStyle = menu->findChild<QStyle *>();
  ASSERT_NE(nullptr, classicStyle);
  ASSERT_EQ(QString("windows"), classicStyle->objectName().toLower());
  ASSERT_EQ(classicStyle->standardPalette().color(QPalette::Window),
            menu->palette().color(QPalette::Window));
#endif
  const QString directory = qEnvironmentVariable("SURFPANEL_UI_CAPTURE_DIR");
  if (!directory.isEmpty()) {
    QDir().mkpath(directory);
    menu->popup(QGuiApplication::primaryScreen()->availableGeometry().center());
    QCoreApplication::processEvents();
    const bool saved = menu->grab().save(directory + "/tray-classic.png");
    menu->hide();
    ASSERT_TRUE(saved);
  }
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

TEST(MainWindowTest, ResultCountControlsHeightAndEmptyState) {
  ResetRecentCache();
  MainWindow window(nullptr, false);
  window.setItems(MakeRankedItems(1));
  auto *input = window.findChild<QLineEdit *>("searchInput");
  auto *label = window.findChild<QLabel *>("emptyState");
  ASSERT_EQ(128, window.height());
  ASSERT_EQ(QString("Type to search actions"), label->text());
  input->setText("missing");
  ASSERT_EQ(128, window.height());
  ASSERT_EQ(QString("No results"), label->text());
  input->setText("git");
  ASSERT_EQ(128, window.height());
  window.setItems(MakeRankedItems(6));
  ASSERT_EQ(348, window.height());
  ASSERT_EQ(660, window.width());
  window.setItems(MakeRankedItems(2));
  ASSERT_EQ(172, window.height());
}

TEST(MainWindowTest, VisibleSearchTransitionsShrinkEmptyState) {
  for (bool native : {true, false}) {
    ResetRecentCache();
    MainWindow window(nullptr, false, native);
    window.setItems(MakeRankedItems(6));
    auto *input = window.findChild<QLineEdit *>("searchInput");
    auto *label = window.findChild<QLabel *>("emptyState");
    window.show();
    for (const QString query :
         {QString("git"), QString("missing"), QString("git"), QString("")}) {
      input->setText(query);
      QCoreApplication::processEvents();
      ASSERT_EQ(query == "git" ? 348 : 128, window.height());
      ASSERT_EQ(12, input->mapTo(&window, QPoint()).y());
      if (query != "git") {
        ASSERT_EQ(44, label->height());
        ASSERT_TRUE(label->isVisible());
      }
    }
    window.hide();
    input->setText("git");
    QCoreApplication::processEvents();
    input->clear();
    window.show();
    QCoreApplication::processEvents();
    ASSERT_EQ(128, window.height());
  }
}

TEST(MainWindowTest, PaletteGeometryHandlesNegativeAndSmallScreens) {
  const QRect available(-1920, -200, 1920, 1080);
  const auto one = PaletteGeometry(available, 1);
  const auto six = PaletteGeometry(available, 128);
  ASSERT_EQ(one.y(), six.y());
  ASSERT_TRUE(available.contains(six));
  ASSERT_EQ(660, six.width());
  const QRect small(0, 0, 500, 300);
  ASSERT_TRUE(small.contains(PaletteGeometry(small, 128)));
  ASSERT_EQ(260, PaletteGeometry(small, 128).height());
}

TEST(MainWindowTest, ForcedFallbackAndTypeLabelsAreAvailable) {
  MainWindow window(nullptr, false, false);
  ASSERT_TRUE(!window.property("nativeBackdrop").toBool());
  ASSERT_TRUE(window.windowFlags() & Qt::FramelessWindowHint);
  SearchResultListModel model;
  auto items = MakeRankedItems(1);
  model.setResults({&items[0]});
  ASSERT_EQ(
      QString("Link"),
      model.index(0, 0).data(SearchResultListModel::TypeLabelRole).toString());
  items[0].type = "plugin";
  ASSERT_EQ(
      QString("Plugin"),
      model.index(0, 0).data(SearchResultListModel::TypeLabelRole).toString());
  items[0].type = "snippet";
  ASSERT_EQ(
      QString("Snippet"),
      model.index(0, 0).data(SearchResultListModel::TypeLabelRole).toString());
}

TEST(MainWindowTest, OptionalVisualCapture) {
  const QString directory = qEnvironmentVariable("SURFPANEL_UI_CAPTURE_DIR");
  if (directory.isEmpty()) {
    return;
  }
  QDir().mkpath(directory);
  for (bool dark : {false, true}) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QGuiApplication::styleHints()->setColorScheme(
        dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
#endif
    for (bool native : {true, false}) {
      ResetRecentCache();
      MainWindow window(nullptr, false, native);
      auto items = MakeRankedItems(6);
      items[0].name = "Git Tool 0 with a long example action name that should "
                      "be elided without "
                      "overlapping its type label";
      items[1].type = "plugin";
      items[1].payload = PluginPayload{"clipboard-filter", "filter"};
      items[2] = MakeSnippetItem("Git Tool 2 snippet", "Example");
      items[2].keywords = {"git"};
      window.setItems(items);
      auto *input = window.findChild<QLineEdit *>("searchInput");
      for (const QString query :
           {QString("git"), QString("missing"), QString("git"), QString(""),
            QString("Git Tool 1")}) {
        input->setText(query);
        for (auto *action : window.findChildren<QAction *>()) {
          if (action->text() == "Show Panel") {
            action->trigger();
            break;
          }
        }
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 150) {
          QCoreApplication::processEvents();
          QThread::msleep(10);
        }
        auto *screen = window.screen();
        const auto rect = window.geometry();
        const QString name = QString(dark ? "dark-" : "light-") +
                             (native ? "native-" : "fallback-") +
                             (query.isEmpty() ? "home" : query) + ".png";
        const bool saved =
            screen
                ->grabWindow(0, rect.x(), rect.y(), rect.width(), rect.height())
                .save(directory + "/" + name);
        ASSERT_TRUE(saved);
      }
    }
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
  QGuiApplication::styleHints()->unsetColorScheme();
#endif
}

TEST(MainWindowTest, ThemeAndAnimationChangesPreserveWindowIdentity) {
  ResetRecentCache();
  MainWindow window(nullptr, false);
  const WId original = window.winId();
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
  for (const auto scheme : {Qt::ColorScheme::Dark, Qt::ColorScheme::Light}) {
    QGuiApplication::styleHints()->setColorScheme(scheme);
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 50) {
      QCoreApplication::processEvents();
      QThread::msleep(5);
    }
    ASSERT_EQ(scheme == Qt::ColorScheme::Dark,
              window.property("darkMode").toBool());
    ASSERT_EQ(original, window.winId());
  }
  QGuiApplication::styleHints()->unsetColorScheme();
#endif
  window.setItems(MakeRankedItems(8));
  for (auto *action : window.findChildren<QAction *>()) {
    if (action->text() == "Show Panel") {
      action->trigger();
      break;
    }
  }
  auto *animation = window.findChild<QPropertyAnimation *>("showAnimation");
  ASSERT_EQ(90, animation->duration());
  auto *input = window.findChild<QLineEdit *>("searchInput");
  input->setText("git");
  ASSERT_EQ(QAbstractAnimation::Stopped, animation->state());
  ASSERT_EQ(original, window.winId());
  ASSERT_EQ(qreal(1), window.windowOpacity());
  window.hide();
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
