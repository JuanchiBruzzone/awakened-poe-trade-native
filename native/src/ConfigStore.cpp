#include "ConfigStore.h"

#include "Logger.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

namespace AptNative {

ConfigStore::ConfigStore(Logger *logger, QObject *parent)
    : QObject(parent), m_logger(logger)
{
    const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    m_dataDirectory = QDir(configRoot).filePath(QStringLiteral("awakened-poe-trade/apt-data"));
}

QString ConfigStore::dataDirectory() const
{
    return m_dataDirectory;
}

QString ConfigStore::uploadsDirectory() const
{
    return QDir(m_dataDirectory).filePath(QStringLiteral("files"));
}

QString ConfigStore::currentConfigPath(bool temporary) const
{
    const bool useTemp = temporary || m_usingTemporaryFile;
    return QDir(m_dataDirectory).filePath(useTemp
        ? QStringLiteral("config.json.tmp")
        : QStringLiteral("config.json"));
}

QString ConfigStore::load() const
{
    const QString temporaryPath = currentConfigPath(true);
    const QString normalPath = currentConfigPath(false);
    const QString path = QFile::exists(temporaryPath) ? temporaryPath : normalPath;
    m_usingTemporaryFile = QFile::exists(temporaryPath);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool ConfigStore::save(const QString &contents, bool temporary)
{
    if (temporary) {
        m_usingTemporaryFile = true;
    }

    if (!QDir().mkpath(m_dataDirectory)) {
        m_logger->write(QStringLiteral("error [ConfigStore] Failed to create data directory."));
        return false;
    }

    QSaveFile file(currentConfigPath(temporary));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_logger->write(QStringLiteral("error [ConfigStore] Failed to open configuration for writing."));
        return false;
    }
    if (file.write(contents.toUtf8()) < 0 || !file.commit()) {
        m_logger->write(QStringLiteral("error [ConfigStore] Failed to atomically save configuration."));
        return false;
    }
    return true;
}

} // namespace AptNative
