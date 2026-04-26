#include "mainwindow.h"

#include "config.h"

#include <QAbstractItemView>
#include <QAbstractListModel>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPen>
#include <QScreen>
#include <QShortcut>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>
#include <optional>
#include <variant>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace fs = std::filesystem;

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
      : QStyledItemDelegate(parent) {}

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

    const QColor rowBg =
        selected ? QColor("#dbeafe") : QColor(255, 255, 255, 195);
    const QColor rowBorder =
        selected ? QColor("#60a5fa") : QColor(148, 163, 184, 75);
    painter->setPen(QPen(rowBorder, 1));
    painter->setBrush(rowBg);
    painter->drawRoundedRect(rowRect, 8, 8);

    const QString name = index.data(Qt::DisplayRole).toString();
    const QString typeRaw =
        index.data(SearchResultListModel::TypeRole).toString();
    const bool isUrl = typeRaw.compare("url", Qt::CaseInsensitive) == 0;
    const QString typeText = isUrl ? "URL" : "SNIPPET";

    const QColor tagBg = isUrl ? QColor("#2563eb") : QColor("#0f766e");
    const QColor tagTextColor("#f8fafc");

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
    painter->drawRoundedRect(tagRect, 10, 10);

    painter->setPen(tagTextColor);
    painter->drawText(tagRect, Qt::AlignCenter, typeText);

    const QRect nameRect = rowRect.adjusted(14, 0, -tagWidth - 24, 0);
    painter->setFont(nameFont);
    painter->setPen(QColor("#0f172a"));
    painter->drawText(
        nameRect, Qt::AlignVCenter | Qt::AlignLeft,
        nameMetrics.elidedText(name, Qt::ElideRight, nameRect.width()));

    painter->restore();
  }
};

namespace {

std::optional<fs::path> FindConfigPath() {
  std::vector<fs::path> candidates = {
      fs::path("config") / "test.toml",
      fs::path("..") / "config" / "test.toml",
      fs::path(QCoreApplication::applicationDirPath().toStdString()) / ".." /
          "config" / "test.toml",
  };

  for (const auto &candidate : candidates) {
    std::error_code ec;
    if (fs::exists(candidate, ec)) {
      return candidate;
    }
  }

  return std::nullopt;
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

} // namespace

MainWindow::MainWindow(QWidget *parent, bool enableHotkey)
    : QMainWindow(parent), input_(nullptr), resultsView_(nullptr),
      resultsModel_(nullptr), resultsDelegate_(nullptr),
      globalHotkeyRegistered_(false), hotkeyId_(1), fallbackShortcut_(nullptr) {
  RegisterDefaultActions(&actionManager_);

  setupWindow();
  setupUi();
  setupConnections();
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
  onQueryTextChanged(input_->text());
}

bool MainWindow::event(QEvent *event) {
  if (event->type() == QEvent::WindowDeactivate && isVisible()) {
    hide();
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
      hide();
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
  setWindowTitle("SurfPanel");
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
  setAttribute(Qt::WA_TranslucentBackground, true);
  resize(720, 360);
}

void MainWindow::setupUi() {
  QWidget *root = new QWidget(this);
  QVBoxLayout *rootLayout = new QVBoxLayout(root);
  rootLayout->setContentsMargins(0, 0, 0, 0);

  QFrame *panel = new QFrame(root);
  panel->setObjectName("panel");
  panel->setFrameShape(QFrame::NoFrame);

  QVBoxLayout *panelLayout = new QVBoxLayout(panel);
  panelLayout->setContentsMargins(14, 14, 14, 14);
  panelLayout->setSpacing(10);

  input_ = new QLineEdit(panel);
  input_->setObjectName("searchInput");
  input_->setPlaceholderText("Search bookmarks and snippets...");
  input_->setClearButtonEnabled(true);

  resultsView_ = new QListView(panel);
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

  rootLayout->addWidget(panel);
  setCentralWidget(root);
  applyStylesheet();
}

void MainWindow::applyStylesheet() {
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

  setStyleSheet(QString::fromUtf8(styleFile.readAll()));
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
      hide();
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

void MainWindow::loadBackendItems() {
  const auto configPath = FindConfigPath();
  if (!configPath.has_value()) {
    searchEngine_.setItems({});
    return;
  }

  try {
    const auto items = loadStringItems(*configPath);
    searchEngine_.setItems(items);
  } catch (const std::exception &e) {
    qWarning() << "Failed to load config for UI backend:" << e.what();
    searchEngine_.setItems({});
  }
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
    hide();
    return;
  }

  centerOnScreen();
  show();
  raise();
  activateWindow();
  input_->setFocus();
  input_->selectAll();
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

  hide();
  input_->clear();
  resultsModel_->setResults({});

  QTimer::singleShot(0, this, [this, actionName, payload]() {
    actionManager_.invoke(actionName, payload);
  });
}
