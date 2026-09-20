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

#ifdef Q_OS_WIN
// True only when the window belongs to another process. SurfPanel's own
// windows (palette, tray menu, tooltips) must never receive the injected
// keystrokes, and there is no target to paste into without a foreground
// window at all.
bool HasExternalForegroundWindow(HWND window) {
  if (window == nullptr) {
    return false;
  }

  DWORD processId = 0;
  GetWindowThreadProcessId(window, &processId);
  return processId != 0 && processId != GetCurrentProcessId();
}
#endif

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
    // The palette hides before the action runs, so the paste has to go to
    // whichever window owns the foreground now. The window captured when the
    // palette opened is only a reference for the log: Windows hands the
    // foreground back to arbitrary windows (and to the shell when the palette
    // was opened from the tray), so requiring an exact match would veto
    // working pastes.
    const HWND captured = static_cast<HWND>(nativePasteTarget_);
    const HWND foreground = GetForegroundWindow();
    if (!HasExternalForegroundWindow(foreground)) {
      qWarning() << "No external foreground window to paste into: captured="
                 << static_cast<void *>(captured)
                 << "foreground=" << static_cast<void *>(foreground);
      return false;
    }
    if (foreground != captured) {
      qInfo() << "Paste target changed since the palette opened: captured="
              << static_cast<void *>(captured)
              << "foreground=" << static_cast<void *>(foreground);
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
    const UINT injected = SendInput(4, inputs, sizeof(INPUT));
    const DWORD sendError = injected == 4 ? 0 : GetLastError();
    if (injected != 4) {
      qWarning() << "SendInput was rejected: injected=" << injected
                 << "lastError=" << sendError
                 << "foreground=" << static_cast<void *>(foreground);
      return false;
    }
    return true;
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

void ActionManager::setVariableSettings(const VariableSettings &settings) {
  variableSettings_ = settings;
}

const VariableSettings &ActionManager::variableSettings() const {
  return variableSettings_;
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

  const auto now = QDateTime::currentDateTime();
  return iter->second->invoke(ResolveVariables(payload, now, variableSettings_),
                              context);
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
