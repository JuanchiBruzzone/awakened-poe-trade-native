#pragma once

#include <QObject>
#include <QString>

namespace AptNative {

class Logger;

class ConfigStore final : public QObject {
    Q_OBJECT
public:
    explicit ConfigStore(Logger *logger, QObject *parent = nullptr);

    QString load() const;
    bool save(const QString &contents, bool temporary);
    QString dataDirectory() const;
    QString uploadsDirectory() const;

private:
    QString currentConfigPath(bool temporary) const;

    Logger *m_logger;
    QString m_dataDirectory;
    mutable bool m_usingTemporaryFile = false;
};

} // namespace AptNative
