#include "GameWindowTracker.h"

#include "Logger.h"

#include <QDBusConnection>
#include <QDBusError>

namespace AptNative {

namespace {
const QString ServiceName = QStringLiteral("io.github.awakened_poe_trade.Native");
const QString ObjectPath = QStringLiteral("/GameWindow");
}

GameWindowTracker::GameWindowTracker(Logger *logger, QObject *parent)
    : QObject(parent), m_logger(logger)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(ServiceName)) {
        m_logger->write(QStringLiteral("warn [GameWindow] Could not register the KWin focus bridge D-Bus service: %1")
            .arg(bus.lastError().message()));
        return;
    }
    if (!bus.registerObject(ObjectPath, this,
                            QDBusConnection::ExportAllSlots |
                            QDBusConnection::ExportAllSignals)) {
        m_logger->write(QStringLiteral("warn [GameWindow] Could not export the KWin focus bridge object: %1")
            .arg(bus.lastError().message()));
        bus.unregisterService(ServiceName);
        return;
    }
    m_logger->write(QStringLiteral("info [GameWindow] KWin focus bridge is ready."));
}

void GameWindowTracker::setExpectedTitle(const QString &title)
{
    const QString normalized = title.trimmed();
    m_expectedTitle = normalized.isEmpty() ? QStringLiteral("Path of Exile") : normalized;
    recompute(true);
}

bool GameWindowTracker::isKnown() const
{
    return m_known;
}

bool GameWindowTracker::isGameActive() const
{
    return m_gameActive;
}

bool GameWindowTracker::wasGameActiveRecently(int maximumAgeMs) const
{
    return m_gameActive ||
           (m_lastGameActive.isValid() &&
            m_lastGameActive.elapsed() <= qMax(0, maximumAgeMs));
}

QString GameWindowTracker::activeCaption() const
{
    return m_caption;
}

QRect GameWindowTracker::gameGeometry() const
{
    return m_gameGeometry;
}

QPoint GameWindowTracker::cursorPosition() const
{
    return m_cursorPosition;
}

bool GameWindowTracker::hasCursorPosition() const
{
    return m_hasCursorPosition;
}

void GameWindowTracker::ReportActiveWindow(const QString &caption,
                                           const QString &resourceClass,
                                           int pid,
                                           int x,
                                           int y,
                                           int width,
                                           int height,
                                           bool waylandClient)
{
    const bool firstReport = !m_known;
    m_known = true;
    m_caption = caption;
    m_resourceClass = resourceClass;
    m_pid = pid;
    m_gameGeometry = QRect(x, y, width, height);
    m_waylandClient = waylandClient;
    recompute();

    if (firstReport) {
        m_logger->write(QStringLiteral("info [GameWindow] KWin focus tracking connected."));
    }
}

void GameWindowTracker::ReportCursorPosition(int x, int y)
{
    const QPoint position(x, y);
    if (m_hasCursorPosition && m_cursorPosition == position) return;
    m_cursorPosition = position;
    m_hasCursorPosition = true;
    emit cursorPositionChanged(x, y);
}

bool GameWindowTracker::matchesGameWindow(const QString &caption,
                                          const QString &resourceClass,
                                          const QString &expectedTitle)
{
    const bool titleMatch =
        !expectedTitle.trimmed().isEmpty() &&
        caption.contains(expectedTitle, Qt::CaseInsensitive);
    return titleMatch ||
           resourceClass.contains(QStringLiteral("pathofexile"),
                                  Qt::CaseInsensitive) ||
           resourceClass.contains(QStringLiteral("path of exile"),
                                  Qt::CaseInsensitive) ||
           resourceClass.compare(QStringLiteral("steam_app_238960"),
                                 Qt::CaseInsensitive) == 0 ||
           resourceClass.compare(QStringLiteral("steam_app_2694490"),
                                 Qt::CaseInsensitive) == 0;
}

bool GameWindowTracker::isSupportedGameWindow(
    const QString &caption,
    const QString &resourceClass,
    const QString &expectedTitle,
    bool waylandClient)
{
    return waylandClient &&
           matchesGameWindow(caption, resourceClass, expectedTitle);
}

QString GameWindowTracker::gameNameForClass(const QString &resourceClass)
{
    if (resourceClass.compare(QStringLiteral("steam_app_238960"),
                              Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Path of Exile 1");
    }
    if (resourceClass.compare(QStringLiteral("steam_app_2694490"),
                              Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Path of Exile 2");
    }
    return QStringLiteral("Path of Exile");
}

void GameWindowTracker::recompute(bool forceSignal)
{
    if (!m_known) return;

    const bool gameWindow =
        matchesGameWindow(m_caption, m_resourceClass, m_expectedTitle);
    if (gameWindow) {
        const int protocol = m_waylandClient ? 1 : 0;
        if (m_lastReportedGameProtocol != protocol) {
            const QString gameName = gameNameForClass(m_resourceClass);
            m_logger->write(m_waylandClient
                ? QStringLiteral(
                    "info [GameWindow] %1 is running as a native Wayland client.")
                    .arg(gameName)
                : QStringLiteral(
                    "error [GameWindow] %1 is running through XWayland. "
                    "This build is Wayland-only; relaunch the game with native "
                    "Wayland enabled.")
                    .arg(gameName));
            m_lastReportedGameProtocol = protocol;
        }
    }
    const bool active = isSupportedGameWindow(
        m_caption, m_resourceClass, m_expectedTitle, m_waylandClient);
    if (!forceSignal && active == m_gameActive) return;

    m_gameActive = active;
    if (active) m_lastGameActive.restart();
    m_logger->write(QStringLiteral("debug [GameWindow] Active=%1 caption=\"%2\" class=\"%3\" pid=%4")
        .arg(active ? QStringLiteral("true") : QStringLiteral("false"),
             m_caption, m_resourceClass, QString::number(m_pid)));
    emit gameActiveChanged(active);
}

} // namespace AptNative
