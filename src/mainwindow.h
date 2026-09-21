#ifndef mainwindow_h
#define mainwindow_h

#include "action_manager.h"
#include "config.h"
#include "item.h"
#include "plugin/plugin_manager.h"
#include "recent_items_store.h"
#include "search_engine.h"
#include <QColor>
#include <QMainWindow>
#include <cstddef>
#include <vector>

class QByteArray;
class QEvent;
class QAction;
class QPropertyAnimation;
class QTimer;
class QLineEdit;
class QListView;
class QLabel;
class QStackedWidget;
class QMenu;
class ConfigWatcher;
class QModelIndex;
class QShortcut;
class QSystemTrayIcon;
class FluentPanel;
class SearchResultItemDelegate;
class SearchResultListModel;
struct ConfigLoadResult;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr, bool enableHotkey = true,
                      bool preferNativeBackdrop = true);
  ~MainWindow() override;

  void setItems(const std::vector<StringItem> &items);

protected:
  bool event(QEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

#ifdef Q_OS_WIN
  bool nativeEvent(const QByteArray &eventType, void *message,
                   qintptr *result) override;
#endif

private:
  void setupWindow();
  void setupUi();
  void applyStylesheet();
  void setupTrayIcon();
  void applyTrayMenuTheme();
  void setupConnections();
  void setupHotkeyPlaceholder(bool enableHotkey);
  ConfigLoadResult loadBackendItems();
  void showPanel();
  void hidePanel(bool clearPasteTarget = true);
  void updateTheme();
  void updatePanelBackground();
  bool isSystemDarkMode() const;
  QColor querySystemAccentColor() const;
  void openConfigDirectory();
  void reloadConfig();
  bool isAutoStartEnabled() const;
  void setAutoStartEnabled(bool enabled);
  void syncAutoStartAction();
  void centerOnScreen();
  void updatePaletteGeometry();
  void toggleVisibilityFromHotkey();
  void moveResultSelection(int delta);
  void onQueryTextChanged(const QString &text);
  std::vector<const StringItem *> recentResultItems() const;
  void activateCurrentResult();
  void activateIndex(const QModelIndex &index);
  void invokeItemAction(const StringItem *item);

  QLineEdit *input_;
  QListView *resultsView_;
  QLabel *emptyState_ = nullptr;
  QStackedWidget *resultsSurface_ = nullptr;
  SearchResultListModel *resultsModel_;
  SearchResultItemDelegate *resultsDelegate_;
  FluentPanel *panel_;
  bool nativeFrame_ = false;
  bool nativeBackdrop_ = false;
  QRect activeScreenGeometry_;
  QPropertyAnimation *showAnimation_ = nullptr;
  QTimer *themeRefreshTimer_ = nullptr;
  ConfigWatcher *configWatcher_ = nullptr;
  QPoint showTarget_;
  QColor accentColor_;
  bool isDarkMode_;
  bool hidingPanel_ = false;
  ThemeMode themeMode_ = ThemeMode::System;

  QSystemTrayIcon *trayIcon_;
  QMenu *trayMenu_;
  QAction *showPanelAction_;
  QAction *showConfigDirAction_;
  QAction *autoStartAction_;
  QAction *exitAction_;

  SearchEngine searchEngine_;
  ActionManager actionManager_;
  DefaultActionContext actionContext_;
  PluginManager pluginManager_;
  RecentItemsStore recentItemsStore_;
  std::vector<StringItem> items_;

  static constexpr std::size_t kTopK = 6;
  static constexpr std::size_t kPrefixModeMaxResults = 128;
  bool globalHotkeyRegistered_;
  int hotkeyId_;
  QShortcut *fallbackShortcut_;
};

#endif
