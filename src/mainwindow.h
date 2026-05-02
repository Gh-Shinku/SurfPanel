#ifndef mainwindow_h
#define mainwindow_h

#include "action_manager.h"
#include "item.h"
#include "search_engine.h"
#include <QMainWindow>
#include <cstddef>
#include <vector>

class QByteArray;
class QEvent;
class QAction;
class QLineEdit;
class QListView;
class QMenu;
class QModelIndex;
class QShortcut;
class QSystemTrayIcon;
class SearchResultItemDelegate;
class SearchResultListModel;
struct ConfigLoadResult;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr, bool enableHotkey = true);
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
  void setupConnections();
  void setupHotkeyPlaceholder(bool enableHotkey);
  ConfigLoadResult loadBackendItems();
  void showPanel();
  void openConfigDirectory();
  void reloadConfig();
  void centerOnScreen();
  void toggleVisibilityFromHotkey();
  void moveResultSelection(int delta);
  void onQueryTextChanged(const QString &text);
  void activateCurrentResult();
  void activateIndex(const QModelIndex &index);
  void invokeItemAction(const StringItem *item);

  QLineEdit *input_;
  QListView *resultsView_;
  SearchResultListModel *resultsModel_;
  SearchResultItemDelegate *resultsDelegate_;

  QSystemTrayIcon *trayIcon_;
  QMenu *trayMenu_;
  QAction *showPanelAction_;
  QAction *showConfigDirAction_;
  QAction *reloadConfigAction_;
  QAction *exitAction_;

  SearchEngine searchEngine_;
  ActionManager actionManager_;

  static constexpr std::size_t kTopK = 6;
  bool globalHotkeyRegistered_;
  int hotkeyId_;
  QShortcut *fallbackShortcut_;
};

#endif
