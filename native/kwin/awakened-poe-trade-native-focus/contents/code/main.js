'use strict';

let lastGameWindow = null;
let activeOverlayWindow = null;

function isPathOfExile(window) {
    if (!window) return false;
    const caption = String(window.caption || '').toLowerCase();
    const resourceClass = String(window.resourceClass || '').toLowerCase();
    return caption.includes('path of exile') ||
        resourceClass.includes('pathofexile') ||
        resourceClass.includes('path of exile') ||
        resourceClass === 'steam_app_238960' ||
        resourceClass === 'steam_app_2694490';
}

function isNativeOverlay(window) {
    if (!window) return false;
    const caption = String(window.caption || '').toLowerCase();
    const resourceClass = String(window.resourceClass || '').toLowerCase();
    return caption === 'awakened poe trade native' ||
        resourceClass === 'io.github.snosme.awakened-poe-trade-native';
}

function configureNativeOverlay(window) {
    if (!isNativeOverlay(window)) return;

    // The Qt 6.11 fallback is an ordinary Wayland surface. Give it overlay
    // semantics in KWin so it covers panels above PoE 1/2 without ever
    // becoming a task, pager, or Alt+Tab entry.
    window.skipTaskbar = true;
    window.skipPager = true;
    window.skipSwitcher = true;
    window.keepAbove = true;
    workspace.raiseWindow(window);
    const isNewOverlay = activeOverlayWindow !== window;
    activeOverlayWindow = window;
    if (isNewOverlay) {
        window.closed.connect(function () {
            if (activeOverlayWindow === window) activeOverlayWindow = null;
        });
    }
}

function reportActiveWindow(window) {
    configureNativeOverlay(window);
    if (isPathOfExile(window)) {
        lastGameWindow = window;
        if (activeOverlayWindow) {
            // Active full-screen windows normally stack over inactive
            // keep-above windows. Re-raise without activating so the overlay
            // is visible while PoE retains keyboard focus.
            workspace.raiseWindow(activeOverlayWindow);
        }
    }
    const caption = window ? String(window.caption || '') : '';
    const resourceClass = window ? String(window.resourceClass || '') : '';
    const pid = window ? Number(window.pid || 0) : 0;
    const geometry = window ? window.frameGeometry : null;
    const x = geometry ? Math.round(Number(geometry.x || 0)) : 0;
    const y = geometry ? Math.round(Number(geometry.y || 0)) : 0;
    const width = geometry ? Math.round(Number(geometry.width || 0)) : 0;
    const height = geometry ? Math.round(Number(geometry.height || 0)) : 0;

    callDBus(
        'io.github.awakened_poe_trade.Native',
        '/GameWindow',
        'io.github.awakened_poe_trade.Native.GameWindow',
        'ReportActiveWindow',
        caption,
        resourceClass,
        pid,
        x,
        y,
        width,
        height,
        Boolean(window && window.waylandClient)
    );

    // A regular xdg_toplevel cannot receive keyboard focus without making
    // Plasma unstack the active full-screen game. Return focus immediately
    // while leaving the always-on-top overlay mapped for pointer interaction.
    if (isNativeOverlay(window) && lastGameWindow) {
        workspace.activeWindow = lastGameWindow;
    }
}

let lastCursorReport = 0;
let lastActiveHeartbeat = 0;
function reportCursorPosition() {
    const now = Date.now();
    if (now - lastCursorReport < 16) return;
    lastCursorReport = now;
    const position = workspace.cursorPos;
    callDBus(
        'io.github.awakened_poe_trade.Native',
        '/GameWindow',
        'io.github.awakened_poe_trade.Native.GameWindow',
        'ReportCursorPosition',
        Math.round(Number(position.x || 0)),
        Math.round(Number(position.y || 0))
    );
    // The host may restart after this script's initial D-Bus report. A
    // low-frequency heartbeat reconnects it without polling keyboard input or
    // changing focus.
    if (now - lastActiveHeartbeat >= 1000) {
        lastActiveHeartbeat = now;
        reportActiveWindow(workspace.activeWindow);
    }
}

workspace.windowAdded.connect(configureNativeOverlay);
workspace.windowActivated.connect(reportActiveWindow);
workspace.cursorPosChanged.connect(reportCursorPosition);

const existingWindows = typeof workspace.windowList === 'function'
    ? workspace.windowList()
    : workspace.stackingOrder;
for (const window of existingWindows) configureNativeOverlay(window);

reportActiveWindow(workspace.activeWindow);
reportCursorPosition();
