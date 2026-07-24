#pragma once

#include <QFile>
#include <QObject>
#include <QTimer>
#include <QStringList>

namespace AptNative {

class Logger;

class GameLogWatcher final : public QObject {
    Q_OBJECT
public:
    explicit GameLogWatcher(Logger *logger, QObject *parent = nullptr);

    void restart(const QString &requestedPath);
    QString actualPath() const;

signals:
    void linesRead(const QStringList &lines);

private slots:
    void poll();

private:
    static QString guessPath();

    Logger *m_logger;
    QFile m_file;
    qint64 m_offset = 0;
    QTimer m_timer;
};

} // namespace AptNative
