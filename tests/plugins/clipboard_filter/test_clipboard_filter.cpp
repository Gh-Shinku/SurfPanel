#include "plugins/clipboard_filter/clipboard_filter_plugin.h"
#include "plugins/host/plugin_manager.h"
#include "support/test_harness.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QThread>
#include <QWindow>
#include <filesystem>
#include <fstream>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

class SuffixTransformer final : public TextTransformer {
public:
  QString transform(const QString &text) const override {
    return text + "_normalized";
  }
};

class FakeClipboardBackend final : public ClipboardBackend {
public:
  ClipboardReadResult readResult;
  bool writeResult = true;
  ClipboardWriteResult nextWriteResult = ClipboardWriteResult::Written;
  quint32 expectedWriteSequence = 0;
  quint32 writtenSequence = 99;
  int readCallCount = 0;
  int writeCallCount = 0;
  QString writtenText;

  ClipboardReadResult read() override {
    ++readCallCount;
    return readResult;
  }

  ClipboardWriteResult writeUnicodeText(const QString &text,
                                        quint32 expectedSequence,
                                        quint32 *sequenceNumber) override {
    ++writeCallCount;
    expectedWriteSequence = expectedSequence;
    if (!writeResult) {
      return ClipboardWriteResult::Failed;
    }
    if (nextWriteResult != ClipboardWriteResult::Written) {
      return nextWriteResult;
    }
    writtenText = text;
    if (writeResult && sequenceNumber != nullptr) {
      *sequenceNumber = writtenSequence;
    }
    return ClipboardWriteResult::Written;
  }
};

ClipboardReadResult ReadyText(const QString &owner, const QString &text,
                              quint32 sequence = 1) {
  ClipboardReadResult result;
  result.status = ClipboardReadStatus::Ready;
  result.content.ownerProcessName = owner;
  result.content.unicodeText = text;
  result.content.sequenceNumber = sequence;
  result.content.hasUnicodeText = true;
  return result;
}

ClipboardFilterConfig EnabledForSumatra() {
  ClipboardFilterConfig config;
  config.enabled = true;
  config.sourceProcesses = {"SumatraPDF.exe"};
  return config;
}

namespace fs = std::filesystem;

struct PluginConfigFixture {
  fs::path root = fs::temp_directory_path() / "surfpanel_clipboard_plugin_test";

  PluginConfigFixture() {
    fs::remove_all(root);
    fs::create_directories(root / "plugins");
  }

  ~PluginConfigFixture() { fs::remove_all(root); }

  void write(const fs::path &relativePath, const std::string &content) {
    fs::create_directories((root / relativePath).parent_path());
    std::ofstream output(root / relativePath);
    output << content;
  }

  PluginConfigurationContext context() const {
    return {root, root / "plugins" / "clipboard-filter.toml",
            root / "items.toml"};
  }
};

} // namespace

TEST(ClipboardFilterTest, SourceMatcherIsCaseInsensitive) {
  SourceMatcher matcher;
  matcher.setSourceProcesses({"SumatraPDF.exe"});

  ASSERT_TRUE(matcher.matches("sumatrapdf.EXE"));
  ASSERT_TRUE(!matcher.matches("notepad.exe"));
}

TEST(ClipboardFilterTest, PdfTransformerMergesInlineBreaks) {
  PdfTextTransformer transformer;

  ASSERT_EQ(QString("The first line continues here. Next sentence."),
            transformer.transform(
                "The first line\r\ncontinues here.\nNext sentence."));
  ASSERT_EQ(QString::fromUtf8("第一行继续第二行。"),
            transformer.transform(QString::fromUtf8("第一行继续\n第二行。")));
}

TEST(ClipboardFilterTest, PdfTransformerMergesReportedSumatraExcerpt) {
  PdfTextTransformer transformer;
  const QString copied =
      "To quantify the synergy between these two primitives, we formulate the "
      "Sparsity Allocation\n"
      "problem: given a fixed total parameter budget, how should capacity be "
      "distributed between\n"
      "MoE experts and Engram memory? Our experiments uncover a distinct "
      "U-shaped scaling\n"
      "law, revealing that even simple lookup mechanisms, when treated as a "
      "first-class modeling\n"
      "primitive, act as essential complements to neural computation.";

  ASSERT_EQ(
      QString("To quantify the synergy between these two primitives, we "
              "formulate the Sparsity Allocation problem: given a fixed total "
              "parameter budget, how should capacity be distributed between "
              "MoE experts and Engram memory? Our experiments uncover a "
              "distinct U-shaped scaling law, revealing that even simple "
              "lookup mechanisms, when treated as a first-class modeling "
              "primitive, act as essential complements to neural computation."),
      transformer.transform(copied));
}

TEST(ClipboardFilterTest, PdfTransformerPreservesParagraphBreaks) {
  PdfTextTransformer transformer;

  ASSERT_EQ(QString::fromUtf8("First paragraph continues.\n\n第二段继续。"),
            transformer.transform(QString::fromUtf8(
                "First paragraph\r\ncontinues.\r\n\r\n第二段\r\n继续。")));
}

TEST(ClipboardFilterTest, PdfTransformerRepairsEnglishHyphenation) {
  PdfTextTransformer transformer;

  ASSERT_EQ(QString("A multiline example."),
            transformer.transform("A multi-\nline exam-\nple."));
  ASSERT_EQ(QString("ISO-Standard"), transformer.transform("ISO-\nStandard"));
}

TEST(ClipboardFilterTest, PdfTransformerRemovesCjkLatinSpacing) {
  PdfTextTransformer transformer;

  ASSERT_EQ(QString::fromUtf8("在Qt 6中使用SumatraPDF阅读PDF文档。"),
            transformer.transform(QString::fromUtf8(
                "在 Qt 6 中使用 SumatraPDF\n阅读 PDF 文档。")));
}

TEST(ClipboardFilterTest, PdfTransformerKeepsPunctuationAttached) {
  PdfTextTransformer transformer;

  ASSERT_EQ(QString("A wrapped sentence, with punctuation."),
            transformer.transform("A wrapped sentence\n, with punctuation."));
}

TEST(ClipboardFilterTest, MatchingTextIsTransformedAndWritten) {
  SuffixTransformer transformer;
  ClipboardProcessor processor(transformer);
  processor.setConfiguration(EnabledForSumatra());
  FakeClipboardBackend backend;
  backend.readResult = ReadyText("SumatraPDF.exe", "copied text");

  ASSERT_EQ(ClipboardProcessResult::Written, processor.process(&backend));
  ASSERT_EQ(1, backend.writeCallCount);
  ASSERT_EQ(QString("copied text_normalized"), backend.writtenText);
}

TEST(ClipboardFilterTest, EventTimeSourceRecoversOwnerlessClipboardWrites) {
  SuffixTransformer transformer;
  ClipboardProcessor processor(transformer);
  processor.setConfiguration(EnabledForSumatra());
  FakeClipboardBackend backend;
  backend.readResult = ReadyText({}, "copied text", 42);

  const ClipboardUpdateContext update{"SumatraPDF.exe", 42};

  ASSERT_EQ(ClipboardProcessResult::Written,
            processor.process(&backend, update));
  ASSERT_EQ(QString("copied text_normalized"), backend.writtenText);
}

TEST(ClipboardFilterTest, StaleEventSourceCannotClaimNewClipboardContent) {
  SuffixTransformer transformer;
  ClipboardProcessor processor(transformer);
  processor.setConfiguration(EnabledForSumatra());
  FakeClipboardBackend backend;
  backend.readResult = ReadyText({}, "new text", 43);

  const ClipboardUpdateContext staleUpdate{"SumatraPDF.exe", 42};

  ASSERT_EQ(ClipboardProcessResult::Ignored,
            processor.process(&backend, staleUpdate));
  ASSERT_EQ(0, backend.writeCallCount);
}

TEST(ClipboardFilterTest, PdfTextIsNormalizedBeforeClipboardWrite) {
  PdfTextTransformer transformer;
  ClipboardProcessor processor(transformer);
  processor.setConfiguration(EnabledForSumatra());
  FakeClipboardBackend backend;
  backend.readResult =
      ReadyText("SumatraPDF.exe", QString::fromUtf8("使用 PDF\n阅读文档"));

  ASSERT_EQ(ClipboardProcessResult::Written, processor.process(&backend));
  ASSERT_EQ(QString::fromUtf8("使用PDF阅读文档"), backend.writtenText);
}

TEST(ClipboardFilterTest, UnmatchedOwnerAndNonTextAreIgnored) {
  SuffixTransformer transformer;
  ClipboardProcessor processor(transformer);
  processor.setConfiguration(EnabledForSumatra());
  FakeClipboardBackend backend;

  backend.readResult = ReadyText("notepad.exe", "copied text");
  ASSERT_EQ(ClipboardProcessResult::Ignored, processor.process(&backend));
  ASSERT_EQ(0, backend.writeCallCount);

  backend.readResult = ReadyText("SumatraPDF.exe", "copied text");
  backend.readResult.content.hasUnicodeText = false;
  ASSERT_EQ(ClipboardProcessResult::Ignored, processor.process(&backend));
  ASSERT_EQ(0, backend.writeCallCount);
}

TEST(ClipboardFilterTest, BusyClipboardRequestsRetryWithoutWriting) {
  SuffixTransformer transformer;
  ClipboardProcessor processor(transformer);
  processor.setConfiguration(EnabledForSumatra());
  FakeClipboardBackend backend;
  backend.readResult.status = ClipboardReadStatus::Busy;

  ASSERT_EQ(ClipboardProcessResult::Retry, processor.process(&backend));
  ASSERT_EQ(0, backend.writeCallCount);
}

TEST(ClipboardFilterTest, UnchangedTextIsRepublishedOnce) {
  IdentityTextTransformer identity;
  ClipboardProcessor identityProcessor(identity);
  identityProcessor.setConfiguration(EnabledForSumatra());
  FakeClipboardBackend backend;
  backend.readResult = ReadyText("SumatraPDF.exe", "copied text");

  ASSERT_EQ(ClipboardProcessResult::Written,
            identityProcessor.process(&backend));
  ASSERT_EQ(QString("copied text"), backend.writtenText);
  ASSERT_EQ(1, backend.writeCallCount);

  backend.readResult.content.sequenceNumber = backend.writtenSequence;
  ASSERT_EQ(ClipboardProcessResult::Ignored,
            identityProcessor.process(&backend));
  ASSERT_EQ(1, backend.writeCallCount);
}

TEST(ClipboardFilterTest, FailedWritesLeaveClipboardUntouched) {
  FakeClipboardBackend backend;
  backend.readResult = ReadyText("SumatraPDF.exe", "copied text");
  SuffixTransformer transformer;
  ClipboardProcessor processor(transformer);
  processor.setConfiguration(EnabledForSumatra());
  backend.writeResult = false;
  ASSERT_EQ(ClipboardProcessResult::WriteFailed, processor.process(&backend));
}

TEST(ClipboardFilterTest, SelfWrittenSequenceIsIgnored) {
  SuffixTransformer transformer;
  ClipboardProcessor processor(transformer);
  processor.setConfiguration(EnabledForSumatra());
  FakeClipboardBackend backend;
  backend.writtenSequence = 42;
  backend.readResult = ReadyText("SumatraPDF.exe", "copied text", 1);

  ASSERT_EQ(ClipboardProcessResult::Written, processor.process(&backend));
  backend.readResult = ReadyText("SurfPanel.exe", "normalized", 42);
  ASSERT_EQ(ClipboardProcessResult::Ignored, processor.process(&backend));
  ASSERT_EQ(1, backend.writeCallCount);
}

TEST(ClipboardFilterTest, ManualFilteringBypassesSourceAndDisabledMonitoring) {
  PdfTextTransformer transformer;
  ClipboardProcessor processor(transformer);
  FakeClipboardBackend backend;
  backend.readResult = ReadyText("browser.exe", "A line\ncontinues.", 42);
  ASSERT_EQ(ClipboardProcessResult::Written,
            processor.process(&backend, std::nullopt, true, 42));
  ASSERT_EQ(QString("A line continues."), backend.writtenText);
  ASSERT_EQ(quint32(42), backend.expectedWriteSequence);

  backend.readResult = ReadyText("SurfPanel.exe", "Single line.", 99);
  ASSERT_EQ(ClipboardProcessResult::Written,
            processor.process(&backend, std::nullopt, true, 99));
  ASSERT_EQ(QString("Single line."), backend.writtenText);
}

TEST(ClipboardFilterTest, ManualFilteringRejectsEmptyAndNonText) {
  PdfTextTransformer transformer;
  ClipboardProcessor processor(transformer);
  FakeClipboardBackend backend;
  backend.readResult = ReadyText("browser.exe", "");
  ASSERT_EQ(ClipboardProcessResult::NoText,
            processor.process(&backend, std::nullopt, true, 1));
  backend.readResult.content.hasUnicodeText = false;
  ASSERT_EQ(ClipboardProcessResult::NoText,
            processor.process(&backend, std::nullopt, true, 1));
  ASSERT_EQ(0, backend.writeCallCount);
}

TEST(ClipboardFilterTest, ClipboardChangesAndWriteContentionAreReported) {
  PdfTextTransformer transformer;
  ClipboardProcessor processor(transformer);
  FakeClipboardBackend backend;
  backend.readResult = ReadyText("browser.exe", "Latest text", 43);
  ASSERT_EQ(ClipboardProcessResult::Changed,
            processor.process(&backend, std::nullopt, true, 42));
  ASSERT_EQ(0, backend.writeCallCount);
  backend.nextWriteResult = ClipboardWriteResult::Changed;
  ASSERT_EQ(ClipboardProcessResult::Changed,
            processor.process(&backend, std::nullopt, true, 43));
  backend.nextWriteResult = ClipboardWriteResult::Busy;
  ASSERT_EQ(ClipboardProcessResult::Retry,
            processor.process(&backend, std::nullopt, true, 43));
}

TEST(ClipboardFilterTest, MissingConfigurationDisablesPlugin) {
  PluginConfigFixture fixture;
  ClipboardFilterPlugin plugin;

  const auto result = plugin.configure(fixture.context());

  ASSERT_EQ(PluginConfigurationState::Disabled, result.state);
  ASSERT_TRUE(result.message.isEmpty());
}

TEST(ClipboardFilterTest, DedicatedConfigurationEnablesPlugin) {
  PluginConfigFixture fixture;
  fixture.write("plugins/clipboard-filter.toml", R"(enabled = true
source_processes = ["SumatraPDF.exe", "sumatrapdf.EXE"]
)");
  ClipboardFilterPlugin plugin;

  const auto result = plugin.configure(fixture.context());

  ASSERT_EQ(PluginConfigurationState::Enabled, result.state);
}

TEST(ClipboardFilterTest, InvalidDedicatedConfigurationIsIsolated) {
  PluginConfigFixture fixture;
  fixture.write("plugins/clipboard-filter.toml", R"(enabled = true
source_processes = []
)");
  ClipboardFilterPlugin plugin;

  const auto result = plugin.configure(fixture.context());

  ASSERT_EQ(PluginConfigurationState::Invalid, result.state);
  ASSERT_TRUE(result.message.contains("no valid source processes"));
}

TEST(ClipboardFilterTest, LegacyConfigurationIsAcceptedWithWarning) {
  PluginConfigFixture fixture;
  fixture.write("items.toml", R"([clipboard_filter]
enabled = true
source_processes = ["SumatraPDF.exe"]
)");
  ClipboardFilterPlugin plugin;

  const auto result = plugin.configure(fixture.context());

  ASSERT_EQ(PluginConfigurationState::Enabled, result.state);
  ASSERT_TRUE(result.message.contains("deprecated"));
}

TEST(ClipboardFilterTest, DedicatedConfigurationWinsOverLegacy) {
  PluginConfigFixture fixture;
  fixture.write("plugins/clipboard-filter.toml", "enabled = [\n");
  fixture.write("items.toml", R"([clipboard_filter]
enabled = true
source_processes = ["SumatraPDF.exe"]
)");
  ClipboardFilterPlugin plugin;

  const auto result = plugin.configure(fixture.context());

  ASSERT_EQ(PluginConfigurationState::Invalid, result.state);
  ASSERT_TRUE(!result.message.contains("deprecated"));
}

#ifdef Q_OS_WIN
TEST(ClipboardFilterTest, ManualFunctionWorksWithMonitoringDisabled) {
  PluginConfigFixture fixture;
  fixture.write("plugins/clipboard-filter.toml", "enabled = false\n");
  QWindow window;
  PluginManager manager;
  ASSERT_TRUE(
      manager.registerPlugin(std::make_unique<ClipboardFilterPlugin>()));
  PluginHostContext host;
  host.nativeWindow = window.winId();
  host.eventLoopOwner = &window;
  manager.initialize(host);
  ASSERT_TRUE(!manager.reload(fixture.root).hasErrors);
  ASSERT_TRUE(!manager.isActive("clipboard-filter"));
  auto *clipboard = QGuiApplication::clipboard();
  const auto previous = clipboard->text();
  clipboard->setText("A browser copy\ncontinues here.");
  PluginFunctionResult result;
  int completions = 0;
  manager.invokeFunction("clipboard-filter", "filter", [&](auto value) {
    result = value;
    ++completions;
  });
  QElapsedTimer completionTimer;
  completionTimer.start();
  while (completions == 0 && completionTimer.elapsed() < 1000) {
    QCoreApplication::processEvents();
    QThread::msleep(10);
  }
  const QString actual = clipboard->text();
  const bool ownedByHost =
      GetClipboardOwner() == reinterpret_cast<HWND>(host.nativeWindow);
  manager.shutdown();
  clipboard->setText(previous);
  ASSERT_TRUE(result.succeeded);
  ASSERT_EQ(1, completions);
  ASSERT_EQ(QString("A browser copy continues here."), actual);
  ASSERT_TRUE(ownedByHost);
}

TEST(ClipboardFilterTest, StopCancelsManualRetriesExactlyOnce) {
  ClipboardFilterPlugin plugin([](WId) {
    auto backend = std::make_unique<FakeClipboardBackend>();
    backend->readResult.status = ClipboardReadStatus::Busy;
    return backend;
  });
  PluginHostContext host;
  host.nativeWindow = 1;
  host.eventLoopOwner = &plugin;
  int completions = 0;
  bool succeeded = true;
  plugin.invokeFunction("filter", host, [&](auto result) {
    ++completions;
    succeeded = result.succeeded;
  });
  ASSERT_EQ(0, completions);
  plugin.stop();
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 150) {
    QCoreApplication::processEvents();
    QThread::msleep(10);
  }
  ASSERT_EQ(1, completions);
  ASSERT_TRUE(!succeeded);
}

TEST(ClipboardFilterTest, ManualRetryCancelsWhenClipboardSequenceChanges) {
  FakeClipboardBackend *backend = nullptr;
  ClipboardFilterPlugin plugin([&](WId) {
    auto value = std::make_unique<FakeClipboardBackend>();
    value->readResult.status = ClipboardReadStatus::Busy;
    backend = value.get();
    return value;
  });
  PluginHostContext host;
  host.nativeWindow = 1;
  host.eventLoopOwner = &plugin;
  int completions = 0;
  PluginFunctionResult result;
  const auto initialSequence = GetClipboardSequenceNumber();
  plugin.invokeFunction("filter", host, [&](auto value) {
    result = value;
    ++completions;
  });
  backend->readResult =
      ReadyText("browser.exe", "New content", initialSequence + 1);
  QElapsedTimer timer;
  timer.start();
  while (completions == 0 && timer.elapsed() < 1000) {
    QCoreApplication::processEvents();
    QThread::msleep(10);
  }
  ASSERT_EQ(1, completions);
  ASSERT_TRUE(!result.succeeded);
  ASSERT_TRUE(result.message.contains("changed"));
}

TEST(ClipboardFilterTest, ManualBusyRetriesEventuallyFail) {
  ClipboardFilterPlugin plugin([](WId) {
    auto backend = std::make_unique<FakeClipboardBackend>();
    backend->readResult.status = ClipboardReadStatus::Busy;
    return backend;
  });
  PluginHostContext host;
  host.nativeWindow = 1;
  host.eventLoopOwner = &plugin;
  int completions = 0;
  PluginFunctionResult result;
  plugin.invokeFunction("filter", host, [&](auto value) {
    result = value;
    ++completions;
  });
  QElapsedTimer timer;
  timer.start();
  while (completions == 0 && timer.elapsed() < 1000) {
    QCoreApplication::processEvents();
    QThread::msleep(10);
  }
  ASSERT_EQ(1, completions);
  ASSERT_TRUE(!result.succeeded);
  ASSERT_TRUE(result.message.contains("busy"));
}

TEST(ClipboardFilterTest, NativeListenerTransformsMatchingClipboardUpdates) {
  PluginConfigFixture fixture;
  const QString processName =
      QFileInfo(QCoreApplication::applicationFilePath()).fileName();
  fixture.write("plugins/clipboard-filter.toml",
                QString("enabled = true\nsource_processes = [\"%1\"]\n")
                    .arg(processName)
                    .toStdString());

  QWindow window;
  PluginHostContext host;
  host.eventLoopOwner = &window;
  host.nativeWindow = window.winId();
  PluginManager manager;
  ASSERT_TRUE(
      manager.registerPlugin(std::make_unique<ClipboardFilterPlugin>()));
  manager.initialize(host);
  const PluginReloadReport report = manager.reload(fixture.root);
  ASSERT_TRUE(!report.hasErrors);
  ASSERT_TRUE(manager.isActive("clipboard-filter"));

  QClipboard *clipboard = QGuiApplication::clipboard();
  const QString previousText = clipboard->text();
  clipboard->setText("A copied line\ncontinues here.");

  const QString expected = "A copied line continues here.";
  QElapsedTimer timer;
  timer.start();
  while (clipboard->text() != expected && timer.elapsed() < 1000) {
    QCoreApplication::processEvents();
    QThread::msleep(10);
  }

  const QString actual = clipboard->text();
  clipboard->setText("An unchanged single line.");
  const DWORD originalSequence = GetClipboardSequenceNumber();
  timer.restart();
  while (GetClipboardSequenceNumber() == originalSequence &&
         timer.elapsed() < 1000) {
    QCoreApplication::processEvents();
    QThread::msleep(10);
  }
  const bool republished = GetClipboardSequenceNumber() != originalSequence;
  const QString unchangedText = clipboard->text();
  const bool ownedByHost =
      GetClipboardOwner() == reinterpret_cast<HWND>(host.nativeWindow);
  manager.shutdown();
  clipboard->setText(previousText);
  ASSERT_EQ(expected, actual);
  ASSERT_TRUE(republished);
  ASSERT_TRUE(ownedByHost);
  ASSERT_EQ(QString("An unchanged single line."), unchangedText);
}
#endif

int main(int argc, char *argv[]) {
  QGuiApplication application(argc, argv);
  return RUN_ALL_TESTS();
}
