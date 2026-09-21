#include "core/action/action_manager.h"

#include "core/action/variable_resolver.h"
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QMetaObject>
#include <QMetaProperty>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <algorithm>

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

bool WriteUnicodeClipboardText(const QString &text, QString *error) {
  constexpr int kAttempts = 6;
  for (int attempt = 0; attempt < kAttempts; ++attempt) {
    if (!OpenClipboard(nullptr)) {
      const DWORD code = GetLastError();
      if (attempt + 1 == kAttempts) {
        *error =
            QString("Clipboard is busy after %1 attempts (Windows error %2).")
                .arg(kAttempts)
                .arg(code);
        return false;
      }
      Sleep(10);
      continue;
    }

    const std::wstring utf16 = text.toStdWString();
    const SIZE_T bytes = (utf16.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) {
      const DWORD code = GetLastError();
      CloseClipboard();
      *error = QString("Cannot allocate clipboard memory (Windows error %1).")
                   .arg(code);
      return false;
    }
    auto *destination = static_cast<wchar_t *>(GlobalLock(memory));
    if (destination == nullptr) {
      const DWORD code = GetLastError();
      GlobalFree(memory);
      CloseClipboard();
      *error = QString("Cannot access clipboard memory (Windows error %1).")
                   .arg(code);
      return false;
    }
    std::copy(utf16.cbegin(), utf16.cend(), destination);
    destination[utf16.size()] = L'\0';
    GlobalUnlock(memory);

    if (!EmptyClipboard() ||
        SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
      const DWORD code = GetLastError();
      GlobalFree(memory);
      CloseClipboard();
      *error =
          QString("Cannot write Unicode clipboard text (Windows error %1).")
              .arg(code);
      return false;
    }
    // The system owns memory after SetClipboardData succeeds.
    CloseClipboard();
    return true;
  }
  return false;
}

bool RestorePasteTarget(HWND target, QString *error) {
  if (!IsWindow(target) || !HasExternalForegroundWindow(target)) {
    *error = "The application active before SurfPanel opened is no longer "
             "available.";
    return false;
  }
  if (GetForegroundWindow() == target) {
    return true;
  }

  const DWORD targetThread = GetWindowThreadProcessId(target, nullptr);
  const DWORD currentThread = GetCurrentThreadId();
  const bool attached = targetThread != 0 && targetThread != currentThread &&
                        AttachThreadInput(currentThread, targetThread, TRUE);
  const BOOL activated = SetForegroundWindow(target);
  if (attached) {
    AttachThreadInput(currentThread, targetThread, FALSE);
  }
  for (int attempt = 0; attempt < 10 && GetForegroundWindow() != target;
       ++attempt) {
    Sleep(10);
  }
  if (GetForegroundWindow() != target) {
    *error = QString("Windows refused to restore the paste target (target=%1, "
                     "SetForegroundWindow=%2).")
                 .arg(reinterpret_cast<quintptr>(target), 0, 16)
                 .arg(activated != FALSE);
    return false;
  }
  return true;
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
  const bool opened = QDesktopServices::openUrl(url);
  if (!opened) {
    lastError_ = "The default browser rejected the URL.";
  }
  return opened;
}

bool DefaultActionContext::copyToClipboard(const QString &text) {
#ifdef Q_OS_WIN
  if (!WriteUnicodeClipboardText(text, &lastError_)) {
    qWarning().noquote() << "Clipboard write failed:" << lastError_;
    return false;
  }
  return true;
#else
  QClipboard *clipboard = QGuiApplication::clipboard();
  if (clipboard == nullptr) {
    lastError_ = "The system clipboard is unavailable.";
    return false;
  }

  clipboard->setText(text, QClipboard::Clipboard);
  const bool written = clipboard->text(QClipboard::Clipboard) == text;
  if (!written) {
    lastError_ = "The clipboard did not retain the copied text.";
  }
  return written;
#endif
}

bool DefaultActionContext::injectIntoActiveInput(const QString &text) {
#ifdef Q_OS_WIN
  if (nativePasteTarget_ == nullptr) {
    lastError_ =
        "Text was copied, but SurfPanel did not capture a paste target.";
    qWarning().noquote() << "Paste target is missing:" << lastError_;
    return false;
  }

  {
    const HWND captured = static_cast<HWND>(nativePasteTarget_);
    QString restoreError;
    if (!RestorePasteTarget(captured, &restoreError)) {
      lastError_ = "Text was copied, but " + restoreError;
      qWarning().noquote() << "Paste target restoration failed:" << lastError_;
      return false;
    }
    const HWND foreground = GetForegroundWindow();
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
      lastError_ = QString("Text was copied, but Windows rejected simulated "
                           "Ctrl+V (injected %1 of 4 events, error %2). The "
                           "target may be elevated.")
                       .arg(injected)
                       .arg(sendError);
      return false;
    }
    return true;
  }
#else
  QObject *focus = QGuiApplication::focusObject();
  const bool injected = InjectIntoInputObject(focus, text);
  if (!injected) {
    lastError_ =
        "Text was copied, but no editable input is available for insertion.";
  }
  return injected;
#endif
}

void DefaultActionContext::clearLastError() { lastError_.clear(); }

const QString &DefaultActionContext::lastError() const { return lastError_; }

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
  return clipboardOk && context.injectIntoActiveInput(payload);
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
