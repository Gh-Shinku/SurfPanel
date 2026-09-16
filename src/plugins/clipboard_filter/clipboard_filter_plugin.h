#pragma once

#include "plugin/plugin.h"
#include "text_transformer.h"

#include <QObject>
#include <memory>
#include <optional>
#include <vector>

struct ClipboardFilterConfig {
  bool enabled = false;
  std::vector<QString> sourceProcesses;
};

enum class ClipboardReadStatus {
  Ready,
  Busy,
  Unavailable,
};

struct ClipboardContent {
  QString ownerProcessName;
  QString unicodeText;
  quint32 sequenceNumber = 0;
  bool hasUnicodeText = false;
};

struct ClipboardReadResult {
  ClipboardReadStatus status = ClipboardReadStatus::Unavailable;
  ClipboardContent content;
};

struct ClipboardUpdateContext {
  QString sourceProcessName;
  quint32 sequenceNumber = 0;
};

class SourceMatcher {
public:
  void setSourceProcesses(const std::vector<QString> &sourceProcesses);
  bool matches(const QString &processName) const;
  bool empty() const;

private:
  std::vector<QString> sourceProcesses_;
};

enum class ClipboardWriteResult { Written, Busy, Changed, Failed };

class ClipboardBackend {
public:
  virtual ~ClipboardBackend() = default;
  virtual ClipboardReadResult read() = 0;
  virtual ClipboardWriteResult writeUnicodeText(const QString &text,
                                                quint32 expectedSequence,
                                                quint32 *sequenceNumber) = 0;
};

enum class ClipboardProcessResult {
  Ignored,
  Retry,
  WriteFailed,
  Written,
  NoText,
  Changed,
};

class ClipboardProcessor {
public:
  explicit ClipboardProcessor(const TextTransformer &transformer);

  void setConfiguration(const ClipboardFilterConfig &config);
  ClipboardProcessResult process(
      ClipboardBackend *backend,
      const std::optional<ClipboardUpdateContext> &updateContext = std::nullopt,
      bool manual = false,
      std::optional<quint32> expectedSequence = std::nullopt);

private:
  SourceMatcher sourceMatcher_;
  const TextTransformer &transformer_;
  bool enabled_ = false;
  std::optional<quint32> selfWrittenSequence_;
};

class ClipboardFilterPlugin final : public QObject, public IPlugin {
public:
  using BackendFactory = std::function<std::unique_ptr<ClipboardBackend>(WId)>;
  explicit ClipboardFilterPlugin(BackendFactory backendFactory = {});
  ~ClipboardFilterPlugin() override;

  PluginMetadata metadata() const override;
  std::vector<PluginFunction> functions() const override;
  void invokeFunction(const QString &name, const PluginHostContext &context,
                      PluginFunctionCompletion completion) override;
  PluginConfigurationResult
  configure(const PluginConfigurationContext &context) override;
  bool start(const PluginHostContext &context) override;
  void stop() override;
  bool handleNativeEvent(const QByteArray &eventType, void *message,
                         qintptr *result) override;

private:
  void scheduleProcessing(int attempt, ClipboardUpdateContext updateContext);
  void log(QtMsgType type, const QString &message) const;
  void processManual(int attempt, quint32 expectedSequence);
  void finishManual(PluginFunctionResult result);

  ClipboardFilterConfig config_;
  PdfTextTransformer transformer_;
  ClipboardProcessor processor_;
  std::unique_ptr<ClipboardBackend> backend_;
  PluginHostContext hostContext_;
  bool listening_ = false;
  bool processing_ = false;
  int generation_ = 0;
  std::unique_ptr<ClipboardBackend> manualBackend_;
  PluginFunctionCompletion manualCompletion_;
  BackendFactory backendFactory_;
};
