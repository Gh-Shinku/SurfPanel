#ifndef SURFPANEL_TEST_HARNESS_H
#define SURFPANEL_TEST_HARNESS_H

#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace test {

using TestFn = void (*)();

struct TestCase {
  const char *suite;
  const char *name;
  TestFn fn;
};

inline std::vector<TestCase> &Registry() {
  static std::vector<TestCase> tests;
  return tests;
}

inline bool RegisterTest(const char *suite, const char *name, TestFn fn) {
  Registry().push_back({suite, name, fn});
  return true;
}

inline void Fail(const char *file, int line, const std::string &message) {
  std::ostringstream oss;
  oss << file << ":" << line << ": " << message;
  throw std::runtime_error(oss.str());
}

inline void AssertTrue(bool condition, const char *expr, const char *file,
                       int line) {
  if (!condition) {
    Fail(file, line, std::string("Assertion failed: ") + expr);
  }
}

template <typename L, typename R>
inline void AssertEq(const L &lhs, const R &rhs, const char *lhsExpr,
                     const char *rhsExpr, const char *file, int line) {
  if (!(lhs == rhs)) {
    std::ostringstream oss;
    oss << "Assertion failed: " << lhsExpr << " == " << rhsExpr;
    Fail(file, line, oss.str());
  }
}

template <typename L, typename R>
inline void AssertNe(const L &lhs, const R &rhs, const char *lhsExpr,
                     const char *rhsExpr, const char *file, int line) {
  if (lhs == rhs) {
    std::ostringstream oss;
    oss << "Assertion failed: " << lhsExpr << " != " << rhsExpr;
    Fail(file, line, oss.str());
  }
}

inline void AssertContains(const std::string &haystack,
                           const std::string &needle, const char *file,
                           int line) {
  if (haystack.find(needle) == std::string::npos) {
    std::ostringstream oss;
    oss << "Expected substring not found. substring='" << needle << "'";
    Fail(file, line, oss.str());
  }
}

inline int RunAllTests() {
  int failed = 0;
  std::fprintf(stderr, "==== Running %zu tests ====\n", Registry().size());

  for (const auto &testCase : Registry()) {
    try {
      testCase.fn();
      std::fprintf(stderr, "[       OK ] %s.%s\n", testCase.suite,
                   testCase.name);
    } catch (const std::exception &e) {
      ++failed;
      std::fprintf(stderr, "[  FAILED  ] %s.%s\n", testCase.suite,
                   testCase.name);
      std::fprintf(stderr, "             %s\n", e.what());
    }
  }

  const int passed = static_cast<int>(Registry().size()) - failed;
  std::fprintf(stderr, "==== Passed: %d Failed: %d ====\n", passed, failed);
  return failed == 0 ? 0 : 1;
}

} // namespace test

#define TEST(suite, name)                                                      \
  static void suite##_##name();                                                \
  static const bool suite##_##name##_registered =                              \
      ::test::RegisterTest(#suite, #name, &suite##_##name);                    \
  static void suite##_##name()

#define ASSERT_TRUE(condition)                                                 \
  ::test::AssertTrue((condition), #condition, __FILE__, __LINE__)

#define ASSERT_EQ(lhs, rhs)                                                    \
  ::test::AssertEq((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

#define ASSERT_NE(lhs, rhs)                                                    \
  ::test::AssertNe((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

#define ASSERT_CONTAINS(haystack, needle)                                      \
  ::test::AssertContains((haystack), (needle), __FILE__, __LINE__)

#define RUN_ALL_TESTS() ::test::RunAllTests()

#endif // SURFPANEL_TEST_HARNESS_H