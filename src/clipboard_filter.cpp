#include "clipboard_filter.h"

#include <QDebug>
#include <QFileInfo>
#include <QTimer>
#include <algorithm>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

constexpr int kClipboardRetryDelayMs = 25;
constexpr int kClipboardMaxRetries = 3;

#ifdef Q_OS_WIN
QString ProcessNameForWindow(HWND owner) {
  if (owner == nullptr) {
    return {};
  }

  DWORD processId = 0;
  GetWindowThreadProcessId(owner, &processId);
  if (processId == 0) {
    return {};
  }

  HANDLE process =
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
  if (process == nullptr) {
    return {};
  }

  wchar_t imagePath[32768] = {};
  DWORD size = static_cast<DWORD>(std::size(imagePath));
  const BOOL resolved =
      QueryFullProcessImageNameW(process, 0, imagePath, &size);
  CloseHandle(process);
  if (!resolved) {
    return {};
  }

  return QFileInfo(QString::fromWCharArray(imagePath, static_cast<int>(size)))
      .fileName();
}

class NativeClipboardBackend final : public ClipboardBackend {
public:
  explicit NativeClipboardBackend(WId hostWindow) : hostWindow_(hostWindow) {}

  void setHostWindow(WId hostWindow) { hostWindow_ = hostWindow; }

  ClipboardReadResult read() override {
    ClipboardReadResult result;
    const HWND owner = GetClipboardOwner();
    result.content.ownerProcessName = ProcessNameForWindow(owner);

    if (!OpenClipboard(nullptr)) {
      result.status = ClipboardReadStatus::Busy;
      return result;
    }

    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
      CloseClipboard();
      result.status = ClipboardReadStatus::Ready;
      return result;
    }

    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data == nullptr) {
      CloseClipboard();
      result.status = ClipboardReadStatus::Busy;
      return result;
    }

    const auto *text = static_cast<const wchar_t *>(GlobalLock(data));
    if (text == nullptr) {
      CloseClipboard();
      result.status = ClipboardReadStatus::Busy;
      return result;
    }

    result.content.unicodeText = QString::fromWCharArray(text);
    result.content.hasUnicodeText = true;
    GlobalUnlock(data);
    CloseClipboard();
    result.content.sequenceNumber = GetClipboardSequenceNumber();
    result.status = ClipboardReadStatus::Ready;
    return result;
  }

  bool writeUnicodeText(const QString &text, quint32 *sequenceNumber) override {
    const std::wstring utf16 = text.toStdWString();
    const SIZE_T bytes = (utf16.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) {
      return false;
    }

    auto *destination = static_cast<wchar_t *>(GlobalLock(memory));
    if (destination == nullptr) {
      GlobalFree(memory);
      return false;
    }
    std::copy(utf16.cbegin(), utf16.cend(), destination);
    destination[utf16.size()] = L'\0';
    GlobalUnlock(memory);

    if (!OpenClipboard(reinterpret_cast<HWND>(hostWindow_))) {
      GlobalFree(memory);
      return false;
    }

    if (!EmptyClipboard() ||
        SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
      CloseClipboard();
      GlobalFree(memory);
      return false;
    }

    CloseClipboard();
    if (sequenceNumber != nullptr) {
      *sequenceNumber = GetClipboardSequenceNumber();
    }
    return true;
  }

private:
  WId hostWindow_;
};
#endif

std::unique_ptr<ClipboardBackend> CreateClipboardBackend(WId hostWindow) {
#ifdef Q_OS_WIN
  return std::make_unique<NativeClipboardBackend>(hostWindow);
#else
  Q_UNUSED(hostWindow);
  return nullptr;
#endif
}

} // namespace

void SourceMatcher::setSourceProcesses(
    const std::vector<QString> &sourceProcesses) {
  sourceProcesses_ = sourceProcesses;
}

bool SourceMatcher::matches(const QString &processName) const {
  return std::any_of(sourceProcesses_.cbegin(), sourceProcesses_.cend(),
                     [&processName](const QString &source) {
                       return source.compare(processName,
                                             Qt::CaseInsensitive) == 0;
                     });
}

bool SourceMatcher::empty() const { return sourceProcesses_.empty(); }

QString IdentityTextTransformer::transform(const QString &text) const {
  return text;
}

ClipboardProcessor::ClipboardProcessor(const TextTransformer &transformer)
    : transformer_(transformer) {}

void ClipboardProcessor::setConfiguration(const ClipboardFilterConfig &config) {
  enabled_ = config.enabled;
  sourceMatcher_.setSourceProcesses(config.sourceProcesses);
  selfWrittenSequence_.reset();
}

ClipboardProcessResult ClipboardProcessor::process(ClipboardBackend *backend) {
  if (!enabled_ || backend == nullptr) {
    return ClipboardProcessResult::Ignored;
  }

  const ClipboardReadResult read = backend->read();
  if (read.status == ClipboardReadStatus::Busy) {
    return ClipboardProcessResult::Retry;
  }
  if (read.status != ClipboardReadStatus::Ready ||
      selfWrittenSequence_ == read.content.sequenceNumber ||
      !read.content.hasUnicodeText ||
      !sourceMatcher_.matches(read.content.ownerProcessName)) {
    return ClipboardProcessResult::Ignored;
  }

  const QString transformed = transformer_.transform(read.content.unicodeText);
  if (transformed == read.content.unicodeText) {
    return ClipboardProcessResult::Ignored;
  }

  quint32 sequenceNumber = 0;
  if (!backend->writeUnicodeText(transformed, &sequenceNumber)) {
    return ClipboardProcessResult::WriteFailed;
  }
  selfWrittenSequence_ = sequenceNumber;
  return ClipboardProcessResult::Written;
}

ClipboardFilter::ClipboardFilter(QObject *parent)
    : QObject(parent), processor_(transformer_) {}

ClipboardFilter::~ClipboardFilter() { disable(); }

void ClipboardFilter::applyConfiguration(const ClipboardFilterConfig &config,
                                         WId hostWindow) {
  disable();
  config_ = config;
  hostWindow_ = hostWindow;
  processor_.setConfiguration(config_);
  ++generation_;

#ifdef Q_OS_WIN
  if (!config_.enabled || config_.sourceProcesses.empty() || hostWindow_ == 0) {
    return;
  }

  backend_ = CreateClipboardBackend(hostWindow_);
  listening_ = backend_ != nullptr &&
               AddClipboardFormatListener(reinterpret_cast<HWND>(hostWindow_));
  if (!listening_) {
    backend_.reset();
    qWarning() << "Failed to register the clipboard format listener.";
  }
#else
  Q_UNUSED(hostWindow_);
#endif
}

void ClipboardFilter::disable() {
  ++generation_;
  processing_ = false;
#ifdef Q_OS_WIN
  if (listening_) {
    RemoveClipboardFormatListener(reinterpret_cast<HWND>(hostWindow_));
  }
#endif
  listening_ = false;
  backend_.reset();
  hostWindow_ = 0;
  config_ = ClipboardFilterConfig{};
  processor_.setConfiguration(config_);
}

#ifdef Q_OS_WIN
bool ClipboardFilter::handleNativeMessage(unsigned int message) {
  if (!listening_ || message != WM_CLIPBOARDUPDATE) {
    return false;
  }
  if (!processing_) {
    processing_ = true;
    scheduleProcessing(0);
  }
  return true;
}
#endif

void ClipboardFilter::scheduleProcessing(int attempt) {
  const int generation = generation_;
  const int delay = attempt == 0 ? 0 : kClipboardRetryDelayMs;
  QTimer::singleShot(delay, this, [this, generation, attempt]() {
    if (generation != generation_ || !listening_) {
      return;
    }

    const ClipboardProcessResult result = processor_.process(backend_.get());
    if (result == ClipboardProcessResult::Retry &&
        attempt < kClipboardMaxRetries) {
      scheduleProcessing(attempt + 1);
      return;
    }

    if (result == ClipboardProcessResult::WriteFailed) {
      qWarning() << "Failed to write filtered clipboard text.";
    }
    processing_ = false;
  });
}
