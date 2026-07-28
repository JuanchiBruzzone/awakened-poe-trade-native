#pragma once

#include <QAction>
#include <QJsonObject>
#include <QKeySequence>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace AptNative {

class Logger;
struct HostConfig;

class ShortcutManager final : public QObject {
    Q_OBJECT
public:
    explicit ShortcutManager(Logger *logger, bool stealConflicts, QObject *parent = nullptr);
    ~ShortcutManager() override;

    void update(const HostConfig &config, const QStringList &reservedShortcuts = {});
    void setGameActive(bool active, bool known);
    void clear();
    void beginLetterCapture(const QString &token);
    void cancelLetterCapture();
    void beginNumberCapture(const QString &token);
    void cancelNumberCapture();
    void beginTextCapture(const QString &token);
    void cancelTextCapture();
    void setOverlayInteractive(bool interactive);

signals:
    void actionTriggered(const QJsonObject &action,
                         const QString &triggeringShortcut,
                         bool keepModKeys);
    void overlayToggleTriggered(const QString &triggeringShortcut);
    void letterCaptured(const QString &token, const QString &letter);
    void numberCaptured(const QString &token, const QString &key);
    void textCaptured(const QString &token, const QString &key);
    void overlayEscapeTriggered();

private:
    static QKeySequence parseSequence(const QString &shortcut);
    static QString stableId(const QJsonObject &action, int index);
    bool registerAction(QAction *action, const QKeySequence &sequence);

    Logger *m_logger;
    bool m_stealConflicts;
    bool m_gameActive = true;
    bool m_gameKnown = false;
    QList<QAction *> m_actions;
    QList<QAction *> m_captureActions;
    QString m_captureToken;
    QList<QAction *> m_numberCaptureActions;
    QString m_numberCaptureToken;
    QList<QAction *> m_textCaptureActions;
    QString m_textCaptureToken;
    QAction *m_overlayEscapeAction = nullptr;
    bool m_overlayInteractive = false;
    QTimer m_captureTimer;
};

} // namespace AptNative
