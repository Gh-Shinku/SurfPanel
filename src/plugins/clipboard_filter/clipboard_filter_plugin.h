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

class SourceMatcher {
public:
  void setSourceProcesses(const std::vector<QString> &sourceProcesses);
  bool matches(const QString &processName) const;
  bool empty() const;

private:
  std::vector<QString> sourceProcesses_;
};

class ClipboardBackend {
public:
  virtual ~ClipboardBackend() = default;
  virtual ClipboardReadResult read() = 0;
  virtual bool writeUnicodeText(const QString &text,
                                quint32 *sequenceNumber) = 0;
};

enum class ClipboardProcessResult {
  Ignored,
  Retry,
  WriteFailed,
  Written,
};

class ClipboardProcessor {
public:
  explicit ClipboardProcessor(const TextTransformer &transformer);

  void setConfiguration(const ClipboardFilterConfig &config);
  ClipboardProcessResult process(ClipboardBackend *backend);

private:
  SourceMatcher sourceMatcher_;
  const TextTransformer &transformer_;
  bool enabled_ = false;
  std::optional<quint32> selfWrittenSequence_;
};

class ClipboardFilterPlugin final : public QObject, public IPlugin {
public:
  ClipboardFilterPlugin();
  ~ClipboardFilterPlugin() override;

  PluginMetadata metadata() const override;
  PluginConfigurationResult
  configure(const PluginConfigurationContext &context) override;
  bool start(const PluginHostContext &context) override;
  void stop() override;
  bool handleNativeEvent(const QByteArray &eventType, void *message,
                         qintptr *result) override;

private:
  void scheduleProcessing(int attempt);
  void log(QtMsgType type, const QString &message) const;

  ClipboardFilterConfig config_;
  PdfTextTransformer transformer_;
  ClipboardProcessor processor_;
  std::unique_ptr<ClipboardBackend> backend_;
  PluginHostContext hostContext_;
  bool listening_ = false;
  bool processing_ = false;
  int generation_ = 0;
};
