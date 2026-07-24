#pragma once

#include <QClipboard>
#include <QObject>
#include <QTimer>

#include <functional>

namespace AptNative {

class Logger;

class ClipboardService final : public QObject {
    Q_OBJECT
public:
    explicit ClipboardService(Logger *logger, QObject *parent = nullptr);

    void beginItemCapture(bool restoreClipboard,
                          std::function<void(const QString &)> success,
                          std::function<void()> failure = {});
    bool isCapturing() const;
    void cancel();

    bool withTemporaryText(const QString &text,
                           bool restoreClipboard,
                           const std::function<void()> &action,
                           int restoreAfterMs = 120);

    static bool isPoeItem(const QString &text);

private slots:
    void poll();

private:
    Logger *m_logger;
    QClipboard *m_clipboard;
    QTimer m_pollTimer;
    int m_elapsedMs = 0;
    QString m_before;
    bool m_restore = false;
    std::function<void(const QString &)> m_success;
    std::function<void()> m_failure;
    bool m_temporaryActive = false;
};

} // namespace AptNative
