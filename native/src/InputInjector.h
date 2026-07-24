#pragma once

#include <QList>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>

#include <functional>

namespace AptNative {

class Logger;

class InputInjector final : public QObject {
    Q_OBJECT
public:
    explicit InputInjector(Logger *logger, QObject *parent = nullptr);

    bool isAvailable() const;
    void sendChord(const QString &requiredChord,
                   const QString &triggeringShortcut = {},
                   std::function<void(bool)> done = {});
    void sendSequence(const QStringList &chords,
                      std::function<void(bool)> done = {});
    void releaseShortcut(const QString &shortcut,
                         bool keepModifiers,
                         std::function<void(bool)> done = {});
    void tapKey(const QString &key, std::function<void(bool)> done = {});
    void typeText(const QString &text, std::function<void(bool)> done = {});

private:
    struct Command {
        QString program;
        QStringList arguments;
        int delayAfterMs = 0;
    };
    struct Job {
        QList<Command> commands;
        int index = 0;
        std::function<void(bool)> done;
        bool success = true;
    };

    static QStringList tokens(const QString &shortcut);
    static int linuxKeyCode(const QString &key);
    void enqueue(QList<Command> commands, std::function<void(bool)> done);
    void startNextJob();
    void runCurrentCommand();

    Logger *m_logger;
    QString m_ydotool;
    QQueue<Job> m_jobs;
    bool m_running = false;
};

} // namespace AptNative
