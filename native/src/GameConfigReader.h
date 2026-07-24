#pragma once

#include <QObject>
#include <QString>

namespace AptNative {

class Logger;

class GameConfigReader final : public QObject {
    Q_OBJECT
public:
    explicit GameConfigReader(Logger *logger, QObject *parent = nullptr);

    void readConfig(const QString &requestedPath);
    QString showModsKey() const;
    QString actualPath() const;

private:
    static QString guessPath();
    static QString keyNameForCode(const QString &code);

    Logger *m_logger;
    QString m_showModsKey = QStringLiteral("Alt");
    QString m_actualPath;
};

} // namespace AptNative
