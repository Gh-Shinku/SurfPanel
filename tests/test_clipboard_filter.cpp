#include "plugins/clipboard_filter/clipboard_filter_plugin.h"
#include "test_harness.h"

#include <filesystem>
#include <fstream>
#include <utility>

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
  quint32 writtenSequence = 99;
  int readCallCount = 0;
  int writeCallCount = 0;
  QString writtenText;

  ClipboardReadResult read() override {
    ++readCallCount;
    return readResult;
  }

  bool writeUnicodeText(const QString &text, quint32 *sequenceNumber) override {
    ++writeCallCount;
    writtenText = text;
    if (writeResult && sequenceNumber != nullptr) {
      *sequenceNumber = writtenSequence;
    }
    return writeResult;
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

TEST(ClipboardFilterTest,
     IdentityTransformAndFailedWritesLeaveClipboardUntouched) {
  IdentityTextTransformer identity;
  ClipboardProcessor identityProcessor(identity);
  identityProcessor.setConfiguration(EnabledForSumatra());
  FakeClipboardBackend backend;
  backend.readResult = ReadyText("SumatraPDF.exe", "copied text");

  ASSERT_EQ(ClipboardProcessResult::Ignored,
            identityProcessor.process(&backend));
  ASSERT_EQ(0, backend.writeCallCount);

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

int main() { return RUN_ALL_TESTS(); }
