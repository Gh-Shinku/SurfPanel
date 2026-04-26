#ifndef SURFPANEL_ACTION_MANAGER_H
#define SURFPANEL_ACTION_MANAGER_H

#include <QUrl>
#include <memory>
#include <string>
#include <unordered_map>

class QObject;
class QString;

class ActionContext {
public:
  virtual ~ActionContext() = default;

  virtual bool openUrlInDefaultBrowser(const QUrl &url) = 0;
  virtual bool copyToClipboard(const QString &text) = 0;
  virtual bool injectIntoActiveInput(const QString &text) = 0;
};

class Action {
public:
  virtual ~Action() = default;
  virtual bool invoke(const QString &payload, ActionContext &context) const = 0;
};

class DefaultActionContext final : public ActionContext {
public:
  bool openUrlInDefaultBrowser(const QUrl &url) override;
  bool copyToClipboard(const QString &text) override;
  bool injectIntoActiveInput(const QString &text) override;
};

class OpenUrlAction final : public Action {
public:
  bool invoke(const QString &payload, ActionContext &context) const override;
};

class InjectContentAction final : public Action {
public:
  bool invoke(const QString &payload, ActionContext &context) const override;
};

class ActionManager {
public:
  static constexpr const char *kOpenUrlAction = "open_url";
  static constexpr const char *kInjectContentAction = "inject_content";

  bool registerAction(const std::string &name, std::unique_ptr<Action> action);
  bool hasAction(const std::string &name) const;

  bool invoke(const std::string &name, const QString &payload) const;
  bool invoke(const std::string &name, const QString &payload,
              ActionContext &context) const;

private:
  std::unordered_map<std::string, std::unique_ptr<Action>> actions_;
};

void RegisterDefaultActions(ActionManager *manager);

bool InjectIntoInputObject(QObject *target, const QString &text);

#endif // SURFPANEL_ACTION_MANAGER_H