#include "item.h"
#include "search_engine.h"
#include "test_harness.h"
#include <array>
#include <vector>

namespace {

StringItem MakeUrlItem(const QString &name,
                       const std::vector<QString> &keywords,
                       const QString &url) {
  StringItem item;
  item.name = name;
  item.type = "url";
  item.keywords = keywords;
  item.payload = UrlPayload{url};
  return item;
}

StringItem MakeSnippetItem(const QString &name,
                           const std::vector<QString> &keywords,
                           const QString &snippet) {
  StringItem item;
  item.name = name;
  item.type = "snippet";
  item.keywords = keywords;
  item.payload = SnippetPayload{snippet};
  return item;
}

std::vector<SearchPrefixRule> DefaultPrefixes() {
  return {
      SearchPrefixRule{QString("snippet"), QString("s")},
      SearchPrefixRule{QString("url"), QString("u")},
  };
}

} // namespace

TEST(SearchEngineTest, EmptyQueryReturnsNoResults) {
  SearchEngine engine;
  engine.setItems(
      {MakeUrlItem("Open GitHub", {"git", "code"}, "https://github.com")});

  ASSERT_EQ(std::size_t(0), engine.search("").size());
  ASSERT_EQ(std::size_t(0), engine.search("   ").size());
}

TEST(SearchEngineTest, ExactNameGetsHighestPriority) {
  SearchEngine engine;
  engine.setItems({
      MakeUrlItem("chrome", {"browser"}, "https://example.com/chrome"),
      MakeUrlItem("Chromium docs", {"chrome"}, "https://example.com/docs"),
  });

  const auto results = engine.search("chrome", 2);
  ASSERT_EQ(std::size_t(2), results.size());
  ASSERT_EQ(QString("chrome"), results[0]->name);
}

TEST(SearchEngineTest, SearchIsCaseInsensitiveAndTrimmed) {
  SearchEngine engine;
  engine.setItems({
      MakeUrlItem("Open GitHub", {"git", "code"}, "https://github.com"),
      MakeUrlItem("linux", {"command"}, "https://linuxcommand.org"),
  });

  const auto results = engine.search("   GIT   ");
  ASSERT_EQ(std::size_t(1), results.size());
  ASSERT_EQ(QString("Open GitHub"), results[0]->name);
}

TEST(SearchEngineTest, TableDrivenRankingCases) {
  struct TestCase {
    QString query;
    QString expectedFirst;
    std::size_t k;
  };

  SearchEngine engine;
  engine.setItems({
      MakeUrlItem("chrome", {"browser"}, "https://example.com/chrome"),
      MakeUrlItem("Chromium docs", {"chrome"}, "https://example.com/docs"),
      MakeUrlItem("git cheatsheet", {"git", "code"}, "https://example.com/git"),
  });

  const std::array<TestCase, 3> cases = {
      TestCase{QString("chrome"), QString("chrome"), 3},
      TestCase{QString(" CHROME "), QString("chrome"), 3},
      TestCase{QString("git"), QString("git cheatsheet"), 3},
  };

  for (const auto &tc : cases) {
    const auto results = engine.search(tc.query, tc.k);
    ASSERT_NE(std::size_t(0), results.size());
    ASSERT_EQ(tc.expectedFirst, results[0]->name);
  }
}

TEST(SearchEngineTest, TopKIsRespected) {
  SearchEngine engine;
  engine.setItems({
      MakeUrlItem("chrome", {"browser"}, "https://example.com/1"),
      MakeUrlItem("chrome docs", {"docs"}, "https://example.com/2"),
      MakeUrlItem("chrome extensions", {"plugins"}, "https://example.com/3"),
  });

  const auto results = engine.search("chrome", 2);
  ASSERT_EQ(std::size_t(2), results.size());
}

TEST(SearchEngineTest, QueryWithoutMatchesReturnsEmpty) {
  SearchEngine engine;
  engine.setItems({
      MakeUrlItem("Open GitHub", {"git"}, "https://github.com"),
      MakeUrlItem("linux", {"command"}, "https://linuxcommand.org"),
  });

  ASSERT_EQ(std::size_t(0), engine.search("nonexistent").size());
}

TEST(SearchEngineTest, ZeroKReturnsNoResults) {
  SearchEngine engine;
  engine.setItems({MakeUrlItem("Open GitHub", {"git"}, "https://github.com")});

  ASSERT_EQ(std::size_t(0), engine.search("git", 0).size());
}

TEST(SearchEngineTest, PrefixFiltersSnippetsAndUsesRemainingQuery) {
  SearchEngine engine;
  engine.setSearchPrefixes(DefaultPrefixes());
  engine.setItems({
      MakeUrlItem("GitHub", {"git"}, "https://github.com"),
      MakeSnippetItem("Git Snippet", {"git"}, "git status"),
      MakeSnippetItem("Docker Snippet", {"docker"}, "docker ps"),
  });

  const auto results = engine.search("s git", 3);
  ASSERT_EQ(std::size_t(1), results.size());
  ASSERT_EQ(QString("Git Snippet"), results[0]->name);
}

TEST(SearchEngineTest, PrefixFiltersUrls) {
  SearchEngine engine;
  engine.setSearchPrefixes(DefaultPrefixes());
  engine.setItems({
      MakeUrlItem("GitHub", {"git"}, "https://github.com"),
      MakeSnippetItem("Git Snippet", {"git"}, "git status"),
  });

  const auto results = engine.search("u git", 3);
  ASSERT_EQ(std::size_t(1), results.size());
  ASSERT_EQ(QString("GitHub"), results[0]->name);
}

TEST(SearchEngineTest, PrefixWithEmptyQueryShowsMatchingType) {
  SearchEngine engine;
  engine.setSearchPrefixes(DefaultPrefixes());
  engine.setItems({
      MakeSnippetItem("Beta Snippet", {"beta"}, "beta"),
      MakeUrlItem("GitHub", {"git"}, "https://github.com"),
      MakeSnippetItem("Alpha Snippet", {"alpha"}, "alpha"),
  });

  const auto results = engine.search("s ", 5);
  ASSERT_EQ(std::size_t(2), results.size());
  ASSERT_EQ(QString("Alpha Snippet"), results[0]->name);
  ASSERT_EQ(QString("Beta Snippet"), results[1]->name);
}

TEST(SearchEngineTest, UnknownPrefixBehavesLikeOrdinarySearch) {
  SearchEngine engine;
  engine.setSearchPrefixes(DefaultPrefixes());
  engine.setItems({
      MakeUrlItem("x git", {"literal"}, "https://example.com"),
      MakeSnippetItem("Git Snippet", {"git"}, "git status"),
  });

  const auto results = engine.search("x git", 3);
  ASSERT_EQ(std::size_t(1), results.size());
  ASSERT_EQ(QString("x git"), results[0]->name);
}

TEST(SearchEngineTest, CustomPrefixesOverrideDefaults) {
  SearchEngine engine;
  engine.setSearchPrefixes({
      SearchPrefixRule{QString("snippet"), QString("clip")},
      SearchPrefixRule{QString("url"), QString("web")},
  });
  engine.setItems({
      MakeUrlItem("GitHub", {"git"}, "https://github.com"),
      MakeSnippetItem("Git Snippet", {"git"}, "git status"),
  });

  const auto results = engine.search("clip git", 3);
  ASSERT_EQ(std::size_t(1), results.size());
  ASSERT_EQ(QString("Git Snippet"), results[0]->name);
  ASSERT_EQ(std::size_t(0), engine.search("s git", 3).size());
}

int main() { return RUN_ALL_TESTS(); }
