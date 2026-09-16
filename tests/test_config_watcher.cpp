#include "config.h"
#include "config_watcher.h"
#include "test_harness.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QThread>
#include <functional>

namespace {
void Save(const QString &path, const QByteArray &text) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QSaveFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  ASSERT_EQ(qint64(text.size()), file.write(text));
  ASSERT_TRUE(file.commit());
}

void Wait(int milliseconds) {
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < milliseconds) {
    QCoreApplication::processEvents();
    QThread::msleep(5);
  }
}

bool Until(const std::function<bool()> &predicate) {
  QElapsedTimer timer;
  timer.start();
  while (!predicate() && timer.elapsed() < 3000) {
    Wait(10);
  }
  return predicate();
}
} // namespace

TEST(ConfigWatcherTest, AtomicSavesDebounceAndIgnoreGeneratedCache) {
  QTemporaryDir temporary;
  const QString root = temporary.path() + "/config";
  Save(root + "/items.toml", "items = []\n");
  ConfigWatcher watcher;
  int reloads = 0;
  QObject::connect(&watcher, &ConfigWatcher::configurationChanged,
                   [&] { ++reloads; });
  watcher.start(root);
  for (int i = 0; i < 3; ++i) {
    Save(root + "/items.toml", "items = []\n# " + QByteArray::number(i));
    Wait(30);
  }
  ASSERT_TRUE(Until([&] { return reloads == 1; }));
  Save(root + "/items.toml", "items = []\n# next save");
  ASSERT_TRUE(Until([&] { return reloads == 2; }));
  Save(root + "/cache/compiled.toml", "items = []\n");
  Save(root + "/editor.tmp", "temporary");
  Wait(500);
  ASSERT_EQ(2, reloads);
  Save(root + "/items.toml", "items = []\n# next save");
  Wait(500);
  ASSERT_EQ(2, reloads);
}

TEST(ConfigWatcherTest, DiscoversImportsPluginsAndRecreatedDirectories) {
  QTemporaryDir temporary;
  const QString root = temporary.path() + "/config";
  Save(root + "/items.toml", "items = []\n");
  ConfigWatcher watcher;
  int reloads = 0;
  QObject::connect(&watcher, &ConfigWatcher::configurationChanged,
                   [&] { ++reloads; });
  watcher.start(root);
  Save(root + "/packages/demo/items.toml", "items = []\n");
  ASSERT_TRUE(Until([&] { return reloads == 1; }));
  Save(root + "/plugins/clipboard-filter.toml", "enabled = false\n");
  ASSERT_TRUE(Until([&] { return reloads == 2; }));
  ASSERT_TRUE(QFile::remove(root + "/plugins/clipboard-filter.toml"));
  ASSERT_TRUE(Until([&] { return reloads == 3; }));
  ASSERT_TRUE(QDir(root).removeRecursively());
  ASSERT_TRUE(Until([&] { return reloads == 4; }));
  Save(root + "/items.toml", "items = []\n");
  ASSERT_TRUE(Until([&] { return reloads == 5; }));
  Save(root + "/items.toml", "items = []\n# restored");
  ASSERT_TRUE(Until([&] { return reloads == 6; }));
}

TEST(ConfigWatcherTest, InvalidConfigurationRecoversWithoutRestart) {
  QTemporaryDir temporary;
  const QString root = temporary.path() + "/config";
  Save(root + "/items.toml", "items = []\n");
  const auto path = std::filesystem::u8path(root.toUtf8().toStdString());
  ASSERT_TRUE(LoadConfigWithFallback(path).ok);
  ConfigWatcher watcher;
  ConfigLoadResult result;
  int reloads = 0;
  QObject::connect(&watcher, &ConfigWatcher::configurationChanged, [&] {
    result = LoadConfigWithFallback(path);
    ++reloads;
  });
  watcher.start(root);
  Save(root + "/items.toml", "invalid = [");
  ASSERT_TRUE(Until([&] { return reloads == 1; }));
  ASSERT_TRUE(result.usedFallback);
  ASSERT_TRUE(!result.message.empty());
  Save(root + "/items.toml", "items = []\n");
  ASSERT_TRUE(Until([&] { return reloads == 2; }));
  ASSERT_TRUE(result.ok);
  ASSERT_TRUE(!result.usedFallback);
  Wait(500);
  ASSERT_EQ(2, reloads);
}

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  return RUN_ALL_TESTS();
}
