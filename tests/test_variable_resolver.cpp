#include "test_harness.h"
#include "variable_resolver.h"
#include <QDate>
#include <QDateTime>
#include <QLocale>
#include <QTime>

namespace {

const QDateTime kFixedTime(QDate(2026, 5, 17), QTime(9, 8, 7));

VariableSettings FixedSettings() {
  VariableSettings settings;
  settings.locale = QLocale(QLocale::Chinese, QLocale::China);
  return settings;
}

} // namespace

TEST(VariableResolverTest, ReplacesDate) {
  ASSERT_EQ(QString("today=2026/05/17"),
            ResolveVariables("today={{date}}", kFixedTime));
}

TEST(VariableResolverTest, ReplacesTime) {
  ASSERT_EQ(QString("time=09:08:07"),
            ResolveVariables("time={{time}}", kFixedTime));
}

TEST(VariableResolverTest, ReplacesDateTime) {
  ASSERT_EQ(QString("at 2026/05/17 09:08:07"),
            ResolveVariables("at {{datetime}}", kFixedTime));
}

TEST(VariableResolverTest, ReplacesMultipleVariables) {
  ASSERT_EQ(QString("2026/05/17 09:08:07 09:08:07"),
            ResolveVariables("{{date}} {{time}} {{time}}", kFixedTime));
}

TEST(VariableResolverTest, LeavesUnknownVariablesUnchanged) {
  ASSERT_EQ(QString("project={{project}}"),
            ResolveVariables("project={{project}}", kFixedTime));
}

TEST(VariableResolverTest, LeavesPlainTextUnchanged) {
  ASSERT_EQ(QString("plain text"), ResolveVariables("plain text", kFixedTime));
}

TEST(VariableResolverTest, AppliesConfiguredDefaultFormats) {
  VariableSettings settings = FixedSettings();
  settings.dateFormat = "yyyy-MM-dd";
  settings.timeFormat = "HH:mm";
  settings.dateTimeFormat = "yyyy-MM-dd'T'HH:mm";

  ASSERT_EQ(
      QString("2026-05-17 09:08 2026-05-17T09:08"),
      ResolveVariables("{{date}} {{time}} {{datetime}}", kFixedTime, settings));
}

TEST(VariableResolverTest, AppliesInlineDateFormats) {
  ASSERT_EQ(
      QString("2026-05-17"),
      ResolveVariables("{{date:yyyy-MM-dd}}", kFixedTime, FixedSettings()));
}

TEST(VariableResolverTest, AppliesInlineFormatsContainingColons) {
  ASSERT_EQ(QString("09:08 09:08:07"),
            ResolveVariables("{{time:HH:mm}} {{datetime:HH:mm:ss}}", kFixedTime,
                             FixedSettings()));
}

TEST(VariableResolverTest, KeepsQuotedLiteralsInInlineFormats) {
  ASSERT_EQ(QString("2026-05-17T09:08"),
            ResolveVariables("{{datetime:yyyy-MM-dd'T'HH:mm}}", kFixedTime,
                             FixedSettings()));
}

TEST(VariableResolverTest, SupportsNonAsciiInlineFormats) {
  ASSERT_EQ(
      QString("2026年5月17日"),
      ResolveVariables("{{date:yyyy年M月d日}}", kFixedTime, FixedSettings()));
}

TEST(VariableResolverTest, UsesLocaleNamesForWeekdayAndMonth) {
  ASSERT_EQ(QString("2026年5月17日 星期日"),
            ResolveVariables("{{date:yyyy年M月d日 dddd}}", kFixedTime,
                             FixedSettings()));
}

TEST(VariableResolverTest, EmptyInlineFormatFallsBackToDefault) {
  VariableSettings settings = FixedSettings();
  settings.dateFormat = "yyyy-MM-dd";

  ASSERT_EQ(QString("2026-05-17"),
            ResolveVariables("{{date:}}", kFixedTime, settings));
}

TEST(VariableResolverTest, UnsupportedInlineFormatFallsBackToDefault) {
  VariableSettings settings = FixedSettings();
  settings.dateFormat = "yyyy-MM-dd";

  ASSERT_EQ(QString("2026-05-17"),
            ResolveVariables("{{date:%Y-%m-%d}}", kFixedTime, settings));
}

TEST(VariableResolverTest, DoesNotMatchVariableNamesWithSuffix) {
  ASSERT_EQ(QString("{{dates}} {{datetime_extra}} {{DATE}}"),
            ResolveVariables("{{dates}} {{datetime_extra}} {{DATE}}",
                             kFixedTime, FixedSettings()));
}

TEST(VariableResolverTest, RejectsFormatsWithoutDateTimeFields) {
  QString reason;
  ASSERT_TRUE(FormatCheck::Invalid ==
              CheckDateTimeFormat("plain text", &reason));
  ASSERT_CONTAINS(reason.toStdString(), "no date or time fields");
}

TEST(VariableResolverTest, RejectsEmptyFormats) {
  QString reason;
  ASSERT_TRUE(FormatCheck::Invalid == CheckDateTimeFormat("  ", &reason));
  ASSERT_CONTAINS(reason.toStdString(), "empty");
}

TEST(VariableResolverTest, RejectsStrftimeFormats) {
  QString reason;
  ASSERT_TRUE(FormatCheck::Invalid == CheckDateTimeFormat("%Y-%m-%d", &reason));
  ASSERT_CONTAINS(reason.toStdString(), "strftime");
}

TEST(VariableResolverTest, FlagsLowercaseMonthAsMinutesTypo) {
  QString reason;
  ASSERT_TRUE(FormatCheck::Suspect ==
              CheckDateTimeFormat("yyyy-mm-dd", &reason));
  ASSERT_CONTAINS(reason.toStdString(), "lowercase 'm' means minutes");

  ASSERT_TRUE(FormatCheck::Ok == CheckDateTimeFormat("yyyy-MM-dd", &reason));
  ASSERT_TRUE(FormatCheck::Ok ==
              CheckDateTimeFormat("yyyy-MM-dd HH:mm", &reason));
  ASSERT_TRUE(FormatCheck::Ok ==
              CheckDateTimeFormat("'m' yyyy-MM-dd", &reason));
}

int main() { return RUN_ALL_TESTS(); }
