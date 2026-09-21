#include "ui/window/mainwindow.h"

#include "core/config/config.h"
#include "core/config/config_watcher.h"
#include "platform/window_effects.h"
#include "plugins/host/builtin_plugins.h"
#include "ui/palette/fluent_panel.h"
#include "ui/palette/palette_geometry.h"
#include "ui/palette/palette_search_input.h"
#include "ui/palette/palette_theme.h"
#include "ui/palette/search_result_view.h"
#include "ui/tray/tray_menu.h"
#include "ui/window/about_dialog.h"
#include <QCursor>
#include <QLabel>
#include <QStackedWidget>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPointer>
#include <QProcess>
#include <QPropertyAnimation>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStyleHints>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <utility>
#include <variant>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {
QColor AccentForegroundColor(const QColor &accent);
}

namespace {

QString ToRgbaString(const QColor &color) {
  return QString("rgba(%1, %2, %3, %4)")
      .arg(color.red())
      .arg(color.green())
      .arg(color.blue())
      .arg(color.alpha());
}

QString ToHexString(const QColor &color) {
  return QString("#%1%2%3")
      .arg(color.red(), 2, 16, QLatin1Char('0'))
      .arg(color.green(), 2, 16, QLatin1Char('0'))
      .arg(color.blue(), 2, 16, QLatin1Char('0'))
      .toUpper();
}

QColor AccentForegroundColor(const QColor &accent) {
  return PaletteContrast(Qt::black, accent) > PaletteContrast(Qt::white, accent)
             ? QColor(Qt::black)
             : QColor(Qt::white);
}

std::optional<QString> FindStylesheetPath() {
  const QString appDir = QCoreApplication::applicationDirPath();
  const std::vector<QString> candidates = {
      "src/ui/palette/mainwindow_fluent.qss",
      "../src/ui/palette/mainwindow_fluent.qss",
      "../../src/ui/palette/mainwindow_fluent.qss",
      appDir + "/../src/ui/palette/mainwindow_fluent.qss",
      appDir + "/../../src/ui/palette/mainwindow_fluent.qss",
  };

  for (const auto &candidate : candidates) {
    if (QFile::exists(candidate)) {
      return candidate;
    }
  }

  return std::nullopt;
}

#ifdef Q_OS_WIN
QString StartupValueName() {
  const QString appName = QCoreApplication::applicationName();
  return appName.isEmpty() ? QString("SurfPanel") : appName;
}

QSettings StartupSettings() {
  return QSettings(
      "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
      QSettings::NativeFormat);
}

QString StartupValueData() {
  const QString exePath =
      QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
  return QString("\"%1\"").arg(exePath);
}

QString NormalizeStartupValue(const QString &value) {
  QString trimmed = value.trimmed();
  if (trimmed.startsWith('"') && trimmed.endsWith('"') && trimmed.size() >= 2) {
    trimmed = trimmed.mid(1, trimmed.size() - 2);
  }
  return QDir::toNativeSeparators(trimmed);
}
#endif

} // namespace

MainWindow::MainWindow(QWidget *parent, bool enableHotkey,
                       bool preferNativeBackdrop)
    : QMainWindow(parent), input_(nullptr), resultsView_(nullptr),
      resultsModel_(nullptr), resultsDelegate_(nullptr), panel_(nullptr),
      accentColor_(QColor("#005FB8")), isDarkMode_(false), trayIcon_(nullptr),
      trayMenu_(nullptr), showPanelAction_(nullptr),
      showConfigDirAction_(nullptr), autoStartAction_(nullptr),
      aboutAction_(nullptr), exitAction_(nullptr),
      globalHotkeyRegistered_(false), hotkeyId_(1), fallbackShortcut_(nullptr) {
  nativeFrame_ = preferNativeBackdrop && SupportsNativeBackdrop();
  RegisterDefaultActions(&actionManager_);
  if (!RegisterBuiltinPlugins(&pluginManager_)) {
    qCritical() << "Failed to register built-in plugins.";
  }

  setupWindow();
  setupUi();
  updateTheme();
  setupConnections();
  setupTrayIcon();
  loadBackendItems();
  setupHotkeyPlaceholder(enableHotkey);

  hide();
}

MainWindow::~MainWindow() {
  pluginManager_.shutdown();
#ifdef Q_OS_WIN
  if (globalHotkeyRegistered_) {
    UnregisterHotKey(reinterpret_cast<HWND>(winId()), hotkeyId_);
  }
#endif
}

void MainWindow::setItems(const std::vector<StringItem> &items) {
  items_ = items;
  searchEngine_.setItems(items);
  searchEngine_.setSearchPrefixes(DefaultSearchPrefixes());
  onQueryTextChanged(input_->text());
}

bool MainWindow::event(QEvent *event) {
  if (themeRefreshTimer_ &&
      (event->type() == QEvent::ApplicationPaletteChange ||
       event->type() == QEvent::PaletteChange)) {
    themeRefreshTimer_->start(0);
  }
  if (event->type() == QEvent::WindowDeactivate && isVisible()) {
    hidePanel();
  }
  return QMainWindow::event(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if ((watched == input_ || watched == resultsView_) &&
      event->type() == QEvent::KeyPress) {
    auto *keyEvent = static_cast<QKeyEvent *>(event);

    if (keyEvent->key() == Qt::Key_Down) {
      moveResultSelection(1);
      return true;
    }

    if (keyEvent->key() == Qt::Key_Up) {
      moveResultSelection(-1);
      return true;
    }

    if (keyEvent->key() == Qt::Key_Escape && isVisible()) {
      hidePanel();
      return true;
    }
  }

  return QMainWindow::eventFilter(watched, event);
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message,
                             qintptr *result) {
  MSG *msg = static_cast<MSG *>(message);
  if (msg && themeRefreshTimer_ &&
      (msg->message == WM_DWMCOLORIZATIONCOLORCHANGED ||
       msg->message == WM_SETTINGCHANGE || msg->message == WM_THEMECHANGED ||
       msg->message == WM_DWMCOMPOSITIONCHANGED)) {
    themeRefreshTimer_->start(0);
  }
  if (nativeFrame_ && msg && msg->message == WM_NCCALCSIZE && msg->wParam) {
    if (result) {
      *result = 0;
    }
    return true;
  }
  if (nativeFrame_ && msg && msg->message == WM_NCHITTEST) {
    if (result) {
      *result = HTCLIENT;
    }
    return true;
  }

  if (msg != nullptr && msg->message == WM_HOTKEY &&
      static_cast<int>(msg->wParam) == hotkeyId_) {
    toggleVisibilityFromHotkey();
    if (result != nullptr) {
      *result = 0;
    }
    return true;
  }

  return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::setupWindow() {
  const QIcon appIcon(":/icons/SurfPanel.ico");
  if (!appIcon.isNull()) {
    setWindowIcon(appIcon);
  }

  setWindowTitle("SurfPanel");
  setWindowFlags(Qt::WindowStaysOnTopHint | Qt::Tool |
                 (nativeFrame_ ? (Qt::CustomizeWindowHint | Qt::WindowTitleHint)
                               : Qt::FramelessWindowHint));
  setAttribute(Qt::WA_TranslucentBackground, true);
  resize(660, 348);
#ifdef Q_OS_WIN
  if (nativeFrame_) {
    HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowLongPtr(hwnd, GWL_STYLE,
                     GetWindowLongPtr(hwnd, GWL_STYLE) | WS_CAPTION |
                         WS_THICKFRAME);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    nativeBackdrop_ = ApplyNativeBackdrop(winId(), isSystemDarkMode());
  }
#endif
  setProperty("nativeBackdrop", nativeBackdrop_);
  setProperty("nativeFrame", nativeFrame_);
}

void MainWindow::setupUi() {
  QWidget *root = new QWidget(this);
  QVBoxLayout *rootLayout = new QVBoxLayout(root);
  rootLayout->setContentsMargins(0, 0, 0, 0);

  panel_ = new FluentPanel(root);
  panel_->setObjectName("panel");
  panel_->setFrameShape(QFrame::NoFrame);
  panel_->setCornerRadius(8);

  QVBoxLayout *panelLayout = new QVBoxLayout(panel_);
  panelLayout->setContentsMargins(12, 12, 12, 20);
  panelLayout->setSpacing(8);

  input_ = new PaletteSearchInput(panel_);
  input_->setObjectName("searchInput");
  input_->setPlaceholderText("Search actions…");
  input_->setClearButtonEnabled(true);

  resultsView_ = new QListView(panel_);
  resultsView_->setObjectName("resultsList");
  resultsView_->setFrameShape(QFrame::NoFrame);
  resultsView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  resultsView_->setSelectionMode(QAbstractItemView::SingleSelection);
  resultsView_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  resultsView_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  resultsView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  resultsView_->setUniformItemSizes(true);

  resultsModel_ = new SearchResultListModel(this);
  resultsDelegate_ = new SearchResultItemDelegate(this);
  resultsView_->setModel(resultsModel_);
  resultsView_->setItemDelegate(resultsDelegate_);

  panelLayout->addWidget(input_);
  resultsSurface_ = new QStackedWidget(panel_);
  resultsSurface_->addWidget(resultsView_);
  emptyState_ = new QLabel("Type to search actions", resultsSurface_);
  emptyState_->setObjectName("emptyState");
  emptyState_->setAlignment(Qt::AlignCenter);
  resultsSurface_->addWidget(emptyState_);
  panelLayout->addWidget(resultsSurface_, 1);
  connect(resultsModel_, &QAbstractItemModel::modelReset, this,
          &MainWindow::updatePaletteGeometry);

  rootLayout->addWidget(panel_);
  setCentralWidget(root);
}

void MainWindow::applyStylesheet() {
  QString styleSource;
  QFile embeddedStyleFile(":/styles/mainwindow_fluent.qss");
  if (embeddedStyleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    styleSource = QString::fromUtf8(embeddedStyleFile.readAll());
  } else {
    const auto stylePath = FindStylesheetPath();
    if (!stylePath.has_value()) {
      qWarning() << "Fluent stylesheet not found; using default style.";
      return;
    }

    QFile styleFile(*stylePath);
    if (!styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qWarning() << "Failed to open stylesheet:" << *stylePath;
      return;
    }
    styleSource = QString::fromUtf8(styleFile.readAll());
  }

  const QColor accent =
      accentColor_.isValid() ? accentColor_ : QColor("#005FB8");
  const auto colors = ColorsForPalette(isDarkMode_);
  const QColor selectionText = AccentForegroundColor(accent);
  QColor selectionBg = accent;
  selectionBg.setAlpha(255);

  const QColor scrollbarHandle =
      isDarkMode_ ? QColor(148, 163, 184, 120) : QColor(104, 104, 104, 96);

  QString themed = styleSource;
  themed.replace("@text_color", ToHexString(colors.text));
  themed.replace("@placeholder_color", ToHexString(colors.secondary));
  themed.replace("@input_border", ToRgbaString(colors.inputBorder));
  themed.replace("@input_surface", ToRgbaString(colors.inputSurface));
  themed.replace("@selection_bg", ToRgbaString(selectionBg));
  themed.replace("@selection_text", ToHexString(selectionText));
  themed.replace("@scrollbar_handle", ToRgbaString(scrollbarHandle));

  setStyleSheet(themed);
}

void MainWindow::updateTheme() {
  const bool darkMode = ResolveDarkTheme(themeMode_, isSystemDarkMode());
  const QColor accent =
      ReadablePaletteAccent(querySystemAccentColor(), darkMode);
  const bool themeChanged =
      (darkMode != isDarkMode_) || (accent != accentColor_ && accent.isValid());
  const bool needsApply = themeChanged || styleSheet().isEmpty();

  isDarkMode_ = darkMode;
  setProperty("darkMode", isDarkMode_);
  if (accent.isValid()) {
    accentColor_ = accent;
  }

  if (needsApply) {
    qInfo() << "Application theme: configured="
            << (themeMode_ == ThemeMode::Dark    ? "dark"
                : themeMode_ == ThemeMode::Light ? "light"
                                                 : "system")
            << "resolved=" << (isDarkMode_ ? "dark" : "light");
    applyStylesheet();
    applyTrayMenuTheme();
    if (aboutDialog_ != nullptr) {
      aboutDialog_->setDarkMode(isDarkMode_);
    }
    static_cast<PaletteSearchInput *>(input_)->setAccentColor(accentColor_);
    const auto colors = ColorsForPalette(isDarkMode_);
    static_cast<PaletteSearchInput *>(input_)->setThemeColors(colors.text,
                                                              colors.secondary);

    if (resultsDelegate_ != nullptr) {
      resultsDelegate_->setTheme(isDarkMode_, accentColor_);
    }

    if (resultsView_ != nullptr) {
      resultsView_->viewport()->update();
    }
  }

  updatePanelBackground();
}

void MainWindow::updatePanelBackground() {
  if (!panel_) {
    return;
  }
  if (nativeFrame_) {
    nativeBackdrop_ = ApplyNativeBackdrop(winId(), isDarkMode_);
  }
  setProperty("nativeBackdrop", nativeBackdrop_);
  if (nativeBackdrop_) {
    panel_->setThemeColors(ColorsForPalette(isDarkMode_).nativeTint,
                           Qt::transparent, isDarkMode_);
  } else {
    QColor base = isDarkMode_ ? QColor("#202020") : QColor("#FAFAFA");
    base.setAlpha(nativeFrame_ ? 255 : (isDarkMode_ ? 242 : 240));
    panel_->setThemeColors(
        base, isDarkMode_ ? QColor(255, 255, 255, 28) : QColor(0, 0, 0, 28),
        isDarkMode_);
  }
}

bool MainWindow::isSystemDarkMode() const {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  const auto scheme = QGuiApplication::styleHints()->colorScheme();
  if (scheme != Qt::ColorScheme::Unknown) {
    return scheme == Qt::ColorScheme::Dark;
  }
#endif
#ifdef Q_OS_WIN
  QSettings settings(
      "HKEY_CURRENT_"
      "USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      QSettings::NativeFormat);
  return settings.value("AppsUseLightTheme", 1).toInt() == 0;
#else
  return palette().color(QPalette::Window).lightness() < 128;
#endif
}

QColor MainWindow::querySystemAccentColor() const {
#ifdef Q_OS_WIN
  QColor fallback("#005FB8");
  HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll");
  if (dwmapi == nullptr) {
    return fallback;
  }

  using DwmGetColorizationColorFn = HRESULT(WINAPI *)(DWORD *, BOOL *);
  auto fn = reinterpret_cast<DwmGetColorizationColorFn>(
      GetProcAddress(dwmapi, "DwmGetColorizationColor"));
  if (fn == nullptr) {
    FreeLibrary(dwmapi);
    return fallback;
  }

  DWORD color = 0;
  BOOL opaque = FALSE;
  const HRESULT result = fn(&color, &opaque);
  FreeLibrary(dwmapi);
  if (FAILED(result)) {
    return fallback;
  }

  return QColor((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
#else
  return QColor("#005FB8");
#endif
}

void MainWindow::applyTrayMenuTheme() {
  if (trayMenu_) {
    static_cast<TrayMenu *>(trayMenu_)->setDarkMode(isDarkMode_);
  }
}

void MainWindow::setupTrayIcon() {
  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    qWarning() << "System tray is unavailable on this platform/session.";
    return;
  }

  trayMenu_ = new TrayMenu(this);
  trayMenu_->setObjectName("trayMenu");
  applyTrayMenuTheme();
  showPanelAction_ = trayMenu_->addAction("Show Panel");
  trayMenu_->setDefaultAction(showPanelAction_);
  showConfigDirAction_ = trayMenu_->addAction("Show Config File Dir");
  trayMenu_->addSeparator();
  autoStartAction_ = trayMenu_->addAction("Start with Windows");
  autoStartAction_->setCheckable(true);
  trayMenu_->addSeparator();
  aboutAction_ = trayMenu_->addAction("About SurfPanel");
  exitAction_ = trayMenu_->addAction("Exit");

  connect(showPanelAction_, &QAction::triggered, this, &MainWindow::showPanel);
  connect(showConfigDirAction_, &QAction::triggered, this,
          &MainWindow::openConfigDirectory);
  connect(autoStartAction_, &QAction::toggled, this,
          &MainWindow::setAutoStartEnabled);
  connect(trayMenu_, &QMenu::aboutToShow, this,
          &MainWindow::syncAutoStartAction);
  connect(aboutAction_, &QAction::triggered, this, [this]() {
    if (aboutDialog_ == nullptr) {
      aboutDialog_ = new AboutDialog(this);
    }
    aboutDialog_->setDarkMode(isDarkMode_);
    aboutDialog_->show();
    aboutDialog_->raise();
    aboutDialog_->activateWindow();
  });
  connect(exitAction_, &QAction::triggered, qApp, &QApplication::quit);

  QIcon trayIcon(":/icons/SurfPanel.ico");
  if (trayIcon.isNull()) {
    trayIcon = windowIcon();
  }

  trayIcon_ = new QSystemTrayIcon(this);
  trayIcon_->setIcon(trayIcon);
  trayIcon_->setToolTip("SurfPanel");
  trayIcon_->setContextMenu(trayMenu_);

  syncAutoStartAction();

  connect(trayIcon_, &QSystemTrayIcon::activated, this,
          [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger ||
                reason == QSystemTrayIcon::DoubleClick) {
              toggleVisibilityFromHotkey();
            }
          });

  trayIcon_->show();
}

void MainWindow::setupConnections() {
  configWatcher_ = new ConfigWatcher(this);
  connect(configWatcher_, &ConfigWatcher::configurationChanged, this,
          &MainWindow::reloadConfig);
  themeRefreshTimer_ = new QTimer(this);
  themeRefreshTimer_->setSingleShot(true);
  connect(themeRefreshTimer_, &QTimer::timeout, this, &MainWindow::updateTheme);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
          [this](Qt::ColorScheme) { themeRefreshTimer_->start(0); });
#endif
  showAnimation_ = new QPropertyAnimation(this, "pos", this);
  showAnimation_->setObjectName("showAnimation");
  showAnimation_->setDuration(90);
  showAnimation_->setEasingCurve(QEasingCurve::OutCubic);
  input_->installEventFilter(this);
  resultsView_->installEventFilter(this);

  connect(input_, &QLineEdit::textChanged, this,
          &MainWindow::onQueryTextChanged);
  connect(input_, &QLineEdit::returnPressed, this,
          &MainWindow::activateCurrentResult);
  connect(resultsView_, &QListView::activated, this,
          &MainWindow::activateIndex);
  connect(resultsView_, &QListView::clicked, this, &MainWindow::activateIndex);

  QShortcut *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  escapeShortcut->setContext(Qt::ApplicationShortcut);
  connect(escapeShortcut, &QShortcut::activated, this, [this]() {
    if (isVisible()) {
      hidePanel();
    }
  });
}

void MainWindow::setupHotkeyPlaceholder(bool enableHotkey) {
  if (!enableHotkey) {
    return;
  }

#ifdef Q_OS_WIN
  // Placeholder implementation: native hotkey registration for Windows only.
  createWinId();
  globalHotkeyRegistered_ =
      RegisterHotKey(reinterpret_cast<HWND>(winId()), hotkeyId_,
                     MOD_ALT | MOD_NOREPEAT, VK_SPACE);
  if (!globalHotkeyRegistered_) {
    qWarning() << "Global hotkey registration failed; using app-local shortcut";
  }
#endif

  if (!globalHotkeyRegistered_) {
    fallbackShortcut_ = new QShortcut(QKeySequence("Alt+Space"), this);
    fallbackShortcut_->setContext(Qt::ApplicationShortcut);
    connect(fallbackShortcut_, &QShortcut::activated, this,
            &MainWindow::toggleVisibilityFromHotkey);
  }
}

ConfigLoadResult MainWindow::loadBackendItems() {
  createWinId();
  PluginHostContext pluginHost;
  pluginHost.eventLoopOwner = this;
  pluginHost.nativeWindow = winId();
  pluginHost.log = [](QtMsgType type, const QString &message) {
    if (type == QtCriticalMsg || type == QtFatalMsg) {
      qCritical().noquote() << message;
    } else {
      qWarning().noquote() << message;
    }
  };
  pluginManager_.initialize(std::move(pluginHost));
  const auto configRoot = FindConfigRoot();
  if (!configRoot.has_value()) {
    items_.clear();
    searchEngine_.setItems({});
    searchEngine_.setSearchPrefixes(DefaultSearchPrefixes());
    ConfigLoadResult result;
    result.searchPrefixes = DefaultSearchPrefixes();
    result.ok = false;
    result.message = "Cannot initialize user configuration directory.";
    qCritical().noquote() << QString::fromStdString(result.message);
    return result;
  }

  if (configWatcher_ && !configWatcher_->isWatching()) {
    const QString root = QString::fromUtf8(configRoot->u8string().c_str());
    configWatcher_->start(root);
  }

  auto result = LoadConfigWithFallback(*configRoot);
  const PluginReloadReport pluginReport = pluginManager_.reload(*configRoot);
  if (!pluginReport.messages.isEmpty()) {
    if (!result.message.empty()) {
      result.message += '\n';
    }
    result.message += pluginReport.messages.join('\n').toStdString();
  }
  items_ = result.items;
  searchEngine_.setItems(result.items);
  searchEngine_.setSearchPrefixes(result.searchPrefixes);
  actionManager_.setVariableSettings(result.variableSettings);
  themeMode_ = result.themeMode;
  updateTheme();
  onQueryTextChanged(input_->text());

  qInfo() << "Configuration loaded:"
          << QString::fromUtf8(configRoot->u8string().c_str())
          << "items=" << result.items.size() << "ok=" << result.ok
          << "fallback=" << result.usedFallback;
  if (!result.message.empty()) {
    qWarning() << "Config load warning:"
               << QString::fromStdString(result.message);
  }

  return result;
}

void MainWindow::showPanel() {
  updateTheme();

#ifdef Q_OS_WIN
  const HWND foregroundWindow = GetForegroundWindow();
  if (foregroundWindow != nullptr &&
      foregroundWindow != reinterpret_cast<HWND>(winId())) {
    actionContext_.setNativePasteTarget(foregroundWindow);
  }
#endif

  if (isVisible()) {
    onQueryTextChanged(input_->text());
    raise();
    activateWindow();
    input_->setFocus();
    input_->selectAll();
    return;
  }

  centerOnScreen();
  onQueryTextChanged(input_->text());
  show();
  raise();
  activateWindow();
  input_->setFocus();
  input_->selectAll();
  if (SystemAnimationsEnabled()) {
    showTarget_ = pos();
    showAnimation_->setStartValue(showTarget_ + QPoint(0, 4));
    showAnimation_->setEndValue(showTarget_);
    showAnimation_->start();
  }
}

void MainWindow::hidePanel(bool clearPasteTarget) {
  if (!isVisible() || hidingPanel_) {
    return;
  }

  // QWidget::hide() can synchronously deliver WindowDeactivate. Prevent that
  // event from re-entering hidePanel() with the default argument and clearing
  // a paste target that an action still needs.
  hidingPanel_ = true;
  hide();
  input_->clear();
  if (showAnimation_ &&
      showAnimation_->state() == QAbstractAnimation::Running) {
    showAnimation_->stop();
    move(showTarget_);
  }

#ifdef Q_OS_WIN
  if (clearPasteTarget) {
    actionContext_.clearNativePasteTarget();
  }
#endif
  hidingPanel_ = false;
}

void MainWindow::reloadConfig() {
  QElapsedTimer timer;
  timer.start();
  qInfo() << "Reloading configuration automatically";
  const auto result = loadBackendItems();
  qInfo() << "Configuration reload finished: ok=" << result.ok
          << "fallback=" << result.usedFallback
          << "items=" << result.items.size()
          << "elapsed ms=" << timer.elapsed();
  if (!result.message.empty()) {
    qWarning().noquote() << "Configuration diagnostics:"
                         << QString::fromStdString(result.message);
  }
  if (trayIcon_ == nullptr || result.message.empty()) {
    return;
  }

  const QString message = QString::fromStdString(result.message);
  const QSystemTrayIcon::MessageIcon icon =
      result.ok ? QSystemTrayIcon::Information : QSystemTrayIcon::Warning;
  trayIcon_->showMessage("SurfPanel Config", message, icon, 3500);
}

bool MainWindow::isAutoStartEnabled() const {
#ifdef Q_OS_WIN
  QSettings settings = StartupSettings();
  const QString value = settings.value(StartupValueName()).toString();
  if (value.isEmpty()) {
    return false;
  }

  const QString normalized = NormalizeStartupValue(value);
  const QString exePath =
      QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
  return normalized.compare(exePath, Qt::CaseInsensitive) == 0;
#else
  return false;
#endif
}

void MainWindow::setAutoStartEnabled(bool enabled) {
#ifdef Q_OS_WIN
  QSettings settings = StartupSettings();
  const QString valueName = StartupValueName();
  if (enabled) {
    settings.setValue(valueName, StartupValueData());
  } else {
    settings.remove(valueName);
  }
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    qWarning() << "Failed to update startup registry entry.";
  }
#endif

  syncAutoStartAction();
}

void MainWindow::syncAutoStartAction() {
  if (autoStartAction_ == nullptr) {
    return;
  }

#ifdef Q_OS_WIN
  const bool enabled = isAutoStartEnabled();
  QSignalBlocker blocker(autoStartAction_);
  autoStartAction_->setEnabled(true);
  autoStartAction_->setChecked(enabled);
#else
  QSignalBlocker blocker(autoStartAction_);
  autoStartAction_->setChecked(false);
  autoStartAction_->setEnabled(false);
#endif
}

void MainWindow::openConfigDirectory() {
  const auto configDir = FindConfigRoot();
  if (!configDir.has_value()) {
    qWarning() << "Unable to locate config directory.";
    return;
  }

  const fs::path absolutePath = fs::absolute(*configDir);
#ifdef Q_OS_WIN
  QString dirPath =
      QDir::cleanPath(QString::fromStdWString(absolutePath.wstring()));
#else
  QString dirPath =
      QDir::cleanPath(QString::fromStdString(absolutePath.string()));
#endif
  dirPath = QDir::toNativeSeparators(dirPath);

#ifdef Q_OS_WIN
  const bool detached = QProcess::startDetached("explorer.exe", {dirPath});
  if (!detached) {
    qWarning() << "Failed to open config directory with explorer:" << dirPath;
  }
#else
  QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
#endif
}

void MainWindow::centerOnScreen() {
  QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }
  if (screen == nullptr) {
    return;
  }

  activeScreenGeometry_ = screen->availableGeometry();
  updatePaletteGeometry();
}

void MainWindow::updatePaletteGeometry() {
  if (showAnimation_ &&
      showAnimation_->state() == QAbstractAnimation::Running) {
    showAnimation_->stop();
  }
  if (!resultsSurface_) {
    return;
  }
  const int count = resultsModel_->rowCount();
  emptyState_->setText(input_->text().trimmed().isEmpty()
                           ? "Type to search actions"
                           : "No results");
  resultsSurface_->setCurrentWidget(count
                                        ? static_cast<QWidget *>(resultsView_)
                                        : static_cast<QWidget *>(emptyState_));
  QRect available = activeScreenGeometry_;
  if (!available.isValid()) {
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) {
      screen = QGuiApplication::primaryScreen();
    }
    available = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
  }
  const QRect geometry = PaletteGeometry(available, count);
  resultsSurface_->setFixedHeight(std::max(1, geometry.height() - 84));
  // Refresh the propagated minimum size before resizing a visible window.
  // Otherwise the previous result height clamps a shrink until a later layout
  // pass, leaving unused space around the input and empty-state row.
  panel_->layout()->activate();
  centralWidget()->layout()->activate();
  layout()->activate();
  setGeometry(geometry);
}

void MainWindow::toggleVisibilityFromHotkey() {
  if (isVisible()) {
    hidePanel();
    return;
  }

  showPanel();
}

void MainWindow::moveResultSelection(int delta) {
  const int count = resultsModel_->rowCount();
  if (count <= 0) {
    return;
  }

  int row = 0;
  const QModelIndex current = resultsView_->currentIndex();
  if (current.isValid()) {
    row = current.row() + delta;
  } else if (delta < 0) {
    row = count - 1;
  }

  if (row < 0) {
    row = 0;
  }
  if (row >= count) {
    row = count - 1;
  }

  const QModelIndex next = resultsModel_->index(row, 0);
  resultsView_->setCurrentIndex(next);
  resultsView_->scrollTo(next, QAbstractItemView::PositionAtCenter);
}

void MainWindow::onQueryTextChanged(const QString &text) {
  if (text.trimmed().isEmpty()) {
    resultsModel_->setResults(recentResultItems());
    if (resultsModel_->rowCount() > 0) {
      resultsView_->setCurrentIndex(resultsModel_->index(0, 0));
    }
    return;
  }

  const SearchQueryInfo query = searchEngine_.parseQuery(text);
  const std::size_t resultLimit =
      query.prefixMode ? kPrefixModeMaxResults : kTopK;
  const auto results = searchEngine_.search(text, resultLimit);
  resultsModel_->setResults(results);

  if (resultsModel_->rowCount() > 0) {
    resultsView_->setCurrentIndex(resultsModel_->index(0, 0));
  }
}

std::vector<const StringItem *> MainWindow::recentResultItems() const {
  std::vector<const StringItem *> results;
  results.reserve(kTopK);

  const std::vector<RecentItemKey> recent = recentItemsStore_.load();
  for (const auto &key : recent) {
    for (const auto &item : items_) {
      if (SameRecentItemKey(key, RecentKeyForItem(item))) {
        results.push_back(&item);
        break;
      }
    }

    if (results.size() >= kTopK) {
      break;
    }
  }

  return results;
}

void MainWindow::activateCurrentResult() {
  QModelIndex index = resultsView_->currentIndex();
  if (!index.isValid() && resultsModel_->rowCount() > 0) {
    index = resultsModel_->index(0, 0);
  }
  activateIndex(index);
}

void MainWindow::activateIndex(const QModelIndex &index) {
  if (!index.isValid()) {
    return;
  }

  invokeItemAction(resultsModel_->itemAt(index.row()));
}

void MainWindow::invokeItemAction(const StringItem *item) {
  if (item == nullptr) {
    return;
  }

  std::string actionName;
  QString payload;

  if (item->type.compare("url", Qt::CaseInsensitive) == 0 &&
      std::holds_alternative<UrlPayload>(item->payload)) {
    actionName = ActionManager::kOpenUrlAction;
    payload = std::get<UrlPayload>(item->payload).url;
  } else if (item->type.compare("snippet", Qt::CaseInsensitive) == 0 &&
             std::holds_alternative<SnippetPayload>(item->payload)) {
    actionName = ActionManager::kInjectContentAction;
    payload = std::get<SnippetPayload>(item->payload).snippet;
  }

  const bool pluginAction =
      item->type == "plugin" &&
      std::holds_alternative<PluginPayload>(item->payload);
  if (!pluginAction && (actionName.empty() || payload.isEmpty())) {
    return;
  }
  const Payload itemPayload = item->payload;
  const RecentItemKey recentKey = RecentKeyForItem(*item);

  hidePanel(false);
  resultsModel_->setResults({});

  QPointer<MainWindow> guardedThis(this);
  const auto finish = [guardedThis, recentKey](PluginFunctionResult result) {
    if (!guardedThis) {
      return;
    }
#ifdef Q_OS_WIN
    guardedThis->actionContext_.clearNativePasteTarget();
#endif
    if (!result.succeeded) {
      qWarning() << "Failed to invoke item:" << recentKey.name
                 << result.message;
      if (guardedThis->trayIcon_) {
        guardedThis->trayIcon_->showMessage("SurfPanel Action", result.message,
                                            QSystemTrayIcon::Warning, 3500);
      }
      return;
    }
    if (!guardedThis->recentItemsStore_.recordUse(recentKey, kTopK)) {
      qWarning() << "Failed to record recently used item:" << recentKey.name;
    }
  };
  QTimer::singleShot(
      30, this,
      [this, actionName, payload, itemPayload, pluginAction, finish]() {
        if (pluginAction) {
          const auto &target = std::get<PluginPayload>(itemPayload);
          pluginManager_.invokeFunction(target.plugin, target.function, finish);
        } else {
          actionContext_.clearLastError();
          const bool succeeded =
              actionManager_.invoke(actionName, payload, actionContext_);
          QString message = actionContext_.lastError();
          if (!succeeded && message.isEmpty()) {
            message = "Failed to invoke action.";
          }
          finish({succeeded, message});
        }
      });
}
