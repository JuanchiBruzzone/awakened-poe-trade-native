#include "NativeHost.h"

#include "OverlayWindow.h"
#include "RuntimePaths.h"

#include <QApplication>
#include <QCursor>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

#include <utility>

namespace AptNative {

NativeHost::NativeHost(NativeOptions options, QObject *parent)
    : QObject(parent),
      m_options(std::move(options)),
      m_logger(this),
      m_configStore(&m_logger, this),
      m_server(m_options.rendererRoot, &m_configStore, &m_logger, this),
      m_shortcuts(&m_logger, m_options.stealShortcuts, this),
      m_input(&m_logger, this),
      m_inputMonitor(&m_logger, this),
      m_screenshot(&m_logger, this),
      m_heistOcr(m_configStore.dataDirectory(), &m_logger, this),
      m_clipboard(&m_logger, this),
      m_gameConfig(&m_logger, this),
      m_gameLog(&m_logger, this),
      m_gameWindow(&m_logger, this),
      m_tray(this),
      m_updater(QStringLiteral(APT_UPDATE_REPOSITORY),
                QStringLiteral(APT_NATIVE_VERSION),
                m_configStore.dataDirectory(),
                m_options.disableUpdates,
                &m_logger,
                this)
{
    connect(&m_server, &EventServer::eventReceived, this, &NativeHost::handleEvent);
    connect(&m_gameLog, &GameLogWatcher::linesRead, this, [this](const QStringList &lines) {
        m_server.broadcast(makeEvent(QStringLiteral("MAIN->CLIENT::game-log"),
            QJsonObject{{QStringLiteral("lines"), QJsonArray::fromStringList(lines)}}));
    });
    connect(&m_shortcuts, &ShortcutManager::actionTriggered,
            this, [this](const QJsonObject &action,
                         const QString &shortcut,
                         bool keepModKeys) {
        const bool copyItemAction =
            action.value(QStringLiteral("type")).toString() ==
            QStringLiteral("copy-item");
        const bool preserveModifiers = keepModKeys && !copyItemAction;
        if (m_hostConfig.logKeys) {
            m_logger.write(QStringLiteral("debug [Shortcuts] Activated %1 with %2")
                .arg(action.value(QStringLiteral("type")).toString(), shortcut));
        }
        if (!m_input.isAvailable()) {
            performAction(action,
                          preserveModifiers ? shortcut : QString{},
                          preserveModifiers);
            return;
        }
        m_input.releaseShortcut(shortcut, preserveModifiers,
            [this, action, shortcut, preserveModifiers](bool released) {
                if (!released) {
                    m_logger.write(QStringLiteral("warn [Shortcuts] Could not force-release the triggering shortcut."));
                }
                performAction(action,
                              preserveModifiers ? shortcut : QString{},
                              preserveModifiers);
            });
    });
    connect(&m_shortcuts, &ShortcutManager::overlayToggleTriggered,
            this, [this](const QString &) {
        const bool closingOverlay = m_overlay && m_overlay->isInteractive();
        m_logger.write(QStringLiteral("debug [Shortcuts] Overlay toggle requested; closing=%1 gameActive=%2")
            .arg(closingOverlay ? QStringLiteral("true") : QStringLiteral("false"),
                 m_gameWindow.isGameActive() ? QStringLiteral("true") : QStringLiteral("false")));
        if (!closingOverlay && m_gameWindow.isKnown() &&
            !m_gameWindow.wasGameActiveRecently(2500)) return;
        if (m_overlay) {
            m_overlay->toggleInteractive();
            sendVisibility(m_overlay->isInteractive());
            sendFocusState(true);
        }
    });
    connect(&m_shortcuts, &ShortcutManager::letterCaptured,
            this, [this](const QString &token, const QString &letter) {
        m_server.broadcast(makeEvent(
            QStringLiteral("MAIN->CLIENT::hotkey-captured"),
            QJsonObject{{QStringLiteral("token"), token},
                        {QStringLiteral("key"),
                         letter.isEmpty() ? QJsonValue(QJsonValue::Null)
                                          : QJsonValue(letter)}}));
    });
    connect(&m_shortcuts, &ShortcutManager::numberCaptured,
            this, [this](const QString &token, const QString &key) {
        m_server.broadcast(makeEvent(
            QStringLiteral("MAIN->CLIENT::number-captured"),
            QJsonObject{{QStringLiteral("token"), token},
                        {QStringLiteral("key"), key}}));
    });
    connect(&m_inputMonitor, &InputEventMonitor::wheelRotated,
            this, [this](int rotation) {
        if (!m_hostConfig.stashScroll || !m_gameWindow.isGameActive() ||
            rotation == 0) return;
        const QRect bounds = m_gameWindow.gameGeometry();
        if (!bounds.isValid()) return;

        const QPoint mouse = QCursor::pos();
        const int sidebarWidth = qRound(bounds.height() * (370.0 / 600.0));
        const bool isStashArea =
            mouse.x() <= bounds.x() + sidebarWidth &&
            mouse.y() > bounds.y() + qRound(bounds.height() * (154.0 / 1600.0)) &&
            mouse.y() < bounds.y() + qRound(bounds.height() * (1192.0 / 1600.0));
        if (!isStashArea) {
            m_input.tapKey(rotation > 0
                ? QStringLiteral("ArrowRight")
                : QStringLiteral("ArrowLeft"));
        }
    });
    connect(&m_tray, &AppTray::toggleOverlay, this, [this] {
        if (m_overlay) {
            m_overlay->toggleInteractive();
            sendVisibility(m_overlay->isInteractive());
            sendFocusState(false);
        } else {
            QDesktopServices::openUrl(m_server.appUrl());
        }
    });
    connect(&m_gameWindow, &GameWindowTracker::gameActiveChanged,
            this, [this](bool active) {
        if (active && m_options.useLayerShell &&
            m_overlay && m_overlay->isInteractive()) {
            m_overlay->setInteractive(false);
        }
        m_shortcuts.setGameActive(
            active && !(m_overlay && m_overlay->isInteractive()),
            true);
        sendFocusState(false);
    });
    connect(&m_tray, &AppTray::quitRequested, qApp, &QApplication::quit);
    connect(&m_updater, &UpdateService::stateChanged, this,
            [this](const QJsonObject &state) {
        m_server.setUpdaterState(state);
        m_server.broadcast(makeEvent(QStringLiteral("MAIN->CLIENT::updater-state"), state));
    });
    m_server.setUpdaterState(m_updater.state());
}

bool NativeHost::start()
{
    logStartupDiagnostics();
    if (!m_server.listen(m_options.listenAddress, m_options.listenPort)) return false;
    m_tray.setAppUrl(m_server.appUrl());
    m_tray.setDataDirectory(m_configStore.dataDirectory());

    if (m_options.browserMode) {
        QDesktopServices::openUrl(m_server.appUrl());
        m_logger.write(QStringLiteral("info [Host] Browser mode enabled; no overlay surface created."));
    } else {
        m_overlay = new OverlayWindow(m_configStore.dataDirectory(),
                                      m_options.useLayerShell,
                                      &m_logger);
        connect(m_overlay, &OverlayWindow::pageLoaded, this, [this] {
            m_server.broadcast(makeEvent(QStringLiteral("MAIN->OVERLAY::overlay-attached")));
            sendVisibility(true);
            sendFocusState(false);
        });
        connect(m_overlay, &OverlayWindow::interactionChanged, this, [this](bool interactive) {
            // Do not let the currently registered gameplay shortcuts consume
            // key combinations while a settings input is trying to record
            // them. The overlay toggle remains enabled by ShortcutManager so
            // Shift+Space can always close the overlay.
            m_shortcuts.setGameActive(
                !interactive && m_gameWindow.isGameActive(),
                m_gameWindow.isKnown());
            sendFocusState(false);
        });
        connect(m_overlay, &OverlayWindow::hideExclusiveWidgetRequested, this, [this] {
            m_server.broadcast(makeEvent(QStringLiteral("MAIN->OVERLAY::hide-exclusive-widget")));
        });
        connect(m_overlay, &OverlayWindow::focusGameRequested, this, [this] {
            m_overlay->focusGame();
            sendFocusState(false);
        });
        connect(m_overlay, &OverlayWindow::visibilityRequested,
                this, &NativeHost::sendVisibility);
        connect(&m_gameWindow, &GameWindowTracker::cursorPositionChanged,
                m_overlay, [this](int x, int y) {
            if (m_overlay) m_overlay->updateCursorPosition(QPoint(x, y));
        });
        if (m_gameWindow.hasCursorPosition()) {
            m_overlay->updateCursorPosition(m_gameWindow.cursorPosition());
        }
        m_overlay->load(m_server.appUrl());
    }

    m_logger.write(QStringLiteral("info Linux native host / v%1 / renderer %2")
        .arg(QStringLiteral(APT_NATIVE_VERSION), m_options.rendererRoot));
    if (!m_input.isAvailable()) {
        m_logger.write(QStringLiteral("error [Host] Price checks and chat input require ydotool."));
    }
    QTimer::singleShot(3000, &m_updater, &UpdateService::check);
    auto *updateTimer = new QTimer(this);
    updateTimer->setInterval(16 * 60 * 60 * 1000);
    connect(updateTimer, &QTimer::timeout, &m_updater, &UpdateService::check);
    updateTimer->start();
    QTimer::singleShot(5000, this, [this] {
        if (!m_gameWindow.isKnown()) {
            m_logger.write(QStringLiteral(
                "error [Startup] The KWin bridge has not connected. Install "
                "and enable awakened-poe-trade-native-focus; game focus "
                "recognition and quick-check cursor tracking are unavailable."));
        }
    });
    return true;
}

void NativeHost::logStartupDiagnostics()
{
    const QString platform = QGuiApplication::platformName();
    const QString desktop = qEnvironmentVariable(
        "XDG_CURRENT_DESKTOP", QStringLiteral("unknown"));
    const QString waylandDisplay = qEnvironmentVariable(
        "WAYLAND_DISPLAY", QStringLiteral("not-set"));
    m_logger.write(QStringLiteral(
        "info [Startup] Session platform=%1 desktop=%2 WAYLAND_DISPLAY=%3")
        .arg(platform, desktop, waylandDisplay));

    const auto reportExecutable = [this](const QString &name, bool required) {
        const QString path = QStandardPaths::findExecutable(name);
        if (path.isEmpty()) {
            m_logger.write(QStringLiteral("%1 [Startup] Dependency missing: %2")
                .arg(required ? QStringLiteral("error") : QStringLiteral("warn"),
                     name));
        } else {
            m_logger.write(QStringLiteral(
                "info [Startup] Dependency ready: %1 (%2)").arg(name, path));
        }
    };
    reportExecutable(QStringLiteral("ydotool"), true);
    reportExecutable(QStringLiteral("wl-paste"), true);
    reportExecutable(QStringLiteral("qdbus6"), false);
    reportExecutable(QStringLiteral("kpackagetool6"), false);

    const QString bridgeMetadata = RuntimePaths::findDataFile(
        QStringLiteral(
            "kwin/scripts/awakened-poe-trade-native-focus/metadata.json"));
    m_logger.write(bridgeMetadata.isEmpty()
        ? QStringLiteral(
            "error [Startup] KWin bridge package is unavailable. Reinstall "
            "the AppImage or run install-kwin-integration.sh.")
        : QStringLiteral("info [Startup] KWin bridge package ready: %1")
            .arg(bridgeMetadata));

    if (QDBusConnection::sessionBus().isConnected()) {
        m_logger.write(QStringLiteral("info [Startup] Session D-Bus is ready."));
    } else {
        m_logger.write(QStringLiteral(
            "error [Startup] The session D-Bus is unavailable; KDE shortcuts "
            "and the KWin bridge cannot operate."));
        return;
    }

    if (!bridgeMetadata.isEmpty()) {
        const QString pluginId =
            QStringLiteral("awakened-poe-trade-native-focus");
        const QString scriptPath = QFileInfo(bridgeMetadata)
            .dir().filePath(QStringLiteral("contents/code/main.js"));
        QDBusInterface scripting(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/Scripting"),
            QStringLiteral("org.kde.kwin.Scripting"),
            QDBusConnection::sessionBus());
        if (!scripting.isValid()) {
            m_logger.write(QStringLiteral(
                "error [Startup] KWin's scripting interface is unavailable."));
            return;
        }
        scripting.call(QStringLiteral("unloadScript"), pluginId);
        const QDBusMessage loadReply = scripting.call(
            QStringLiteral("loadScript"), scriptPath, pluginId);
        const QDBusMessage startReply =
            scripting.call(QStringLiteral("start"));
        if (loadReply.type() == QDBusMessage::ErrorMessage ||
            startReply.type() == QDBusMessage::ErrorMessage) {
            const QString error = loadReply.type() == QDBusMessage::ErrorMessage
                ? loadReply.errorMessage()
                : startReply.errorMessage();
            m_logger.write(QStringLiteral(
                "error [Startup] Could not load the KWin bridge: %1").arg(error));
        } else {
            m_logger.write(QStringLiteral(
                "info [Startup] KWin bridge loaded for this session."));
        }
    }
}

void NativeHost::handleEvent(const QString &name, const QJsonValue &payload)
{
    if (name == QStringLiteral("CLIENT->MAIN::update-host-config")) {
        applyHostConfig(HostConfig::fromJson(payload.toObject()));
        return;
    }
    if (name == QStringLiteral("CLIENT->MAIN::begin-hotkey-capture")) {
        m_shortcuts.beginLetterCapture(
            payload.toObject().value(QStringLiteral("token")).toString());
        return;
    }
    if (name == QStringLiteral("CLIENT->MAIN::begin-number-capture")) {
        m_shortcuts.beginNumberCapture(
            payload.toObject().value(QStringLiteral("token")).toString());
        return;
    }
    if (name == QStringLiteral("CLIENT->MAIN::cancel-number-capture")) {
        m_shortcuts.cancelNumberCapture();
        return;
    }
    if (name == QStringLiteral("CLIENT->MAIN::save-config")) {
        const QJsonObject object = payload.toObject();
        const QString contents = object.value(QStringLiteral("contents")).toString();
        const bool temporary = object.value(QStringLiteral("isTemporary")).toBool(false);
        if (m_configStore.save(contents, temporary)) {
            m_server.broadcast(makeEvent(QStringLiteral("MAIN->CLIENT::config-changed"),
                QJsonObject{{QStringLiteral("contents"), contents}}));
        }
        return;
    }
    if (name == QStringLiteral("OVERLAY->MAIN::focus-game")) {
        if (m_overlay) m_overlay->focusGame();
        sendFocusState(false);
        return;
    }
    if (name == QStringLiteral("OVERLAY->MAIN::track-area")) {
        if (!m_overlay) return;
        const QJsonObject object = payload.toObject();
        const QJsonObject from = object.value(QStringLiteral("from")).toObject();
        const QJsonObject area = object.value(QStringLiteral("area")).toObject();
        const QPoint origin(from.value(QStringLiteral("x")).toInt(),
                            from.value(QStringLiteral("y")).toInt());
        const QRect rect(area.value(QStringLiteral("x")).toInt(),
                         area.value(QStringLiteral("y")).toInt(),
                         area.value(QStringLiteral("width")).toInt(),
                         area.value(QStringLiteral("height")).toInt());
        m_overlay->beginTrackArea(origin, rect,
                                  object.value(QStringLiteral("closeThreshold")).toInt(0),
                                  object.value(QStringLiteral("holdKey")).toString());
        return;
    }
    if (name == QStringLiteral("CLIENT->MAIN::user-action")) {
        const QJsonObject object = payload.toObject();
        const QString action = object.value(QStringLiteral("action")).toString();
        if (action == QStringLiteral("quit")) {
            qApp->quit();
        } else if (action == QStringLiteral("stash-search")) {
            stashSearch(object.value(QStringLiteral("text")).toString());
        } else if (action == QStringLiteral("check-for-update")) {
            m_updater.check();
        } else if (action == QStringLiteral("update-and-restart")) {
            m_updater.installAndRestart();
        }
    }
}

void NativeHost::applyHostConfig(const HostConfig &config)
{
    m_hostConfig = config;
    m_tray.setOverlayKey(config.overlayKey);
    m_gameWindow.setExpectedTitle(config.windowTitle);
    m_gameConfig.readConfig(config.gameConfig);
    const QStringList reservedShortcuts{
        QStringLiteral("Ctrl + C"), QStringLiteral("Ctrl + V"),
        QStringLiteral("Ctrl + A"), QStringLiteral("Ctrl + F"),
        QStringLiteral("Ctrl + Enter"), QStringLiteral("Home"),
        QStringLiteral("Delete"), QStringLiteral("Enter"),
        QStringLiteral("ArrowUp"), QStringLiteral("ArrowRight"),
        QStringLiteral("ArrowLeft"),
        advancedItemCopyChord(m_gameConfig.showModsKey())
    };
    m_shortcuts.update(config, reservedShortcuts);
    m_shortcuts.setGameActive(m_gameWindow.isGameActive(), m_gameWindow.isKnown());
    m_gameLog.restart(config.clientLog);
    m_inputMonitor.setEnabled(config.stashScroll);
    m_heistOcr.setLanguage(config.language);
}

void NativeHost::performAction(const QJsonObject &action,
                               const QString &triggeringShortcut,
                               bool keepModKeys)
{
    Q_UNUSED(keepModKeys);
    const QString type = action.value(QStringLiteral("type")).toString();
    const bool closingOverlay = type == QStringLiteral("toggle-overlay") &&
                                m_overlay && m_overlay->isInteractive();
    const bool overlayActivation =
        type == QStringLiteral("toggle-overlay") &&
        m_gameWindow.wasGameActiveRecently(2500);
    if (!closingOverlay && !overlayActivation &&
        m_gameWindow.isKnown() && !m_gameWindow.isGameActive()) {
        m_logger.write(QStringLiteral("debug [Shortcuts] Ignored %1 because Path of Exile is not active.").arg(type));
        return;
    }
    if (type == QStringLiteral("copy-item")) {
        copyItem(action, triggeringShortcut);
    } else if (type == QStringLiteral("trigger-event")) {
        m_server.sendToLastActive(makeEvent(QStringLiteral("MAIN->CLIENT::widget-action"),
            QJsonObject{{QStringLiteral("target"), action.value(QStringLiteral("target"))}}));
    } else if (type == QStringLiteral("toggle-overlay")) {
        if (m_overlay) {
            m_overlay->toggleInteractive();
            sendVisibility(m_overlay->isInteractive());
            sendFocusState(true);
        }
    } else if (type == QStringLiteral("stash-search")) {
        stashSearch(action.value(QStringLiteral("text")).toString());
    } else if (type == QStringLiteral("paste-in-chat")) {
        pasteInChat(action.value(QStringLiteral("text")).toString(),
                    action.value(QStringLiteral("send")).toBool(false));
    } else if (type == QStringLiteral("ocr-text")) {
        if (action.value(QStringLiteral("target")).toString() !=
            QStringLiteral("heist-gems")) return;
        const QString target = action.value(QStringLiteral("target")).toString();
        const qint64 pressTime = QDateTime::currentMSecsSinceEpoch();
        m_screenshot.captureActiveWindow(
            [this, target, pressTime](const QImage &image) {
                m_heistOcr.recognize(image,
                    [this, target, pressTime](const HeistOcrResult &result) {
                    if (!result.error.isEmpty()) return;
                    m_server.sendToLastActive(makeEvent(
                        QStringLiteral("MAIN->CLIENT::ocr-text"),
                        QJsonObject{
                            {QStringLiteral("target"), target},
                            {QStringLiteral("pressTime"), pressTime},
                            {QStringLiteral("ocrTime"), result.elapsedMs},
                            {QStringLiteral("paragraphs"),
                             QJsonArray::fromStringList(result.paragraphs)}
                        }));
                });
            },
            [](const QString &) {});
    } else if (type == QStringLiteral("test-only")) {
        m_logger.write(QStringLiteral("info [Shortcuts] Test shortcut activated."));
    } else {
        m_logger.write(QStringLiteral("warn [Shortcuts] Unsupported action type: %1").arg(type));
    }
}

void NativeHost::copyItem(const QJsonObject &action, const QString &triggeringShortcut)
{
    Q_UNUSED(triggeringShortcut);
    const QPoint position = m_gameWindow.hasCursorPosition()
        ? m_gameWindow.cursorPosition()
        : QCursor::pos();
    const bool focusOverlay = action.value(QStringLiteral("focusOverlay")).toBool(false);
    const QString target = action.value(QStringLiteral("target")).toString();

    m_clipboard.beginItemCapture(m_hostConfig.restoreClipboard,
        [this, position, focusOverlay, target](const QString &text) {
            QJsonObject payload{
                {QStringLiteral("target"), target},
                {QStringLiteral("clipboard"), text},
                {QStringLiteral("position"), QJsonObject{
                    {QStringLiteral("x"), position.x()}, {QStringLiteral("y"), position.y()}}},
                {QStringLiteral("focusOverlay"), focusOverlay}
            };
            m_server.sendToLastActive(makeEvent(QStringLiteral("MAIN->CLIENT::item-text"), payload));
            if (m_overlay && focusOverlay) {
                m_overlay->setInteractive(true);
                sendFocusState(true);
            }
            // A passive quick check deliberately leaves PoE focused. Sending
            // overlay=false here would make the renderer apply hide-on-blur
            // immediately, before cursor tracking has a chance to control the
            // popup's lifetime.
        });

    const QString chord =
        advancedItemCopyChord(m_gameConfig.showModsKey());
    const auto injectCopy = [this, chord] {
        if (!m_clipboard.isCapturing()) return;
        m_input.sendChord(chord, {}, [this, chord](bool ok) {
            if (!ok) {
                m_clipboard.cancel();
                m_logger.write(QStringLiteral("error [Shortcuts] Failed to inject item-copy chord: %1").arg(chord));
            }
        });
    };
    // KGlobalAccel reports the action before Proton has fully observed the
    // triggering key release. This timing avoids racing that state and retries
    // once for Wine/Wayland clipboard latency.
    QTimer::singleShot(120, this, injectCopy);
    QTimer::singleShot(420, this, injectCopy);
}

void NativeHost::stashSearch(const QString &text)
{
    if (text.isEmpty()) return;
    if (m_overlay) {
        m_overlay->focusGame();
        sendFocusState(false);
    }
    m_clipboard.withTemporaryText(text, m_hostConfig.restoreClipboard, [this] {
        m_input.sendSequence({
            QStringLiteral("Ctrl + F"),
            QStringLiteral("Ctrl + V"),
            QStringLiteral("Enter")
        });
    });
}

void NativeHost::pasteInChat(const QString &text, bool send)
{
    if (text.isEmpty()) return;
    static const QString lastPlaceholder = QStringLiteral("@last");
    static const QString autoClear = QStringLiteral("#%@$&/");

    // Plain commands do not need the clipboard. Typing them directly avoids a
    // Wayland selection race where Proton can paste the item text left behind
    // by the preceding price check instead of the newly offered command.
    if (!text.startsWith(lastPlaceholder) &&
        !text.endsWith(lastPlaceholder)) {
        QStringList openChat{QStringLiteral("Enter")};
        if (!autoClear.contains(text.front())) {
            openChat << QStringLiteral("Ctrl + A");
        }
        m_input.sendSequence(openChat, [this, text, send](bool opened) {
            if (!opened) return;
            m_input.typeText(text, [this, send](bool typed) {
                if (!typed || !send) return;
                m_input.sendSequence({
                    QStringLiteral("Enter"),
                    QStringLiteral("Enter"),
                    QStringLiteral("ArrowUp"),
                    QStringLiteral("ArrowUp"),
                    QStringLiteral("Escape")
                });
            });
        });
        return;
    }

    QString clipboardText = text;
    QStringList sequence;
    if (text.startsWith(lastPlaceholder)) {
        clipboardText = text.mid(lastPlaceholder.size() + 1);
        sequence << QStringLiteral("Ctrl + Enter");
    } else if (text.endsWith(lastPlaceholder)) {
        clipboardText.chop(lastPlaceholder.size());
        sequence << QStringLiteral("Ctrl + Enter")
                 << QStringLiteral("Home")
                 << QStringLiteral("Home")
                 << QStringLiteral("Delete");
    } else {
        sequence << QStringLiteral("Enter");
        if (!autoClear.contains(text.front())) {
            sequence << QStringLiteral("Ctrl + A");
        }
    }
    sequence << QStringLiteral("Ctrl + V");
    if (send) {
        sequence << QStringLiteral("Enter")
                 << QStringLiteral("Enter")
                 << QStringLiteral("ArrowUp")
                 << QStringLiteral("ArrowUp")
                 << QStringLiteral("Escape");
    }

    m_clipboard.withTemporaryText(
        clipboardText, m_hostConfig.restoreClipboard, [this, sequence] {
            m_input.sendSequence(sequence);
    });
}

void NativeHost::sendVisibility(bool visible)
{
    m_server.broadcast(makeEvent(QStringLiteral("MAIN->OVERLAY::visibility"),
        QJsonObject{{QStringLiteral("isVisible"), visible}}));
}

void NativeHost::sendFocusState(bool usingHotkey)
{
    const bool overlay = m_overlay && m_overlay->isInteractive();
    const bool game = m_gameWindow.isKnown() ? m_gameWindow.isGameActive() : !overlay;
    m_server.broadcast(makeEvent(QStringLiteral("MAIN->OVERLAY::focus-change"),
        QJsonObject{{QStringLiteral("game"), game},
                    {QStringLiteral("overlay"), overlay},
                    {QStringLiteral("usingHotkey"), usingHotkey}}));
}

} // namespace AptNative
