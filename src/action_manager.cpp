#include "action_manager.h"
#include "variable_resolver.h"
#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QMetaObject>
#include <QMetaProperty>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariant>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

bool TryInvokeInsertMethod(QObject *target, const char *signature,
                           const char *methodName, const QString &text) {
  const QMetaObject *metaObject = target->metaObject();
  if (metaObject->indexOfMethod(signature) < 0) {
    return false;
  }
  return QMetaObject::invokeMethod(target, methodName, Qt::DirectConnection,
                                   Q_ARG(QString, text));
}

bool AppendToProperty(QObject *target, const char *propertyName,
                      const QString &text) {
  const QMetaObject *metaObject = target->metaObject();
  const int propertyIndex = metaObject->indexOfProperty(propertyName);
  if (propertyIndex >= 0) {
    const QMetaProperty property = metaObject->property(propertyIndex);
    if (!property.isWritable() || !property.isReadable()) {
      return false;
    }
    const QString updated = property.read(target).toString() + text;
    return property.write(target, updated);
  }

  const QVariant current = target->property(propertyName);
  if (!current.isValid()) {
    return false;
  }

  const QString updated = current.toString() + text;
  target->setProperty(propertyName, QVariant(updated));
  return target->property(propertyName).toString() == updated;
}

} // namespace

bool InjectIntoInputObject(QObject *target, const QString &text) {
  if (target == nullptr || text.isEmpty()) {
    return false;
  }

  if (TryInvokeInsertMethod(target, "insert(QString)", "insert", text)) {
    return true;
  }

  if (TryInvokeInsertMethod(target, "insertPlainText(QString)",
                            "insertPlainText", text)) {
    return true;
  }

  if (AppendToProperty(target, "text", text)) {
    return true;
  }

  return AppendToProperty(target, "plainText", text);
}

bool DefaultActionContext::openUrlInDefaultBrowser(const QUrl &url) {
  return QDesktopServices::openUrl(url);
}

bool DefaultActionContext::copyToClipboard(const QString &text) {
  QClipboard *clipboard = QGuiApplication::clipboard();
  if (clipboard == nullptr) {
    return false;
  }

  clipboard->setText(text, QClipboard::Clipboard);
  return clipboard->text(QClipboard::Clipboard) == text;
}

bool DefaultActionContext::injectIntoActiveInput(const QString &text) {
#ifdef Q_OS_WIN
  if (nativePasteTarget_ != nullptr) {
    const HWND target = static_cast<HWND>(nativePasteTarget_);
    if (GetForegroundWindow() != target) {
      return false;
    }

    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(4, inputs, sizeof(INPUT)) == 4;
  }
#endif

  QObject *focus = QGuiApplication::focusObject();
  return InjectIntoInputObject(focus, text);
}

#ifdef Q_OS_WIN
void DefaultActionContext::setNativePasteTarget(void *window) {
  nativePasteTarget_ = window;
}

void DefaultActionContext::clearNativePasteTarget() {
  nativePasteTarget_ = nullptr;
}
#endif

bool OpenUrlAction::invoke(const QString &payload,
                           ActionContext &context) const {
  const QString trimmed = payload.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }

  const QUrl url = QUrl::fromUserInput(trimmed);
  if (!url.isValid()) {
    return false;
  }

  return context.openUrlInDefaultBrowser(url);
}

bool InjectContentAction::invoke(const QString &payload,
                                 ActionContext &context) const {
  if (payload.isEmpty()) {
    return false;
  }

  const bool clipboardOk = context.copyToClipboard(payload);
  const bool inputOk = context.injectIntoActiveInput(payload);
  return clipboardOk && inputOk;
}

bool ActionManager::registerAction(const std::string &name,
                                   std::unique_ptr<Action> action) {
  if (name.empty() || action == nullptr || hasAction(name)) {
    return false;
  }

  actions_.emplace(name, std::move(action));
  return true;
}

bool ActionManager::hasAction(const std::string &name) const {
  return actions_.find(name) != actions_.end();
}

bool ActionManager::invoke(const std::string &name,
                           const QString &payload) const {
  static DefaultActionContext defaultContext;
  return invoke(name, payload, defaultContext);
}

bool ActionManager::invoke(const std::string &name, const QString &payload,
                           ActionContext &context) const {
  const auto iter = actions_.find(name);
  if (iter == actions_.end()) {
    return false;
  }

  return iter->second->invoke(ResolveVariables(payload), context);
}

void RegisterDefaultActions(ActionManager *manager) {
  if (manager == nullptr) {
    return;
  }

  manager->registerAction(ActionManager::kOpenUrlAction,
                          std::make_unique<OpenUrlAction>());
  manager->registerAction(ActionManager::kInjectContentAction,
                          std::make_unique<InjectContentAction>());
}
