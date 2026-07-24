#include "OverlayWindow.h"

#include "Logger.h"

#include <LayerShellQt/Window>

#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QRegion>
#include <QScreen>
#include <QWebEngineNavigationRequest>
#include <QWebEngineNewWindowRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWindow>

#include <cmath>
#include <functional>

namespace AptNative {

namespace {
class OverlayWebView final : public QWebEngineView {
public:
    explicit OverlayWebView(QWidget *parent = nullptr) : QWebEngineView(parent) {}

    std::function<void()> requestGameFocus;

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        const bool closeShortcut =
            event->key() == Qt::Key_Escape ||
            (event->key() == Qt::Key_W &&
             event->modifiers().testFlag(Qt::ControlModifier));
        if (closeShortcut) {
            event->accept();
            if (requestGameFocus) requestGameFocus();
            return;
        }
        QWebEngineView::keyPressEvent(event);
    }

};
}

OverlayWindow::OverlayWindow(QString profilePath,
                             bool useLayerShell,
                             Logger *logger,
                             QWidget *parent)
    : QWidget(parent),
      m_view(new OverlayWebView(this)),
      m_browserView(new OverlayWebView(this)),
      m_logger(logger),
      m_useLayerShell(useLayerShell)
{
    setObjectName(QStringLiteral("awakened-native-overlay"));
    setWindowTitle(QStringLiteral("Awakened PoE Trade Native"));
    // The fallback remains a normal window because Plasma unmaps unparented
    // tool windows when focus is returned to the game. The KWin bridge gives
    // it skip-taskbar/pager/switcher and keep-above semantics instead.
    Qt::WindowFlags windowFlags = Qt::FramelessWindowHint |
                                  Qt::WindowStaysOnTopHint |
                                  Qt::WindowDoesNotAcceptFocus |
                                  Qt::Window;
    if (!m_useLayerShell &&
        QGuiApplication::platformName() == QStringLiteral("xcb")) {
        // A managed X11 keep-above window still sits below KWin's active
        // full-screen layer. Override-redirect is the native overlay path:
        // it is not a task and can be raised above a full-screen X11 game.
        windowFlags |= Qt::X11BypassWindowManagerHint;
    }
    setWindowFlags(windowFlags);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAutoFillBackground(false);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    auto *profile = new QWebEngineProfile(QStringLiteral("apt-native"), this);
    profile->setPersistentStoragePath(QDir(profilePath).filePath(QStringLiteral("webengine")));
    profile->setCachePath(QDir(profilePath).filePath(QStringLiteral("webengine-cache")));
    profile->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);
    profile->setHttpUserAgent(profile->httpUserAgent() +
        QStringLiteral(" AwakenedPoETradeNative/%1 Electron/40.9.1")
            .arg(QStringLiteral(APT_NATIVE_VERSION)));
    auto *page = new QWebEnginePage(profile, m_view);
    m_view->setPage(page);
    auto *browserPage = new QWebEnginePage(profile, m_browserView);
    m_browserView->setPage(browserPage);
    m_browserView->hide();

    const auto focusGame = [this] { emit focusGameRequested(); };
    static_cast<OverlayWebView *>(m_view)->requestGameFocus = focusGame;
    static_cast<OverlayWebView *>(m_browserView)->requestGameFocus = focusGame;

    page->setBackgroundColor(Qt::transparent);
    m_view->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    m_view->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    m_view->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
    m_view->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, false);

    connect(m_view, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (!ok) {
            m_logger->write(QStringLiteral("error [Overlay] Renderer page failed to load."));
            return;
        }
        emit pageLoaded();
    });
    connect(page, &QWebEnginePage::newWindowRequested, this,
            [](QWebEngineNewWindowRequest &request) {
        if (request.requestedUrl().isValid()) QDesktopServices::openUrl(request.requestedUrl());
    });
    connect(page, &QWebEnginePage::navigationRequested, this,
            [](QWebEngineNavigationRequest &request) {
        const QUrl url = request.url();
        const bool local = url.host() == QStringLiteral("127.0.0.1") ||
                           url.host() == QStringLiteral("localhost");
        if (request.isMainFrame() && !local && url.scheme().startsWith(QStringLiteral("http"))) {
            QDesktopServices::openUrl(url);
            request.reject();
        }
            });
    connect(browserPage, &QWebEnginePage::newWindowRequested, this,
            [](QWebEngineNewWindowRequest &request) {
        if (request.requestedUrl().isValid()) {
            QDesktopServices::openUrl(request.requestedUrl());
        }
    });

    m_trackTimer.setInterval(60);
    connect(&m_trackTimer, &QTimer::timeout, this, &OverlayWindow::checkTrackedArea);
    m_visibilityTimer.setInterval(20);
    connect(&m_visibilityTimer, &QTimer::timeout,
            this, &OverlayWindow::checkUiVisibility);
    m_visibilityTimer.start();

    m_hideUiTimer.setSingleShot(true);
    connect(&m_hideUiTimer, &QTimer::timeout, this, [this] {
        const Qt::KeyboardModifiers modifiers =
            QGuiApplication::queryKeyboardModifiers();
        if (modifiers == Qt::AltModifier) setUiVisible(false);
    });

    m_browserSyncTimer.setInterval(80);
    connect(&m_browserSyncTimer, &QTimer::timeout,
            this, &OverlayWindow::syncEmbeddedBrowser);
    m_browserSyncTimer.start();

    // Do not create the native surface in the constructor. The working EE2
    // path assigns the layer-shell role while the overlay is still hidden,
    // immediately before its first show.
    setInteractive(false);
}

void OverlayWindow::configureLayerShell()
{
    if (!m_useLayerShell) {
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen) screen = QGuiApplication::primaryScreen();
        if (screen) setGeometry(screen->geometry());
        return;
    }

    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) setGeometry(screen->geometry());

    if (m_layerWindow) {
        if (screen) {
            setGeometry(screen->geometry());
            m_layerWindow->setScreen(screen);
            m_layerWindow->setDesiredSize(screen->geometry().size());
        }
        return;
    }

    winId();
    if (!windowHandle()) {
        m_logger->write(QStringLiteral("error [Overlay] No native QWindow was created."));
        return;
    }
    m_layerWindow = LayerShellQt::Window::get(windowHandle());
    if (!m_layerWindow) {
        m_logger->write(QStringLiteral("error [Overlay] Failed to acquire LayerShellQt window."));
        return;
    }
    m_layerWindow->setScope(QStringLiteral("awakened-poe-trade-native"));
    m_layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    QFlags<LayerShellQt::Window::Anchor> anchors;
    anchors.setFlag(LayerShellQt::Window::AnchorTop);
    anchors.setFlag(LayerShellQt::Window::AnchorBottom);
    anchors.setFlag(LayerShellQt::Window::AnchorLeft);
    anchors.setFlag(LayerShellQt::Window::AnchorRight);
    m_layerWindow->setAnchors(anchors);
    m_layerWindow->setExclusiveZone(-1);
    m_layerWindow->setMargins({});
    if (screen) {
        m_layerWindow->setScreen(screen);
        m_layerWindow->setDesiredSize(screen->geometry().size());
    }
    m_layerWindow->setActivateOnShow(false);
    // Keep PoE as the active full-screen surface at all times. Accepting
    // keyboard focus here makes Plasma reveal its panels/taskbar.
    m_layerWindow->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);
}

void OverlayWindow::load(const QUrl &url)
{
    m_view->load(url);
}

void OverlayWindow::setInteractive(bool interactive)
{
    if (m_interactive == interactive) return;
    m_interactive = interactive;
    m_logger->write(QStringLiteral("debug [Overlay] Interactive=%1 layerShell=%2")
        .arg(interactive ? QStringLiteral("true") : QStringLiteral("false"),
             m_useLayerShell ? QStringLiteral("true") : QStringLiteral("false")));

    if (interactive) configureLayerShell();
    m_view->setAttribute(Qt::WA_TransparentForMouseEvents, !interactive);
    m_browserView->setAttribute(Qt::WA_TransparentForMouseEvents, !interactive);
    setAttribute(Qt::WA_TransparentForMouseEvents, !interactive);
    setAttribute(Qt::WA_ShowWithoutActivating,
                 !interactive || !m_useLayerShell);
    if (windowHandle()) {
        // QWaylandWindow::setMask updates wl_surface.set_input_region without
        // replacing the surface. A region entirely outside the surface is
        // click-through; the full rect restores pointer input.
        windowHandle()->setMask(interactive
            ? QRegion(rect())
            : QRegion(QRect(-2, -2, 1, 1)));
    }

    if (interactive) {
        // Plasma can leave a tool window in a hidden/minimized state after
        // focus returns to a full-screen game. Restore it before requesting
        // activation so the global shortcut always makes it visible.
        if (m_useLayerShell) {
            show();
        } else {
            // Keep this a normal mapped surface. KWin hides an inactive
            // full-screen surface when focus is returned to the game's own
            // full-screen surface; keep-above supplies the desired stacking.
            show();
        }
        raise();
    } else {
        clearFocus();
        // The regular Wayland fallback must not leave an invisible top-level
        // surface alive: Plasma may focus it, reveal panels over a full-screen
        // game, or add it to the task manager. Layer-shell remains mapped so
        // passive renderer widgets can still be shown without activation.
        if (!m_useLayerShell) hide();
    }
    emit interactionChanged(interactive);
}

void OverlayWindow::checkUiVisibility()
{
    const Qt::KeyboardModifiers modifiers =
        QGuiApplication::queryKeyboardModifiers();
    if (modifiers == Qt::AltModifier) {
        if (m_uiVisible && !m_hideUiTimer.isActive()) {
            m_hideUiTimer.start(m_interactive ? 85 : 275);
        }
    } else {
        m_hideUiTimer.stop();
        setUiVisible(true);
    }
}

void OverlayWindow::setUiVisible(bool visible)
{
    if (m_uiVisible == visible) return;
    m_uiVisible = visible;
    emit visibilityRequested(visible);
}

void OverlayWindow::syncEmbeddedBrowser()
{
    static const QString script = QStringLiteral(
        "(() => {"
        " const el = document.querySelector('webview');"
        " if (!el) return null;"
        " const r = el.getBoundingClientRect();"
        " const src = String(el.src || el.getAttribute('src') || '');"
        " return {src, x:r.x, y:r.y, width:r.width, height:r.height};"
        "})()");

    m_view->page()->runJavaScript(script, [this](const QVariant &result) {
        const QVariantMap state = result.toMap();
        const QUrl url(state.value(QStringLiteral("src")).toString());
        const QRect geometry(
            qRound(state.value(QStringLiteral("x")).toDouble()),
            qRound(state.value(QStringLiteral("y")).toDouble()),
            qRound(state.value(QStringLiteral("width")).toDouble()),
            qRound(state.value(QStringLiteral("height")).toDouble()));

        if (!url.isValid() || !url.scheme().startsWith(QStringLiteral("http")) ||
            geometry.width() <= 0 || geometry.height() <= 0) {
            m_browserView->hide();
            return;
        }
        if (url != m_browserUrl) {
            m_browserUrl = url;
            m_browserView->load(url);
        }
        m_browserView->setGeometry(geometry);
        m_browserView->show();
        m_browserView->raise();
    });
}

bool OverlayWindow::isInteractive() const
{
    return m_interactive;
}

void OverlayWindow::toggleInteractive()
{
    setInteractive(!m_interactive);
}

void OverlayWindow::beginTrackArea(const QPoint &from,
                                   const QRect &area,
                                   int closeThreshold,
                                   const QString &holdKey)
{
    m_trackFrom = from;
    m_trackArea = area;
    m_closeThreshold = qMax(0, closeThreshold);
    m_trackHoldKey = holdKey.trimmed();
    // Mapping the layer-shell surface can briefly make Wayland report the
    // cursor in a different coordinate space. Give it time to settle, then
    // establish a fresh baseline so a quick check is not dismissed by that
    // synthetic jump.
    m_trackPositionSettled = false;
    m_trackGraceTimer.restart();
    m_trackTimer.start();
}

void OverlayWindow::updateCursorPosition(const QPoint &position)
{
    m_cursorPosition = position;
    m_hasCursorPosition = true;
}

void OverlayWindow::checkTrackedArea()
{
    if (!m_trackArea.isValid()) {
        m_trackTimer.stop();
        return;
    }

    const QPoint cursor = m_hasCursorPosition ? m_cursorPosition : QCursor::pos();
    if (m_interactive) {
        if (!m_trackArea.contains(cursor)) {
            m_trackTimer.stop();
            setInteractive(false);
        }
        return;
    }

    const Qt::KeyboardModifiers modifiers = QGuiApplication::queryKeyboardModifiers();
    const bool holdPressed =
        (m_trackHoldKey.compare(QStringLiteral("Ctrl"), Qt::CaseInsensitive) == 0 &&
         modifiers.testFlag(Qt::ControlModifier)) ||
        (m_trackHoldKey.compare(QStringLiteral("Alt"), Qt::CaseInsensitive) == 0 &&
         modifiers.testFlag(Qt::AltModifier)) ||
        (m_trackHoldKey.compare(QStringLiteral("Shift"), Qt::CaseInsensitive) == 0 &&
         modifiers.testFlag(Qt::ShiftModifier));

    if (holdPressed && m_trackArea.contains(cursor)) {
        m_trackTimer.stop();
        setInteractive(true);
        return;
    }

    constexpr qint64 TrackGraceMs = 400;
    if (!m_trackPositionSettled) {
        if (m_trackGraceTimer.elapsed() < TrackGraceMs) return;
        m_trackFrom = cursor;
        m_trackPositionSettled = true;
        return;
    }

    if (!holdPressed) {
        const QPoint delta = cursor - m_trackFrom;
        const double distance = std::hypot(double(delta.x()), double(delta.y()));
        if (distance > m_closeThreshold) {
            m_trackTimer.stop();
            emit hideExclusiveWidgetRequested();
        }
    }
}

void OverlayWindow::focusGame()
{
    m_trackTimer.stop();
    setInteractive(false);
}

} // namespace AptNative
