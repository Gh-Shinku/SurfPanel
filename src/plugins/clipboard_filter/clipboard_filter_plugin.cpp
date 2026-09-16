#include "clipboard_filter_plugin.h"

#include "toml.hpp"

#include <QDebug>
#include <QFileInfo>
#include <QTimer>
#include <algorithm>
#include <filesystem>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr int kClipboardRetryDelayMs = 25;
constexpr int kClipboardMaxRetries = 3;

QString JoinMessages(const std::vector<QString> &messages) {
  QString result;
  for (const auto &message : messages) {
    if (!result.isEmpty()) {
      result += '\n';
    }
    result += message;
  }
  return result;
}

PluginConfigurationResult ParseConfiguration(const toml::value &value,
                                             bool legacy,
                                             ClipboardFilterConfig *config) {
  std::vector<QString> messages;
  if (legacy) {
    messages.push_back(
        "using deprecated [clipboard_filter] configuration; move it to "
        "plugins/clipboard-filter.toml");
  }

  if (!value.is_table()) {
    return {PluginConfigurationState::Invalid,
            JoinMessages(messages) + (messages.empty() ? "" : "\n") +
                "configuration must be a table"};
  }

  const auto enabled = toml::find_or(value, "enabled", toml::value{false});
  if (!enabled.is_boolean()) {
    messages.push_back("enabled must be a boolean");
    return {PluginConfigurationState::Invalid, JoinMessages(messages)};
  }
  if (!enabled.as_boolean()) {
    return {PluginConfigurationState::Disabled, JoinMessages(messages)};
  }

#ifndef Q_OS_WIN
  messages.push_back("clipboard monitoring is only supported on Windows");
  return {PluginConfigurationState::Invalid, JoinMessages(messages)};
#else
  const auto sources =
      toml::find_or(value, "source_processes", toml::value{toml::array{}});
  if (!sources.is_array()) {
    messages.push_back("source_processes must be an array of process names");
    return {PluginConfigurationState::Invalid, JoinMessages(messages)};
  }

  config->enabled = true;
  for (const auto &source : sources.as_array()) {
    if (!source.is_string()) {
      messages.push_back("ignoring a non-string source process");
      continue;
    }

    const QString process =
        QString::fromStdString(source.as_string()).trimmed();
    if (process.isEmpty() || process.contains('/') || process.contains('\\')) {
      messages.push_back("ignoring invalid source process: " +
                         QString::fromStdString(source.as_string()));
      continue;
    }

    const bool duplicate = std::any_of(
        config->sourceProcesses.cbegin(), config->sourceProcesses.cend(),
        [&process](const QString &existing) {
          return existing.compare(process, Qt::CaseInsensitive) == 0;
        });
    if (!duplicate) {
      config->sourceProcesses.push_back(process);
    }
  }

  if (config->sourceProcesses.empty()) {
    *config = ClipboardFilterConfig{};
    messages.push_back("enabled but no valid source processes were configured");
    return {PluginConfigurationState::Invalid, JoinMessages(messages)};
  }

  return {PluginConfigurationState::Enabled, JoinMessages(messages)};
#endif
}

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

  ClipboardReadResult read() override {
    ClipboardReadResult result;
    result.content.ownerProcessName = ProcessNameForWindow(GetClipboardOwner());

    if (!OpenClipboard(nullptr)) {
      result.status = ClipboardReadStatus::Busy;
      return result;
    }
    result.content.sequenceNumber = GetClipboardSequenceNumber();
    result.content.ownerProcessName = ProcessNameForWindow(GetClipboardOwner());

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
    result.status = ClipboardReadStatus::Ready;
    return result;
  }

  ClipboardWriteResult writeUnicodeText(const QString &text,
                                        quint32 expectedSequence,
                                        quint32 *sequenceNumber) override {
    const std::wstring utf16 = text.toStdWString();
    const SIZE_T bytes = (utf16.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) {
      return ClipboardWriteResult::Failed;
    }

    auto *destination = static_cast<wchar_t *>(GlobalLock(memory));
    if (destination == nullptr) {
      GlobalFree(memory);
      return ClipboardWriteResult::Failed;
    }
    std::copy(utf16.cbegin(), utf16.cend(), destination);
    destination[utf16.size()] = L'\0';
    GlobalUnlock(memory);

    if (!OpenClipboard(reinterpret_cast<HWND>(hostWindow_))) {
      GlobalFree(memory);
      return ClipboardWriteResult::Busy;
    }

    if (GetClipboardSequenceNumber() != expectedSequence) {
      CloseClipboard();
      GlobalFree(memory);
      return ClipboardWriteResult::Changed;
    }

    if (!EmptyClipboard() ||
        SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
      CloseClipboard();
      GlobalFree(memory);
      return ClipboardWriteResult::Failed;
    }

    if (sequenceNumber != nullptr) {
      *sequenceNumber = GetClipboardSequenceNumber();
    }
    CloseClipboard();
    return ClipboardWriteResult::Written;
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

ClipboardProcessor::ClipboardProcessor(const TextTransformer &transformer)
    : transformer_(transformer) {}

void ClipboardProcessor::setConfiguration(const ClipboardFilterConfig &config) {
  enabled_ = config.enabled;
  sourceMatcher_.setSourceProcesses(config.sourceProcesses);
  selfWrittenSequence_.reset();
}

ClipboardProcessResult ClipboardProcessor::process(
    ClipboardBackend *backend,
    const std::optional<ClipboardUpdateContext> &updateContext, bool manual,
    std::optional<quint32> expectedSequence) {
  if ((!manual && !enabled_) || backend == nullptr) {
    return ClipboardProcessResult::Ignored;
  }

  ClipboardReadResult read = backend->read();
  if (read.status == ClipboardReadStatus::Busy) {
    return ClipboardProcessResult::Retry;
  }
  if (expectedSequence && read.status == ClipboardReadStatus::Ready &&
      read.content.sequenceNumber != *expectedSequence) {
    return ClipboardProcessResult::Changed;
  }
  if (updateContext.has_value() &&
      updateContext->sequenceNumber == read.content.sequenceNumber &&
      read.content.ownerProcessName.isEmpty()) {
    read.content.ownerProcessName = updateContext->sourceProcessName;
  }
  if (read.status != ClipboardReadStatus::Ready ||
      (!manual && (selfWrittenSequence_ == read.content.sequenceNumber ||
                   !sourceMatcher_.matches(read.content.ownerProcessName)))) {
    return ClipboardProcessResult::Ignored;
  }
  if (!read.content.hasUnicodeText || read.content.unicodeText.isEmpty()) {
    return manual ? ClipboardProcessResult::NoText
                  : ClipboardProcessResult::Ignored;
  }

  const QString transformed = transformer_.transform(read.content.unicodeText);
  // Republish even unchanged text under SurfPanel's ownership so clipboard
  // managers that exclude the source PDF reader can capture every copy.
  quint32 sequenceNumber = 0;
  const auto write = backend->writeUnicodeText(
      transformed, read.content.sequenceNumber, &sequenceNumber);
  if (write == ClipboardWriteResult::Busy) {
    return ClipboardProcessResult::Retry;
  }
  if (write == ClipboardWriteResult::Changed) {
    return ClipboardProcessResult::Changed;
  }
  if (write != ClipboardWriteResult::Written) {
    return ClipboardProcessResult::WriteFailed;
  }
  selfWrittenSequence_ = sequenceNumber;
  return ClipboardProcessResult::Written;
}

ClipboardFilterPlugin::ClipboardFilterPlugin(BackendFactory backendFactory)
    : processor_(transformer_),
      backendFactory_(backendFactory ? std::move(backendFactory)
                                     : CreateClipboardBackend) {}

ClipboardFilterPlugin::~ClipboardFilterPlugin() { stop(); }

PluginMetadata ClipboardFilterPlugin::metadata() const {
  return {"clipboard-filter", "Clipboard Filter", kSurfPanelPluginApiVersion};
}

std::vector<PluginFunction> ClipboardFilterPlugin::functions() const {
  return {{"filter", "Normalize the current clipboard text and write it back "
                     "without restricting its source (Windows only)."}};
}

void ClipboardFilterPlugin::invokeFunction(
    const QString &name, const PluginHostContext &context,
    PluginFunctionCompletion completion) {
  if (name != "filter") {
    completion({false, "Unknown clipboard function: " + name});
    return;
  }
#ifdef Q_OS_WIN
  if (manualCompletion_) {
    completion({false, "Clipboard filtering is already in progress."});
    return;
  }
  if (context.nativeWindow == 0 || context.eventLoopOwner == nullptr) {
    completion({false, "Clipboard host is not available."});
    return;
  }
  manualBackend_ = backendFactory_(context.nativeWindow);
  manualCompletion_ = std::move(completion);
  processManual(0, GetClipboardSequenceNumber());
#else
  Q_UNUSED(context);
  completion({false, "Clipboard filtering is only supported on Windows."});
#endif
}

void ClipboardFilterPlugin::finishManual(PluginFunctionResult result) {
  auto completion = std::move(manualCompletion_);
  manualCompletion_ = {};
  manualBackend_.reset();
  if (completion) {
    completion(std::move(result));
  }
}

void ClipboardFilterPlugin::processManual(int attempt,
                                          quint32 expectedSequence) {
  const auto result = processor_.process(manualBackend_.get(), std::nullopt,
                                         true, expectedSequence);
  if (result == ClipboardProcessResult::Retry &&
      attempt < kClipboardMaxRetries) {
    const int generation = generation_;
    QTimer::singleShot(kClipboardRetryDelayMs, this,
                       [this, generation, attempt, expectedSequence]() {
                         if (generation == generation_ && manualCompletion_) {
                           processManual(attempt + 1, expectedSequence);
                         }
                       });
    return;
  }
  if (result == ClipboardProcessResult::Written) {
    finishManual({true, {}});
  } else if (result == ClipboardProcessResult::Changed) {
    finishManual({false, "Clipboard changed; filtering was cancelled."});
  } else if (result == ClipboardProcessResult::NoText) {
    finishManual({false, "Clipboard does not contain non-empty Unicode text."});
  } else if (result == ClipboardProcessResult::Retry) {
    finishManual({false, "Clipboard is busy. Please try again."});
  } else {
    finishManual({false, "Failed to filter clipboard text."});
  }
}

PluginConfigurationResult
ClipboardFilterPlugin::configure(const PluginConfigurationContext &context) {
  stop();
  config_ = ClipboardFilterConfig{};
  processor_.setConfiguration(config_);

  try {
    toml::value value;
    bool legacy = false;
    if (fs::exists(context.pluginConfigPath)) {
      value = toml::parse(context.pluginConfigPath, toml::spec::v(1, 1, 0));
    } else if (fs::exists(context.legacyMainConfigPath)) {
      const auto root =
          toml::parse(context.legacyMainConfigPath, toml::spec::v(1, 1, 0));
      if (!root.contains("clipboard_filter")) {
        return {};
      }
      value = toml::find(root, "clipboard_filter");
      legacy = true;
    } else {
      return {};
    }

    PluginConfigurationResult result =
        ParseConfiguration(value, legacy, &config_);
    if (result.state == PluginConfigurationState::Enabled) {
      processor_.setConfiguration(config_);
    }
    return result;
  } catch (const std::exception &error) {
    return {PluginConfigurationState::Invalid,
            QString("failed to parse configuration: %1").arg(error.what())};
  }
}

bool ClipboardFilterPlugin::start(const PluginHostContext &context) {
  stop();
#ifdef Q_OS_WIN
  if (!config_.enabled || config_.sourceProcesses.empty() ||
      context.eventLoopOwner == nullptr || context.nativeWindow == 0) {
    return false;
  }

  hostContext_ = context;
  backend_ = backendFactory_(context.nativeWindow);
  listening_ =
      backend_ != nullptr &&
      AddClipboardFormatListener(reinterpret_cast<HWND>(context.nativeWindow));
  if (!listening_) {
    log(QtCriticalMsg, "failed to register clipboard format listener");
    backend_.reset();
    hostContext_ = PluginHostContext{};
    return false;
  }
  return true;
#else
  Q_UNUSED(context);
  return false;
#endif
}

void ClipboardFilterPlugin::stop() {
  ++generation_;
  finishManual({false, "Clipboard filtering was cancelled."});
  processing_ = false;
#ifdef Q_OS_WIN
  if (listening_) {
    RemoveClipboardFormatListener(
        reinterpret_cast<HWND>(hostContext_.nativeWindow));
  }
#endif
  listening_ = false;
  backend_.reset();
  hostContext_ = PluginHostContext{};
}

bool ClipboardFilterPlugin::handleNativeEvent(const QByteArray &eventType,
                                              void *message, qintptr *result) {
  Q_UNUSED(eventType);
  Q_UNUSED(result);
#ifdef Q_OS_WIN
  const auto *nativeMessage = static_cast<MSG *>(message);
  if (!listening_ || nativeMessage == nullptr ||
      nativeMessage->message != WM_CLIPBOARDUPDATE) {
    return false;
  }
  if (!processing_) {
    ClipboardUpdateContext updateContext;
    updateContext.sourceProcessName = ProcessNameForWindow(GetClipboardOwner());
    if (updateContext.sourceProcessName.isEmpty()) {
      updateContext.sourceProcessName =
          ProcessNameForWindow(GetForegroundWindow());
    }
    updateContext.sequenceNumber = GetClipboardSequenceNumber();
    processing_ = true;
    scheduleProcessing(0, std::move(updateContext));
  }
  return true;
#else
  Q_UNUSED(message);
  return false;
#endif
}

void ClipboardFilterPlugin::scheduleProcessing(
    int attempt, ClipboardUpdateContext updateContext) {
  const int generation = generation_;
  const int delay = attempt == 0 ? 0 : kClipboardRetryDelayMs;
  QTimer::singleShot(
      delay, this,
      [this, generation, attempt, updateContext = std::move(updateContext)]() {
        if (generation != generation_ || !listening_) {
          return;
        }

        const ClipboardProcessResult result =
            processor_.process(backend_.get(), updateContext);
        if (result == ClipboardProcessResult::Retry &&
            attempt < kClipboardMaxRetries) {
          scheduleProcessing(attempt + 1, updateContext);
          return;
        }
        if (result == ClipboardProcessResult::WriteFailed) {
          log(QtWarningMsg, "failed to write filtered clipboard text");
        }
        processing_ = false;
      });
}

void ClipboardFilterPlugin::log(QtMsgType type, const QString &message) const {
  if (hostContext_.log) {
    hostContext_.log(type, message);
  } else if (type == QtCriticalMsg || type == QtFatalMsg) {
    qCritical().noquote() << message;
  } else {
    qWarning().noquote() << message;
  }
}
