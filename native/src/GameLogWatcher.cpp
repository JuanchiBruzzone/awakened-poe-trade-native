#include "GameLogWatcher.h"

#include "Logger.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace AptNative {

GameLogWatcher::GameLogWatcher(Logger *logger, QObject *parent)
    : QObject(parent), m_logger(logger)
{
    m_timer.setInterval(450);
    connect(&m_timer, &QTimer::timeout, this, &GameLogWatcher::poll);
}

QString GameLogWatcher::guessPath()
{
    const QString home = QDir::homePath();
    const QStringList candidates{
        QDir(home).filePath(QStringLiteral(".wine/drive_c/Program Files (x86)/Grinding Gear Games/Path of Exile/logs/Client.txt")),
        QDir(home).filePath(QStringLiteral(".local/share/Steam/steamapps/common/Path of Exile/logs/Client.txt")),
        QDir(home).filePath(QStringLiteral(".steam/steam/steamapps/common/Path of Exile/logs/Client.txt"))
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

void GameLogWatcher::restart(const QString &requestedPath)
{
    m_timer.stop();
    if (m_file.isOpen()) m_file.close();

    QString path = requestedPath.trimmed();
    if (path.isEmpty()) path = guessPath();
    if (path.isEmpty()) {
        m_logger->write(QStringLiteral("warn [GameLogWatcher] Client.txt was not uniquely detected."));
        return;
    }

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_logger->write(QStringLiteral("error [GameLogWatcher] Failed to open Client.txt."));
        return;
    }
    m_offset = m_file.size();
    m_timer.start();
    m_logger->write(QStringLiteral("info [GameLogWatcher] Watching %1").arg(path));
}

QString GameLogWatcher::actualPath() const
{
    return m_file.fileName();
}

void GameLogWatcher::poll()
{
    if (!m_file.isOpen()) return;
    const qint64 size = m_file.size();
    if (size < m_offset) m_offset = 0;
    if (size == m_offset) return;

    if (!m_file.seek(m_offset)) return;
    const QByteArray added = m_file.read(size - m_offset);
    m_offset = m_file.pos();

    QStringList lines;
    const QList<QByteArray> rawLines = added.split('\n');
    for (const QByteArray &raw : rawLines) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (!line.isEmpty()) lines.append(line);
    }
    if (!lines.isEmpty()) emit linesRead(lines);
}

} // namespace AptNative
