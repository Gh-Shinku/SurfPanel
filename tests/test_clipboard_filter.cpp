#include "clipboard_filter.h"
#include "test_harness.h"

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

} // namespace

TEST(ClipboardFilterTest, SourceMatcherIsCaseInsensitive) {
  SourceMatcher matcher;
  matcher.setSourceProcesses({"SumatraPDF.exe"});

  ASSERT_TRUE(matcher.matches("sumatrapdf.EXE"));
  ASSERT_TRUE(!matcher.matches("notepad.exe"));
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

int main() { return RUN_ALL_TESTS(); }
