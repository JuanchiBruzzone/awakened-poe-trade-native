#include "ShortcutManager.h"

#include "Logger.h"
#include "Types.h"

#include <KGlobalAccel>

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QKeySequence>
#include <QRegularExpression>
#include <QSet>

#include <utility>

namespace AptNative {

ShortcutManager::ShortcutManager(Logger *logger, bool stealConflicts, QObject *parent)
    : QObject(parent), m_logger(logger), m_stealConflicts(stealConflicts)
{
    m_captureTimer.setSingleShot(true);
    m_captureTimer.setInterval(10000);
    connect(&m_captureTimer, &QTimer::timeout, this, [this] {
        cancelLetterCapture();
        cancelNumberCapture();
    });
}

ShortcutManager::~ShortcutManager()
{
    clear();
}

QKeySequence ShortcutManager::parseSequence(const QString &shortcut)
{
    QString normalized = shortcut;
    normalized.replace(QStringLiteral("ArrowLeft"), QStringLiteral("Left"));
    normalized.replace(QStringLiteral("ArrowRight"), QStringLiteral("Right"));
    normalized.replace(QStringLiteral("ArrowUp"), QStringLiteral("Up"));
    normalized.replace(QStringLiteral("ArrowDown"), QStringLiteral("Down"));
    normalized.replace(QRegularExpression(QStringLiteral("\\s*\\+\\s*")), QStringLiteral("+"));
    return QKeySequence::fromString(normalized, QKeySequence::PortableText);
}

QString ShortcutManager::stableId(const QJsonObject &action, int index)
{
    const QString type = action.value(QStringLiteral("type")).toString(QStringLiteral("unknown"));
    QString identity = action.value(QStringLiteral("target")).toString();
    if (identity.isEmpty()) identity = action.value(QStringLiteral("text")).toString();
    const QByteArray hash = QCryptographicHash::hash(
        QJsonDocument(action).toJson(QJsonDocument::Compact), QCryptographicHash::Sha1).toHex().left(10);
    return QStringLiteral("apt-native-%1-%2-%3")
        .arg(type, identity.left(24).replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), QStringLiteral("_")))
        .arg(index)
        .append(QLatin1Char('-')).append(QString::fromLatin1(hash));
}

bool ShortcutManager::registerAction(QAction *action, const QKeySequence &sequence)
{
    if (sequence.isEmpty()) return false;

    KGlobalAccel::self()->setDefaultShortcut(action, {sequence}, KGlobalAccel::NoAutoloading);
    auto tryRegister = [action, &sequence] {
        const bool set = KGlobalAccel::self()->setShortcut(
            action, {sequence}, KGlobalAccel::NoAutoloading);
        return set && KGlobalAccel::self()->shortcut(action).contains(sequence);
    };

    if (tryRegister()) return true;

    if (m_stealConflicts) {
        KGlobalAccel::stealShortcutSystemwide(sequence);
        m_logger->write(QStringLiteral("warn [Shortcuts] Stole global shortcut by explicit request: %1")
            .arg(sequence.toString(QKeySequence::NativeText)));
        if (tryRegister()) return true;
    }

    m_logger->write(QStringLiteral("warn [Shortcuts] Could not register global shortcut, probably due to a conflict: %1")
        .arg(sequence.toString(QKeySequence::NativeText)));
    return false;
}

void ShortcutManager::clear()
{
    cancelLetterCapture();
    cancelNumberCapture();
    for (QAction *action : std::as_const(m_actions)) {
        KGlobalAccel::self()->removeAllShortcuts(action);
        action->deleteLater();
    }
    m_actions.clear();
}

void ShortcutManager::beginLetterCapture(const QString &token)
{
    cancelLetterCapture();
    cancelNumberCapture();
    if (token.isEmpty()) return;
    m_captureToken = token;

    QStringList keys{QStringLiteral("Backspace")};
    for (QChar key = QLatin1Char('A'); key <= QLatin1Char('Z');
         key = QChar(key.unicode() + 1)) {
        keys.append(QString(key));
    }

    for (const QString &key : keys) {
        auto *action = new QAction(this);
        action->setObjectName(QStringLiteral("apt-native-letter-capture-%1").arg(key));
        action->setText(QStringLiteral("Awakened PoE Trade: Capture %1").arg(key));
        connect(action, &QAction::triggered, this, [this, key] {
            const QString capturedToken = m_captureToken;
            const QString captured =
                key == QStringLiteral("Backspace") ? QString{} : key;
            cancelLetterCapture();
            if (!capturedToken.isEmpty()) emit letterCaptured(capturedToken, captured);
        });
        if (registerAction(action, parseSequence(key))) {
            action->setEnabled(true);
            m_captureActions.append(action);
        } else {
            action->deleteLater();
        }
    }

    m_captureTimer.start();
    m_logger->write(QStringLiteral("debug [Shortcuts] Letter capture armed with %1 keys.")
        .arg(m_captureActions.size()));
}

void ShortcutManager::beginNumberCapture(const QString &token)
{
    cancelLetterCapture();
    cancelNumberCapture();
    if (token.isEmpty()) return;
    m_numberCaptureToken = token;

    QStringList keys{
        QStringLiteral("Backspace"), QStringLiteral("Delete"),
        QStringLiteral("Enter"), QStringLiteral("Escape"),
        QStringLiteral("."), QStringLiteral("-")
    };
    for (int digit = 0; digit <= 9; ++digit) {
        keys.append(QString::number(digit));
    }

    for (const QString &key : keys) {
        auto *action = new QAction(this);
        action->setObjectName(
            QStringLiteral("apt-native-number-capture-%1").arg(key));
        action->setText(
            QStringLiteral("Awakened PoE Trade: Capture number %1").arg(key));
        connect(action, &QAction::triggered, this, [this, key] {
            const QString capturedToken = m_numberCaptureToken;
            if (capturedToken.isEmpty()) return;
            emit numberCaptured(capturedToken, key);
            if (key == QStringLiteral("Enter") ||
                key == QStringLiteral("Escape")) {
                cancelNumberCapture();
            } else {
                m_captureTimer.start();
            }
        });
        if (registerAction(action, parseSequence(key))) {
            action->setEnabled(true);
            m_numberCaptureActions.append(action);
        } else {
            action->deleteLater();
        }
    }

    m_captureTimer.start();
    m_logger->write(
        QStringLiteral("debug [Shortcuts] Number capture armed with %1 keys.")
            .arg(m_numberCaptureActions.size()));
}

void ShortcutManager::cancelLetterCapture()
{
    m_captureTimer.stop();
    m_captureToken.clear();
    for (QAction *action : std::as_const(m_captureActions)) {
        KGlobalAccel::self()->removeAllShortcuts(action);
        action->deleteLater();
    }
    m_captureActions.clear();
}

void ShortcutManager::cancelNumberCapture()
{
    m_captureTimer.stop();
    m_numberCaptureToken.clear();
    for (QAction *action : std::as_const(m_numberCaptureActions)) {
        KGlobalAccel::self()->removeAllShortcuts(action);
        action->deleteLater();
    }
    m_numberCaptureActions.clear();
}

void ShortcutManager::setGameActive(bool active, bool known)
{
    m_gameActive = active;
    m_gameKnown = known;
    for (QAction *action : std::as_const(m_actions)) {
        const bool overlayToggle = action->property("aptToggleOverlay").toBool();
        action->setEnabled(!known || active || overlayToggle);
    }
}

void ShortcutManager::update(const HostConfig &config, const QStringList &reservedShortcuts)
{
    clear();
    QSet<QString> seenShortcuts;
    QSet<QString> reserved;
    for (const QString &text : reservedShortcuts) {
        const QKeySequence sequence = parseSequence(text);
        if (!sequence.isEmpty()) reserved.insert(sequence.toString(QKeySequence::PortableText));
    }
    QHash<QByteArray, int> actionOccurrences;
    for (const QJsonValue &value : config.shortcuts) {
        const QJsonObject entry = value.toObject();
        const QString shortcutText = entry.value(QStringLiteral("shortcut")).toString().trimmed();
        const QJsonObject actionObject = entry.value(QStringLiteral("action")).toObject();
        if (shortcutText.isEmpty() || actionObject.isEmpty()) {
            continue;
        }
        const QKeySequence sequence = parseSequence(shortcutText);
        if (sequence.isEmpty()) {
            m_logger->write(QStringLiteral("warn [Shortcuts] Unsupported shortcut syntax: %1").arg(shortcutText));
            continue;
        }
        const QString portable = sequence.toString(QKeySequence::PortableText);
        const QString actionType = actionObject.value(QStringLiteral("type")).toString();
        if (actionType != QStringLiteral("test-only") && reserved.contains(portable)) {
            m_logger->write(QStringLiteral("error [Shortcuts] Hotkey reserved by the game was not registered: %1")
                .arg(shortcutText));
            continue;
        }
        if (seenShortcuts.contains(portable)) {
            m_logger->write(QStringLiteral("warn [Shortcuts] Duplicate shortcut ignored: %1").arg(shortcutText));
            continue;
        }
        seenShortcuts.insert(portable);

        const QByteArray actionIdentity = QJsonDocument(actionObject).toJson(QJsonDocument::Compact);
        const int occurrence = actionOccurrences.value(actionIdentity, 0);
        actionOccurrences.insert(actionIdentity, occurrence + 1);

        auto *action = new QAction(this);
        action->setObjectName(stableId(actionObject, occurrence));
        action->setText(QStringLiteral("Awakened PoE Trade: %1")
            .arg(actionObject.value(QStringLiteral("type")).toString()));
        action->setProperty("aptToggleOverlay", actionType == QStringLiteral("toggle-overlay"));
        const bool keepModKeys = entry.value(QStringLiteral("keepModKeys")).toBool(false);
        connect(action, &QAction::triggered, this,
                [this, actionObject, shortcutText, keepModKeys] {
            emit actionTriggered(actionObject, shortcutText, keepModKeys);
        });
        if (registerAction(action, sequence)) {
            if (actionType == QStringLiteral("test-only")) {
                KGlobalAccel::self()->removeAllShortcuts(action);
                action->deleteLater();
            } else {
                action->setEnabled(!m_gameKnown || m_gameActive ||
                                   action->property("aptToggleOverlay").toBool());
                m_actions.append(action);
            }
        } else {
            action->deleteLater();
        }
    }

    if (!config.overlayKey.trimmed().isEmpty()) {
        const QKeySequence sequence = parseSequence(config.overlayKey);
        if (!sequence.isEmpty() && !seenShortcuts.contains(sequence.toString(QKeySequence::PortableText))) {
            auto *action = new QAction(this);
            action->setObjectName(QStringLiteral("apt-native-overlay-toggle"));
            action->setText(QStringLiteral("Awakened PoE Trade: Toggle overlay"));
            action->setProperty("aptToggleOverlay", true);
            const QString shortcut = config.overlayKey;
            connect(action, &QAction::triggered, this, [this, shortcut] {
                emit overlayToggleTriggered(shortcut);
            });
            if (registerAction(action, sequence)) {
                action->setEnabled(true);
                m_actions.append(action);
            }
            else action->deleteLater();
        }
    }

    m_logger->write(QStringLiteral("info [Shortcuts] Registered %1 global shortcuts.").arg(m_actions.size()));
}

} // namespace AptNative
