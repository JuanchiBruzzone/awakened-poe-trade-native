#include "ClipboardService.h"

#include "Logger.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>

namespace AptNative {

namespace {
constexpr int PollDelayMs = 48;
constexpr int PollLimitMs = 1200;
}

ClipboardService::ClipboardService(Logger *logger, QObject *parent)
    : QObject(parent), m_logger(logger), m_clipboard(QGuiApplication::clipboard())
{
    m_pollTimer.setInterval(PollDelayMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &ClipboardService::poll);
}

bool ClipboardService::isPoeItem(const QString &text)
{
    static const QStringList prefixes{
        QStringLiteral("Item Class: "),
        QStringLiteral("Класс предмета: "),
        QStringLiteral("Classe d'objet: "),
        QStringLiteral("Gegenstandsklasse: "),
        QStringLiteral("Classe do Item: "),
        QStringLiteral("Clase de objeto: "),
        QStringLiteral("ชนิดไอเทม: "),
        QStringLiteral("아이템 종류: "),
        QStringLiteral("物品種類: "),
        QStringLiteral("物品类别: ")
    };
    for (const QString &prefix : prefixes) {
        if (text.startsWith(prefix)) return true;
    }
    return false;
}

void ClipboardService::beginItemCapture(bool restoreClipboard,
                                        std::function<void(const QString &)> success,
                                        std::function<void()> failure)
{
    cancel();
    m_restore = restoreClipboard;
    m_success = std::move(success);
    m_failure = std::move(failure);
    m_before = m_clipboard->text(QClipboard::Clipboard);
    m_elapsedMs = 0;

    if (isPoeItem(m_before)) m_before.clear();
    // KDE can reject an empty clipboard and Proton 10+ may otherwise leave the
    // old selection in place. The marker guarantees that a later PoE item is new.
    m_clipboard->setText(
        QStringLiteral("__APT_FORCE_EMPTY_%1").arg(QDateTime::currentMSecsSinceEpoch()),
        QClipboard::Clipboard);
    m_pollTimer.start();
}

void ClipboardService::cancel()
{
    if (!m_pollTimer.isActive()) return;
    m_pollTimer.stop();
    if (m_restore) m_clipboard->setText(m_before, QClipboard::Clipboard);
    m_success = {};
    m_failure = {};
}

bool ClipboardService::isCapturing() const
{
    return m_pollTimer.isActive();
}

void ClipboardService::poll()
{
    QString after = m_clipboard->text(QClipboard::Clipboard);
    if (!isPoeItem(after) &&
        !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        const QString wlPaste =
            QStandardPaths::findExecutable(QStringLiteral("wl-paste"));
        if (!wlPaste.isEmpty()) {
            QProcess process;
            process.start(wlPaste, {QStringLiteral("--no-newline")});
            if (process.waitForFinished(160) &&
                process.exitStatus() == QProcess::NormalExit &&
                process.exitCode() == 0) {
                const QString waylandText =
                    QString::fromUtf8(process.readAllStandardOutput());
                if (isPoeItem(waylandText)) after = waylandText;
            } else {
                process.kill();
                process.waitForFinished(50);
            }
        }
    }
    if (isPoeItem(after)) {
        m_pollTimer.stop();
        if (m_restore) m_clipboard->setText(m_before, QClipboard::Clipboard);
        const auto callback = std::move(m_success);
        m_failure = {};
        if (callback) callback(after);
        return;
    }

    m_elapsedMs += PollDelayMs;
    if (m_elapsedMs < PollLimitMs) return;

    m_pollTimer.stop();
    if (m_restore) m_clipboard->setText(m_before, QClipboard::Clipboard);
    m_logger->write(QStringLiteral("warn [ClipboardPoller] No Path of Exile item text found."));
    const auto callback = std::move(m_failure);
    m_success = {};
    if (callback) callback();
}

bool ClipboardService::withTemporaryText(const QString &text,
                                         bool restoreClipboard,
                                         const std::function<void()> &action,
                                         int restoreAfterMs)
{
    if (m_temporaryActive) return false;
    m_temporaryActive = true;
    const QString saved = m_clipboard->text(QClipboard::Clipboard);
    m_clipboard->setText(text, QClipboard::Clipboard);
    action();
    QTimer::singleShot(restoreAfterMs, this, [this, saved, restoreClipboard] {
        if (restoreClipboard) {
            m_clipboard->setText(saved, QClipboard::Clipboard);
        }
        m_temporaryActive = false;
    });
    return true;
}

} // namespace AptNative
