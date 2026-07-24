#include "Logger.h"

#include <QDateTime>
#include <QDebug>

namespace AptNative {

Logger::Logger(QObject *parent) : QObject(parent) {}

QString Logger::history() const
{
    return m_history;
}

void Logger::write(const QString &message)
{
    const QString line = QStringLiteral("[%1] %2\n")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), message);
    m_history.append(line);
    qInfo().noquote() << line.trimmed();
    emit entry(line);
}

} // namespace AptNative
