#include "mainwindow.h"

#include "config.h"

#include <QAbstractItemView>
#include <QAbstractListModel>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFont>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLinearGradient>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QProcess>
#include <QRandomGenerator>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <variant>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {
QColor AccentForegroundColor(const QColor &accent);
}

class SearchResultListModel final : public QAbstractListModel {
public:
  enum Roles {
    TypeRole = Qt::UserRole + 1,
  };

  explicit SearchResultListModel(QObject *parent = nullptr)
      : QAbstractListModel(parent) {}

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    if (parent.isValid()) {
      return 0;
    }
    return static_cast<int>(results_.size());
  }

  QVariant data(const QModelIndex &index, int role) const override {
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

  void setResults(std::vector<const StringItem *> results) {
    beginResetModel();
    results_ = std::move(results);
    endResetModel();
  }

  const StringItem *itemAt(int row) const {
    if (row < 0 || row >= static_cast<int>(results_.size())) {
      return nullptr;
    }
    return results_[row];
  }

private:
  std::vector<const StringItem *> results_;
};

class SearchResultItemDelegate final : public QStyledItemDelegate {
public:
  explicit SearchResultItemDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent), accentColor_(QColor("#005FB8")),
        darkMode_(false) {}

  void setTheme(const QColor &accentColor, bool darkMode) {
    accentColor_ = accentColor;
    darkMode_ = darkMode;
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &) const override {
    return {option.rect.width(), 44};
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRect rowRect = option.rect.adjusted(8, 4, -8, -4);
    const bool selected = (option.state & QStyle::State_Selected) != 0;

    const QColor baseRowBg =
        darkMode_ ? QColor(44, 44, 44, 230) : QColor(255, 255, 255, 195);
    const QColor baseRowBorder =
        darkMode_ ? QColor(255, 255, 255, 28) : QColor(148, 163, 184, 75);
    QColor selectedBg = accentColor_;
    selectedBg.setAlpha(darkMode_ ? 102 : 77);
    QColor selectedBorder = accentColor_;
    selectedBorder.setAlpha(darkMode_ ? 200 : 220);

    const QColor rowBg = selected ? selectedBg : baseRowBg;
    const QColor rowBorder = selected ? selectedBorder : baseRowBorder;
    painter->setPen(QPen(rowBorder, 1));
    painter->setBrush(rowBg);
    painter->drawRoundedRect(rowRect, 4, 4);

    const QString name = index.data(Qt::DisplayRole).toString();
    const QString typeRaw =
        index.data(SearchResultListModel::TypeRole).toString();
    const bool isUrl = typeRaw.compare("url", Qt::CaseInsensitive) == 0;
    const QString typeText = isUrl ? "URL" : "SNIPPET";

    QColor tagBg = accentColor_;
    if (!isUrl) {
      tagBg = accentColor_.darker(115);
    }
    const QColor tagTextColor = AccentForegroundColor(accentColor_);

    QFont nameFont = option.font;
    nameFont.setPointSizeF(11.5);
    nameFont.setWeight(QFont::DemiBold);
    painter->setFont(nameFont);

    const QFontMetrics nameMetrics(nameFont);

    QFont tagFont = option.font;
    tagFont.setPointSizeF(9.0);
    tagFont.setWeight(QFont::Medium);
    const QFontMetrics tagMetrics(tagFont);

    const int tagPaddingX = 8;
    const int tagHeight = 20;
    const int tagWidth =
        tagMetrics.horizontalAdvance(typeText) + tagPaddingX * 2;
    const QRect tagRect(rowRect.right() - tagWidth - 12,
                        rowRect.center().y() - tagHeight / 2, tagWidth,
                        tagHeight);

    painter->setFont(tagFont);
    painter->setBrush(tagBg);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(tagRect, 4, 4);

    painter->setPen(tagTextColor);
    painter->drawText(tagRect, Qt::AlignCenter, typeText);

    const QRect nameRect = rowRect.adjusted(14, 0, -tagWidth - 24, 0);
    painter->setFont(nameFont);
    painter->setPen(darkMode_ ? QColor("#F1F5F9") : QColor("#0F172A"));
    painter->drawText(
        nameRect, Qt::AlignVCenter | Qt::AlignLeft,
        nameMetrics.elidedText(name, Qt::ElideRight, nameRect.width()));

    painter->restore();
  }

private:
  QColor accentColor_;
  bool darkMode_;
};

class FluentPanel final : public QFrame {
public:
  explicit FluentPanel(QWidget *parent = nullptr)
      : QFrame(parent), baseColor_(Qt::transparent),
        borderColor_(Qt::transparent), cornerRadius_(8), darkMode_(false) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
  }

  void setThemeColors(const QColor &baseColor, const QColor &borderColor,
                      bool darkMode) {
    baseColor_ = baseColor;
    borderColor_ = borderColor;
    darkMode_ = darkMode;
    rebuildNoise();
    update();
  }

  void setCornerRadius(int radius) {
    if (cornerRadius_ == radius) {
      return;
    }
    cornerRadius_ = radius;
    update();
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRectF rect = this->rect();
    rect.adjust(0.5, 0.5, -0.5, -0.5);

    QPainterPath path;
    path.addRoundedRect(rect, cornerRadius_, cornerRadius_);

    painter.setPen(Qt::NoPen);
    painter.setBrush(baseColor_);
    painter.drawPath(path);

    painter.save();
    painter.setClipPath(path);

    QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
    gradient.setColorAt(0.0, QColor(255, 255, 255, darkMode_ ? 10 : 6));
    gradient.setColorAt(1.0, QColor(0, 0, 0, darkMode_ ? 18 : 8));
    painter.setBrush(gradient);
    painter.drawRect(rect);

    if (!noiseTile_.isNull()) {
      painter.setBrush(QBrush(noiseTile_));
      painter.drawRect(rect);
    }

    painter.restore();

    painter.setPen(QPen(borderColor_, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
  }

private:
  void rebuildNoise() { noiseTile_ = QPixmap(); }

  QColor baseColor_;
  QColor borderColor_;
  int cornerRadius_;
  bool darkMode_;
  QPixmap noiseTile_;
};

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

QColor BlendColors(const QColor &base, const QColor &tint, double tintRatio) {
  const double ratio = std::clamp(tintRatio, 0.0, 1.0);
  const double baseRatio = 1.0 - ratio;
  const int red = static_cast<int>(base.red() * baseRatio + tint.red() * ratio);
  const int green =
      static_cast<int>(base.green() * baseRatio + tint.green() * ratio);
  const int blue =
      static_cast<int>(base.blue() * baseRatio + tint.blue() * ratio);
  return QColor(red, green, blue);
}

QColor AccentForegroundColor(const QColor &accent) {
  const double luminance = (0.2126 * accent.red() + 0.7152 * accent.green() +
                            0.0722 * accent.blue()) /
                           255.0;
  return luminance > 0.6 ? QColor(10, 10, 10) : QColor(248, 250, 252);
}

QIcon BuildSearchIcon(const QColor &color, int size) {
  QPixmap pixmap(size, size);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QPen pen(color);
  pen.setWidthF(1.6);
  pen.setCapStyle(Qt::RoundCap);
  painter.setPen(pen);

  const QPointF center(size * 0.45, size * 0.45);
  const qreal radius = size * 0.22;
  painter.drawEllipse(center, radius, radius);
  painter.drawLine(QPointF(size * 0.60, size * 0.60),
                   QPointF(size * 0.78, size * 0.78));

  return QIcon(pixmap);
}

std::optional<QString> FindStylesheetPath() {
  const QString appDir = QCoreApplication::applicationDirPath();
  const std::vector<QString> candidates = {
      "src/mainwindow_fluent.qss",
      "../src/mainwindow_fluent.qss",
      "../../src/mainwindow_fluent.qss",
      appDir + "/mainwindow_fluent.qss",
      appDir + "/../src/mainwindow_fluent.qss",
      appDir + "/../../src/mainwindow_fluent.qss",
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

MainWindow::MainWindow(QWidget *parent, bool enableHotkey)
    : QMainWindow(parent), input_(nullptr), resultsView_(nullptr),
      resultsModel_(nullptr), resultsDelegate_(nullptr), panel_(nullptr),
      panelShadow_(nullptr), searchIconAction_(nullptr),
      accentColor_(QColor("#005FB8")), isDarkMode_(false), trayIcon_(nullptr),
      trayMenu_(nullptr), showPanelAction_(nullptr),
      showConfigDirAction_(nullptr), reloadConfigAction_(nullptr),
      autoStartAction_(nullptr), exitAction_(nullptr),
      globalHotkeyRegistered_(false), hotkeyId_(1), fallbackShortcut_(nullptr) {
  RegisterDefaultActions(&actionManager_);

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
#ifdef Q_OS_WIN
  if (globalHotkeyRegistered_) {
    UnregisterHotKey(reinterpret_cast<HWND>(winId()), hotkeyId_);
  }
#endif
}

void MainWindow::setItems(const std::vector<StringItem> &items) {
  searchEngine_.setItems(items);
  searchEngine_.setSearchPrefixes(DefaultSearchPrefixes());
  onQueryTextChanged(input_->text());
}

bool MainWindow::event(QEvent *event) {
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
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
  setAttribute(Qt::WA_TranslucentBackground, true);
  resize(720, 360);
}

void MainWindow::setupUi() {
  QWidget *root = new QWidget(this);
  QVBoxLayout *rootLayout = new QVBoxLayout(root);
  rootLayout->setContentsMargins(0, 0, 0, 0);

  panel_ = new FluentPanel(root);
  panel_->setObjectName("panel");
  panel_->setFrameShape(QFrame::NoFrame);
  panel_->setCornerRadius(8);

  panelShadow_ = new QGraphicsDropShadowEffect(panel_);
  panel_->setGraphicsEffect(panelShadow_);

  QVBoxLayout *panelLayout = new QVBoxLayout(panel_);
  panelLayout->setContentsMargins(14, 14, 14, 14);
  panelLayout->setSpacing(10);

  input_ = new QLineEdit(panel_);
  input_->setObjectName("searchInput");
  input_->setPlaceholderText("Search bookmarks and snippets...");
  input_->setClearButtonEnabled(true);

  resultsView_ = new QListView(panel_);
  resultsView_->setObjectName("resultsList");
  resultsView_->setFrameShape(QFrame::NoFrame);
  resultsView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  resultsView_->setSelectionMode(QAbstractItemView::SingleSelection);
  resultsView_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  resultsView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  resultsView_->setUniformItemSizes(true);

  resultsModel_ = new SearchResultListModel(this);
  resultsDelegate_ = new SearchResultItemDelegate(this);
  resultsView_->setModel(resultsModel_);
  resultsView_->setItemDelegate(resultsDelegate_);

  panelLayout->addWidget(input_);
  panelLayout->addWidget(resultsView_, 1);

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
  const QColor textColor = isDarkMode_ ? QColor("#F1F5F9") : QColor("#0F172A");
  const QColor placeholderColor =
      isDarkMode_ ? QColor("#94A3B8") : QColor("#64748B");
  const QColor inputBg =
      isDarkMode_ ? QColor(48, 48, 48, 255) : QColor(255, 255, 255, 255);
  const QColor inputBgFocus =
      isDarkMode_ ? QColor(56, 56, 56, 255) : QColor(255, 255, 255, 255);
  const QColor inputBorder =
      isDarkMode_ ? QColor(255, 255, 255, 25) : QColor(30, 41, 59, 35);
  const QColor selectionText = AccentForegroundColor(accent);
  QColor selectionBg = accent;
  selectionBg.setAlpha(isDarkMode_ ? 102 : 77);

  const QColor menuBg =
      isDarkMode_ ? QColor(32, 32, 32, 235) : QColor(255, 255, 255, 230);
  const QColor menuBorder =
      isDarkMode_ ? QColor(255, 255, 255, 25) : QColor(0, 0, 0, 30);
  const QColor menuSeparator =
      isDarkMode_ ? QColor(70, 70, 70) : QColor(224, 224, 224);
  const QColor scrollbarHandle =
      isDarkMode_ ? QColor(148, 163, 184, 120) : QColor(100, 116, 139, 120);

  QString themed = styleSource;
  themed.replace("@accent_color", ToHexString(accent));
  themed.replace("@text_color", ToHexString(textColor));
  themed.replace("@placeholder_color", ToHexString(placeholderColor));
  themed.replace("@input_bg_focus", ToRgbaString(inputBgFocus));
  themed.replace("@input_bg", ToRgbaString(inputBg));
  themed.replace("@input_border", ToRgbaString(inputBorder));
  themed.replace("@selection_bg", ToRgbaString(selectionBg));
  themed.replace("@selection_text", ToHexString(selectionText));
  themed.replace("@menu_bg", ToRgbaString(menuBg));
  themed.replace("@menu_border", ToRgbaString(menuBorder));
  themed.replace("@menu_separator", ToHexString(menuSeparator));
  themed.replace("@scrollbar_handle", ToRgbaString(scrollbarHandle));

  setStyleSheet(themed);

  if (input_ != nullptr) {
    QPalette palette = input_->palette();
    palette.setColor(QPalette::Base, inputBg);
    palette.setColor(QPalette::Text, textColor);
    palette.setColor(QPalette::PlaceholderText, placeholderColor);
    palette.setColor(QPalette::Highlight, selectionBg);
    palette.setColor(QPalette::HighlightedText, selectionText);
    input_->setPalette(palette);

    const QString inputStyle =
        QString("QLineEdit#searchInput { background: %1; color: %2; border: "
                "1px solid %3; "
                "border-radius: 4px; padding: 12px 14px; font-size: 15px; "
                "selection-background-color: %4; selection-color: %5; }"
                "QLineEdit#searchInput:focus { background: %6; border: 1px "
                "solid %7; }"
                "QLineEdit#searchInput::placeholder { color: %8; }")
            .arg(ToRgbaString(inputBg))
            .arg(ToHexString(textColor))
            .arg(ToRgbaString(inputBorder))
            .arg(ToRgbaString(selectionBg))
            .arg(ToHexString(selectionText))
            .arg(ToRgbaString(inputBgFocus))
            .arg(ToHexString(accent))
            .arg(ToHexString(placeholderColor));
    input_->setStyleSheet(inputStyle);
  }
}

void MainWindow::updateTheme() {
  const bool darkMode = isSystemDarkMode();
  const QColor accent = querySystemAccentColor();
  const bool themeChanged =
      (darkMode != isDarkMode_) || (accent != accentColor_ && accent.isValid());
  const bool needsApply = themeChanged || styleSheet().isEmpty();

  isDarkMode_ = darkMode;
  if (accent.isValid()) {
    accentColor_ = accent;
  }

  if (needsApply) {
    applyStylesheet();
    updateDropShadow();
    updateSearchIcon();

    if (resultsDelegate_ != nullptr) {
      resultsDelegate_->setTheme(accentColor_, isDarkMode_);
    }

    if (resultsView_ != nullptr) {
      resultsView_->viewport()->update();
    }
  }

  updatePanelBackground();
}

void MainWindow::updatePanelBackground() {
  if (panel_ == nullptr) {
    return;
  }

  const QColor base = isDarkMode_ ? QColor("#1C1C1C") : QColor("#F3F3F3");
  QColor wallpaper = sampleWallpaperDominantColor();
  if (!wallpaper.isValid()) {
    wallpaper = base;
  }

  const double tintRatio = isDarkMode_ ? 0.15 : 0.10;
  QColor mica = BlendColors(base, wallpaper, tintRatio);
  mica.setAlpha(isDarkMode_ ? 242 : 248);

  const QColor border =
      isDarkMode_ ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 15);
  panel_->setThemeColors(mica, border, isDarkMode_);
}

void MainWindow::updateDropShadow() {
  if (panelShadow_ == nullptr) {
    return;
  }

  panelShadow_->setBlurRadius(48.0);
  panelShadow_->setOffset(0, 8);
  panelShadow_->setColor(QColor(0, 0, 0, isDarkMode_ ? 102 : 38));
}

void MainWindow::updateSearchIcon() {
  if (searchIconAction_ == nullptr) {
    return;
  }

  const QColor iconColor = isDarkMode_ ? QColor("#94A3B8") : QColor("#64748B");
  searchIconAction_->setIcon(BuildSearchIcon(iconColor, 16));
}

bool MainWindow::isSystemDarkMode() const {
#ifdef Q_OS_WIN
  QSettings settings(
      "HKEY_CURRENT_"
      "USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      QSettings::NativeFormat);
  return settings.value("AppsUseLightTheme", 1).toInt() == 0;
#else
  return false;
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

QColor MainWindow::sampleWallpaperDominantColor() const {
#ifdef Q_OS_WIN
  QSettings settings("HKEY_CURRENT_USER\\Control Panel\\Desktop",
                     QSettings::NativeFormat);
  const QString wallpaperPath = settings.value("WallPaper").toString();
  if (wallpaperPath.isEmpty() || !QFile::exists(wallpaperPath)) {
    return QColor();
  }

  QImage wallpaper(wallpaperPath);
  if (wallpaper.isNull()) {
    return QColor();
  }

  const QImage scaled =
      wallpaper.scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  return QColor::fromRgb(scaled.pixel(0, 0));
#else
  return QColor();
#endif
}

void MainWindow::setupTrayIcon() {
  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    qWarning() << "System tray is unavailable on this platform/session.";
    return;
  }

  trayMenu_ = new QMenu(this);
  showPanelAction_ = trayMenu_->addAction("Show Panel");
  showConfigDirAction_ = trayMenu_->addAction("Show Config File Dir");
  reloadConfigAction_ = trayMenu_->addAction("Reload Config");
  autoStartAction_ = trayMenu_->addAction("Start with Windows");
  autoStartAction_->setCheckable(true);
  trayMenu_->addSeparator();
  exitAction_ = trayMenu_->addAction("Exit");

  connect(showPanelAction_, &QAction::triggered, this, &MainWindow::showPanel);
  connect(showConfigDirAction_, &QAction::triggered, this,
          &MainWindow::openConfigDirectory);
  connect(reloadConfigAction_, &QAction::triggered, this,
          &MainWindow::reloadConfig);
  connect(autoStartAction_, &QAction::toggled, this,
          &MainWindow::setAutoStartEnabled);
  connect(trayMenu_, &QMenu::aboutToShow, this,
          &MainWindow::syncAutoStartAction);
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
  const auto configRoot = FindConfigRoot();
  if (!configRoot.has_value()) {
    searchEngine_.setItems({});
    searchEngine_.setSearchPrefixes(DefaultSearchPrefixes());
    ConfigLoadResult result;
    result.searchPrefixes = DefaultSearchPrefixes();
    result.ok = false;
    result.message = "Config directory not found.";
    return result;
  }

  auto result = LoadConfigWithFallback(*configRoot);
  searchEngine_.setItems(result.items);
  searchEngine_.setSearchPrefixes(result.searchPrefixes);
  onQueryTextChanged(input_->text());

  if (!result.ok || result.usedFallback) {
    qWarning() << "Config load warning:"
               << QString::fromStdString(result.message);
  }

  return result;
}

void MainWindow::showPanel() {
  updateTheme();

  if (isVisible()) {
    raise();
    activateWindow();
    input_->setFocus();
    input_->selectAll();
    return;
  }

  centerOnScreen();
  setWindowOpacity(1.0);
  show();
  raise();
  activateWindow();
  input_->setFocus();
  input_->selectAll();
}

void MainWindow::hidePanel() {
  if (!isVisible()) {
    return;
  }

  hide();
  setWindowOpacity(1.0);
}

void MainWindow::reloadConfig() {
  const auto result = loadBackendItems();
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
  QString dirPath =
      QDir::cleanPath(QString::fromStdString(absolutePath.string()));
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
  QScreen *screen = QGuiApplication::primaryScreen();
  if (screen == nullptr) {
    return;
  }

  const QRect available = screen->availableGeometry();
  const QPoint centered = available.center() - rect().center();
  move(centered);
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
  const auto results = searchEngine_.search(text, kTopK);
  resultsModel_->setResults(results);

  if (resultsModel_->rowCount() > 0) {
    resultsView_->setCurrentIndex(resultsModel_->index(0, 0));
  }
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

  if (actionName.empty() || payload.isEmpty()) {
    return;
  }

  hidePanel();
  input_->clear();
  resultsModel_->setResults({});

  QTimer::singleShot(0, this, [this, actionName, payload]() {
    actionManager_.invoke(actionName, payload);
  });
}
