#include "action_manager.h"
#include "test_harness.h"
#include <QObject>
#include <QString>
#include <QVariant>

namespace {

class FakeActionContext final : public ActionContext {
public:
  bool openUrlResult = true;
  bool clipboardResult = true;
  bool injectResult = true;

  int openUrlCallCount = 0;
  int clipboardCallCount = 0;
  int injectCallCount = 0;

  QUrl lastUrl;
  QString lastClipboardText;
  QString lastInjectedText;

  bool openUrlInDefaultBrowser(const QUrl &url) override {
    ++openUrlCallCount;
    lastUrl = url;
    return openUrlResult;
  }

  bool copyToClipboard(const QString &text) override {
    ++clipboardCallCount;
    lastClipboardText = text;
    return clipboardResult;
  }

  bool injectIntoActiveInput(const QString &text) override {
    ++injectCallCount;
    lastInjectedText = text;
    return injectResult;
  }
};

} // namespace

TEST(ActionManagerTest, RegisterAndInvokeOpenUrlAction) {
  ActionManager manager;
  FakeActionContext context;

  ASSERT_TRUE(manager.registerAction(ActionManager::kOpenUrlAction,
                                     std::make_unique<OpenUrlAction>()));
  ASSERT_TRUE(manager.invoke(ActionManager::kOpenUrlAction,
                             "https://github.com", context));

  ASSERT_EQ(1, context.openUrlCallCount);
  ASSERT_EQ(QString("https"), context.lastUrl.scheme());
  ASSERT_EQ(QString("github.com"), context.lastUrl.host());
}

TEST(ActionManagerTest, RejectsDuplicateActionName) {
  ActionManager manager;

  ASSERT_TRUE(manager.registerAction("any", std::make_unique<OpenUrlAction>()));
  ASSERT_TRUE(
      !manager.registerAction("any", std::make_unique<OpenUrlAction>()));
}

TEST(ActionManagerTest, UnknownActionReturnsFalse) {
  ActionManager manager;
  FakeActionContext context;

  ASSERT_TRUE(!manager.invoke("missing", "payload", context));
}

TEST(ActionManagerTest, OpenUrlActionRejectsEmptyPayload) {
  ActionManager manager;
  FakeActionContext context;

  ASSERT_TRUE(manager.registerAction(ActionManager::kOpenUrlAction,
                                     std::make_unique<OpenUrlAction>()));
  ASSERT_TRUE(!manager.invoke(ActionManager::kOpenUrlAction, "   ", context));
  ASSERT_EQ(0, context.openUrlCallCount);
}

TEST(ActionManagerTest, InjectActionWritesClipboardAndInput) {
  ActionManager manager;
  FakeActionContext context;

  ASSERT_TRUE(manager.registerAction(ActionManager::kInjectContentAction,
                                     std::make_unique<InjectContentAction>()));
  ASSERT_TRUE(manager.invoke(ActionManager::kInjectContentAction, "hello world",
                             context));

  ASSERT_EQ(1, context.clipboardCallCount);
  ASSERT_EQ(1, context.injectCallCount);
  ASSERT_EQ(QString("hello world"), context.lastClipboardText);
  ASSERT_EQ(QString("hello world"), context.lastInjectedText);
}

TEST(ActionManagerTest, InjectActionFailsIfInputInjectionFails) {
  ActionManager manager;
  FakeActionContext context;
  context.injectResult = false;

  ASSERT_TRUE(manager.registerAction(ActionManager::kInjectContentAction,
                                     std::make_unique<InjectContentAction>()));
  ASSERT_TRUE(
      !manager.invoke(ActionManager::kInjectContentAction, "content", context));

  ASSERT_EQ(1, context.clipboardCallCount);
  ASSERT_EQ(1, context.injectCallCount);
}

TEST(ActionManagerTest, RegisterDefaultActionsAddsBothActions) {
  ActionManager manager;

  RegisterDefaultActions(&manager);

  ASSERT_TRUE(manager.hasAction(ActionManager::kOpenUrlAction));
  ASSERT_TRUE(manager.hasAction(ActionManager::kInjectContentAction));
}

TEST(ActionManagerTest, InjectIntoInputObjectAppendsDynamicTextProperty) {
  QObject target;
  target.setProperty("text", QVariant(QString("prefix_")));

  ASSERT_TRUE(InjectIntoInputObject(&target, "suffix"));
  ASSERT_EQ(QString("prefix_suffix"), target.property("text").toString());
}

int main() { return RUN_ALL_TESTS(); }