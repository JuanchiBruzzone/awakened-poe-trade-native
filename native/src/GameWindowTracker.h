#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QString>

namespace AptNative {

class Logger;

class GameWindowTracker final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.awakened_poe_trade.Native.GameWindow")

public:
    explicit GameWindowTracker(Logger *logger, QObject *parent = nullptr);

    void setExpectedTitle(const QString &title);
    bool isKnown() const;
    bool isGameActive() const;
    bool wasGameActiveRecently(int maximumAgeMs) const;
    QString activeCaption() const;
    QRect gameGeometry() const;
    QPoint cursorPosition() const;
    bool hasCursorPosition() const;

public slots:
    void ReportActiveWindow(const QString &caption,
                            const QString &resourceClass,
                            int pid,
                            int x,
                            int y,
                            int width,
                            int height,
                            bool waylandClient);
    void ReportCursorPosition(int x, int y);

signals:
    void gameActiveChanged(bool active);
    void cursorPositionChanged(int x, int y);

private:
    void recompute(bool forceSignal = false);

    Logger *m_logger;
    QString m_expectedTitle = QStringLiteral("Path of Exile");
    QString m_caption;
    QString m_resourceClass;
    int m_pid = 0;
    QRect m_gameGeometry;
    QPoint m_cursorPosition;
    bool m_hasCursorPosition = false;
    bool m_waylandClient = false;
    int m_lastReportedGameProtocol = -1;
    bool m_known = false;
    bool m_gameActive = false;
    QElapsedTimer m_lastGameActive;
};

} // namespace AptNative
