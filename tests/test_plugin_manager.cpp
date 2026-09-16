#include "plugin/plugin_manager.h"
#include "test_harness.h"

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

class FakePlugin final : public IPlugin {
public:
  explicit FakePlugin(QString id, PluginConfigurationState state,
                      std::vector<QString> *stopOrder = nullptr)
      : id_(std::move(id)), state_(state), stopOrder_(stopOrder) {}

  PluginMetadata metadata() const override {
    return PluginMetadata{id_, id_ + " name", kSurfPanelPluginApiVersion};
  }

  PluginConfigurationResult
  configure(const PluginConfigurationContext &context) override {
    ++configureCount;
    lastConfigPath = context.pluginConfigPath;
    return PluginConfigurationResult{state_, configurationMessage};
  }

  bool start(const PluginHostContext &) override {
    ++startCount;
    return startResult;
  }

  void stop() override {
    ++stopCount;
    if (stopOrder_ != nullptr) {
      stopOrder_->push_back(id_);
    }
  }

  bool handleNativeEvent(const QByteArray &, void *, qintptr *) override {
    ++eventCount;
    return handlesEvent;
  }

  std::vector<PluginFunction> functions() const override {
    return {{"run", "Run the test function."}};
  }
  void invokeFunction(const QString &, const PluginHostContext &,
                      PluginFunctionCompletion completion) override {
    ++functionCount;
    if (throwFunction) {
      throw std::runtime_error("test failure");
    }
    if (deferFunction) {
      pendingCompletion = std::move(completion);
    } else {
      completion({true, {}});
      completion({true, {}});
    }
  }
  bool deferFunction = false;
  bool throwFunction = false;
  int functionCount = 0;
  PluginFunctionCompletion pendingCompletion;

  PluginConfigurationState state_;
  QString configurationMessage;
  bool startResult = true;
  bool handlesEvent = false;
  int configureCount = 0;
  int startCount = 0;
  int stopCount = 0;
  int eventCount = 0;
  std::filesystem::path lastConfigPath;

private:
  QString id_;
  std::vector<QString> *stopOrder_;
};

PluginHostContext HostContext() {
  PluginHostContext context;
  context.nativeWindow = 1;
  return context;
}

} // namespace

TEST(PluginManagerTest, RejectsInvalidAndDuplicatePluginIds) {
  PluginManager manager;
  ASSERT_TRUE(manager.registerPlugin(std::make_unique<FakePlugin>(
      "valid-plugin", PluginConfigurationState::Disabled)));
  ASSERT_TRUE(!manager.registerPlugin(std::make_unique<FakePlugin>(
      "valid-plugin", PluginConfigurationState::Disabled)));
  ASSERT_TRUE(!manager.registerPlugin(std::make_unique<FakePlugin>(
      "Invalid_Plugin", PluginConfigurationState::Disabled)));
  ASSERT_EQ(std::size_t(1), manager.pluginCount());
}

TEST(PluginManagerTest, ReloadStartsEnabledAndStopsDisabledPlugins) {
  PluginManager manager;
  auto plugin =
      std::make_unique<FakePlugin>("demo", PluginConfigurationState::Enabled);
  FakePlugin *rawPlugin = plugin.get();
  ASSERT_TRUE(manager.registerPlugin(std::move(plugin)));
  manager.initialize(HostContext());

  const auto first = manager.reload("C:/config");
  ASSERT_TRUE(!first.hasErrors);
  ASSERT_TRUE(manager.isActive("demo"));
  ASSERT_EQ(std::filesystem::path("C:/config/plugins/demo.toml"),
            rawPlugin->lastConfigPath);

  rawPlugin->state_ = PluginConfigurationState::Disabled;
  manager.reload("C:/config");
  ASSERT_TRUE(!manager.isActive("demo"));
  ASSERT_EQ(1, rawPlugin->stopCount);
}

TEST(PluginManagerTest, InvalidPluginDoesNotBlockOtherPlugins) {
  PluginManager manager;
  auto invalid = std::make_unique<FakePlugin>(
      "invalid", PluginConfigurationState::Invalid);
  auto valid =
      std::make_unique<FakePlugin>("valid", PluginConfigurationState::Enabled);
  ASSERT_TRUE(manager.registerPlugin(std::move(invalid)));
  ASSERT_TRUE(manager.registerPlugin(std::move(valid)));
  manager.initialize(HostContext());

  const auto report = manager.reload("C:/config");
  ASSERT_TRUE(report.hasErrors);
  ASSERT_TRUE(!manager.isActive("invalid"));
  ASSERT_TRUE(manager.isActive("valid"));
}

TEST(PluginManagerTest, FailedStartLeavesPluginStopped) {
  PluginManager manager;
  auto plugin =
      std::make_unique<FakePlugin>("demo", PluginConfigurationState::Enabled);
  FakePlugin *rawPlugin = plugin.get();
  rawPlugin->startResult = false;
  ASSERT_TRUE(manager.registerPlugin(std::move(plugin)));
  manager.initialize(HostContext());

  const auto report = manager.reload("C:/config");
  ASSERT_TRUE(report.hasErrors);
  ASSERT_TRUE(!manager.isActive("demo"));
  ASSERT_EQ(1, rawPlugin->startCount);
  ASSERT_EQ(1, rawPlugin->stopCount);
}

TEST(PluginManagerTest, NativeEventsAreBroadcastToAllActivePlugins) {
  PluginManager manager;
  auto first =
      std::make_unique<FakePlugin>("first", PluginConfigurationState::Enabled);
  auto second =
      std::make_unique<FakePlugin>("second", PluginConfigurationState::Enabled);
  FakePlugin *firstRaw = first.get();
  FakePlugin *secondRaw = second.get();
  firstRaw->handlesEvent = true;
  ASSERT_TRUE(manager.registerPlugin(std::move(first)));
  ASSERT_TRUE(manager.registerPlugin(std::move(second)));
  manager.initialize(HostContext());
  manager.reload("C:/config");

  ASSERT_TRUE(
      manager.handleNativeEvent("windows_generic_MSG", nullptr, nullptr));
  ASSERT_EQ(1, firstRaw->eventCount);
  ASSERT_EQ(1, secondRaw->eventCount);
}

TEST(PluginManagerTest, ShutdownStopsPluginsInReverseOrder) {
  std::vector<QString> stopOrder;
  PluginManager manager;
  ASSERT_TRUE(manager.registerPlugin(std::make_unique<FakePlugin>(
      "first", PluginConfigurationState::Enabled, &stopOrder)));
  ASSERT_TRUE(manager.registerPlugin(std::make_unique<FakePlugin>(
      "second", PluginConfigurationState::Enabled, &stopOrder)));
  manager.initialize(HostContext());
  manager.reload("C:/config");

  manager.shutdown();
  ASSERT_EQ(std::size_t(2), stopOrder.size());
  ASSERT_EQ(QString("second"), stopOrder[0]);
  ASSERT_EQ(QString("first"), stopOrder[1]);
}

TEST(PluginManagerTest, FunctionsDoNotRequireActiveMonitoring) {
  PluginManager manager;
  auto plugin =
      std::make_unique<FakePlugin>("demo", PluginConfigurationState::Disabled);
  auto *raw = plugin.get();
  manager.registerPlugin(std::move(plugin));
  manager.initialize(HostContext());
  manager.reload("C:/config");
  ASSERT_EQ(std::size_t(1), manager.functions("demo").size());
  int completions = 0;
  bool succeeded = false;
  manager.invokeFunction("demo", "run", [&](auto result) {
    ++completions;
    succeeded = result.succeeded;
  });
  ASSERT_TRUE(succeeded);
  ASSERT_EQ(1, completions);
  ASSERT_EQ(1, raw->functionCount);
  manager.invokeFunction("demo", "missing",
                         [&](auto result) { ASSERT_TRUE(!result.succeeded); });
  manager.invokeFunction("missing", "run",
                         [&](auto result) { ASSERT_TRUE(!result.succeeded); });
  raw->throwFunction = true;
  manager.invokeFunction("demo", "run",
                         [&](auto result) { ASSERT_TRUE(!result.succeeded); });
}

TEST(PluginManagerTest, ReloadCancelsInactivePluginFunctionsExactlyOnce) {
  PluginManager manager;
  auto plugin =
      std::make_unique<FakePlugin>("demo", PluginConfigurationState::Invalid);
  auto *raw = plugin.get();
  raw->deferFunction = true;
  manager.registerPlugin(std::move(plugin));
  manager.initialize(HostContext());
  manager.reload("C:/config");
  int completions = 0;
  bool succeeded = true;
  manager.invokeFunction("demo", "run", [&](auto result) {
    ++completions;
    succeeded = result.succeeded;
  });
  manager.reload("C:/config");
  raw->pendingCompletion({true, {}});
  ASSERT_EQ(1, completions);
  ASSERT_TRUE(!succeeded);
  ASSERT_EQ(1, raw->stopCount);
}

int main() { return RUN_ALL_TESTS(); }
