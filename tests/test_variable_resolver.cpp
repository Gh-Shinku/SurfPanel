#include "test_harness.h"
#include "variable_resolver.h"
#include <QDate>
#include <QDateTime>
#include <QTime>

namespace {

const QDateTime kFixedTime(QDate(2026, 5, 17), QTime(9, 8, 7));

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

int main() { return RUN_ALL_TESTS(); }
