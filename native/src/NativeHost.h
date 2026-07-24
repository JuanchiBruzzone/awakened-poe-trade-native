#pragma once

#include "AppTray.h"
#include "ClipboardService.h"
#include "ConfigStore.h"
#include "EventServer.h"
#include "GameConfigReader.h"
#include "GameLogWatcher.h"
#include "GameWindowTracker.h"
#include "InputEventMonitor.h"
#include "InputInjector.h"
#include "HeistOcrService.h"
#include "Logger.h"
#include "ScreenshotService.h"
#include "ShortcutManager.h"
#include "Types.h"
#include "UpdateService.h"

#include <QHostAddress>
#include <QObject>

namespace AptNative {

class OverlayWindow;

struct NativeOptions {
    QString rendererRoot;
    QHostAddress listenAddress = QHostAddress::LocalHost;
    quint16 listenPort = 0;
    bool useLayerShell = true;
    bool browserMode = false;
    bool stealShortcuts = false;
    bool disableUpdates = false;
};

class NativeHost final : public QObject {
    Q_OBJECT
public:
    explicit NativeHost(NativeOptions options, QObject *parent = nullptr);
    bool start();

private:
    void handleEvent(const QString &name, const QJsonValue &payload);
    void applyHostConfig(const HostConfig &config);
    void performAction(const QJsonObject &action,
                       const QString &triggeringShortcut,
                       bool keepModKeys);
    void copyItem(const QJsonObject &action, const QString &triggeringShortcut);
    void stashSearch(const QString &text);
    void pasteInChat(const QString &text, bool send);
    void sendVisibility(bool visible);
    void sendFocusState(bool usingHotkey = false);
    void logStartupDiagnostics();
    NativeOptions m_options;
    Logger m_logger;
    ConfigStore m_configStore;
    EventServer m_server;
    ShortcutManager m_shortcuts;
    InputInjector m_input;
    InputEventMonitor m_inputMonitor;
    ScreenshotService m_screenshot;
    HeistOcrService m_heistOcr;
    ClipboardService m_clipboard;
    GameConfigReader m_gameConfig;
    GameLogWatcher m_gameLog;
    GameWindowTracker m_gameWindow;
    AppTray m_tray;
    UpdateService m_updater;
    OverlayWindow *m_overlay = nullptr;
    HostConfig m_hostConfig;
};

} // namespace AptNative
