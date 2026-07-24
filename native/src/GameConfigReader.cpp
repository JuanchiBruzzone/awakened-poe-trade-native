#include "GameConfigReader.h"

#include "Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>

namespace AptNative {

GameConfigReader::GameConfigReader(Logger *logger, QObject *parent)
    : QObject(parent), m_logger(logger) {}

QString GameConfigReader::showModsKey() const
{
    return m_showModsKey;
}

QString GameConfigReader::actualPath() const
{
    return m_actualPath;
}

QString GameConfigReader::guessPath()
{
    const QString home = QDir::homePath();
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QStringList candidates{
        QDir(docs).filePath(QStringLiteral("My Games/Path of Exile/production_Config.ini")),
        QDir(home).filePath(QStringLiteral(".local/share/Steam/steamapps/compatdata/238960/pfx/drive_c/users/steamuser/Documents/My Games/Path of Exile/production_Config.ini")),
        QDir(home).filePath(QStringLiteral(".steam/steam/steamapps/compatdata/238960/pfx/drive_c/users/steamuser/Documents/My Games/Path of Exile/production_Config.ini"))
    };

    QSet<QString> found;
    for (const QString &candidate : candidates) {
        const QFileInfo info(candidate);
        if (!info.isFile()) continue;
        const QString canonical = info.canonicalFilePath();
        if (!canonical.isEmpty()) found.insert(canonical);
    }
    return found.size() == 1 ? *found.constBegin() : QString{};
}

QString GameConfigReader::keyNameForCode(const QString &code)
{
    static const QHash<int, QString> names = [] {
        QHash<int, QString> map{
            {8, QStringLiteral("Backspace")}, {9, QStringLiteral("Tab")},
            {13, QStringLiteral("Enter")}, {16, QStringLiteral("Shift")},
            {17, QStringLiteral("Ctrl")}, {18, QStringLiteral("Alt")},
            {20, QStringLiteral("CapsLock")}, {27, QStringLiteral("Escape")},
            {32, QStringLiteral("Space")}, {33, QStringLiteral("PageUp")},
            {34, QStringLiteral("PageDown")}, {35, QStringLiteral("End")},
            {36, QStringLiteral("Home")}, {37, QStringLiteral("Left")},
            {38, QStringLiteral("Up")}, {39, QStringLiteral("Right")},
            {40, QStringLiteral("Down")}, {45, QStringLiteral("Insert")},
            {46, QStringLiteral("Delete")}
        };
        for (int i = 0; i <= 9; ++i) map.insert(48 + i, QString::number(i));
        for (int i = 0; i < 26; ++i) map.insert(65 + i, QString(QChar(static_cast<char16_t>(u'A' + i))));
        for (int i = 1; i <= 24; ++i) map.insert(111 + i, QStringLiteral("F%1").arg(i));
        return map;
    }();

    bool ok = false;
    const int number = code.toInt(&ok);
    return ok ? names.value(number) : QString{};
}

void GameConfigReader::readConfig(const QString &requestedPath)
{
    QString path = requestedPath.trimmed();
    if (path.isEmpty()) {
        path = guessPath();
    }
    if (path.isEmpty()) {
        m_actualPath.clear();
        m_showModsKey = QStringLiteral("Alt");
        m_logger->write(QStringLiteral("warn [GameConfig] Could not uniquely locate production_Config.ini; using Alt."));
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_actualPath.clear();
        m_showModsKey = QStringLiteral("Alt");
        m_logger->write(QStringLiteral("error [GameConfig] Failed to read production_Config.ini; using Alt."));
        return;
    }

    bool inActionKeys = false;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (!line.isEmpty() && line.front() == QChar(0xFEFF)) {
            line.remove(0, 1);
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            inActionKeys = line.compare(QStringLiteral("[ACTION_KEYS]"), Qt::CaseInsensitive) == 0;
            continue;
        }
        if (!inActionKeys || !line.startsWith(QStringLiteral("show_advanced_item_descriptions="))) {
            continue;
        }

        const QString raw = line.section(QLatin1Char('='), 1).trimmed();
        const QStringList parts = raw.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.isEmpty()) break;

        const QString mainKey = keyNameForCode(parts.at(0));
        if (mainKey.isEmpty()) {
            m_logger->write(QStringLiteral("warn [GameConfig] Unknown advanced-description key code: %1").arg(raw));
            break;
        }

        QStringList shortcut;
        if (parts.size() > 1) {
            const QString modifier = parts.at(1);
            if (modifier == QStringLiteral("1")) shortcut << QStringLiteral("Shift");
            else if (modifier == QStringLiteral("2")) shortcut << QStringLiteral("Ctrl");
            else if (modifier == QStringLiteral("3")) shortcut << QStringLiteral("Alt");
        }
        shortcut << mainKey;
        m_showModsKey = shortcut.join(QStringLiteral(" + "));
        m_actualPath = path;
        m_logger->write(QStringLiteral("info [GameConfig] Advanced descriptions key: %1").arg(m_showModsKey));
        return;
    }

    m_actualPath = path;
    m_showModsKey = QStringLiteral("Alt");
    m_logger->write(QStringLiteral("warn [GameConfig] Advanced-description binding not found; using Alt."));
}

} // namespace AptNative
