#include "InputInjector.h"

#include "Logger.h"

#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>

namespace AptNative {

InputInjector::InputInjector(Logger *logger, QObject *parent)
    : QObject(parent), m_logger(logger),
      m_ydotool(QStandardPaths::findExecutable(QStringLiteral("ydotool")))
{
    if (m_ydotool.isEmpty()) {
        m_logger->write(QStringLiteral("error [InputInjector] ydotool was not found in PATH."));
    }
}

bool InputInjector::isAvailable() const
{
    return !m_ydotool.isEmpty();
}

QStringList InputInjector::tokens(const QString &shortcut)
{
    QString normalized = shortcut;
    normalized.replace(QStringLiteral("ArrowLeft"), QStringLiteral("Left"));
    normalized.replace(QStringLiteral("ArrowRight"), QStringLiteral("Right"));
    normalized.replace(QStringLiteral("ArrowUp"), QStringLiteral("Up"));
    normalized.replace(QStringLiteral("ArrowDown"), QStringLiteral("Down"));
    normalized.replace(QStringLiteral("+"), QStringLiteral(" "));
    return normalized.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

int InputInjector::linuxKeyCode(const QString &key)
{
    static const QHash<QString, int> map{
        {QStringLiteral("Escape"), 1},
        {QStringLiteral("1"), 2}, {QStringLiteral("2"), 3}, {QStringLiteral("3"), 4},
        {QStringLiteral("4"), 5}, {QStringLiteral("5"), 6}, {QStringLiteral("6"), 7},
        {QStringLiteral("7"), 8}, {QStringLiteral("8"), 9}, {QStringLiteral("9"), 10},
        {QStringLiteral("0"), 11}, {QStringLiteral("Minus"), 12}, {QStringLiteral("Equal"), 13},
        {QStringLiteral("Backspace"), 14}, {QStringLiteral("Tab"), 15},
        {QStringLiteral("Q"), 16}, {QStringLiteral("W"), 17}, {QStringLiteral("E"), 18},
        {QStringLiteral("R"), 19}, {QStringLiteral("T"), 20}, {QStringLiteral("Y"), 21},
        {QStringLiteral("U"), 22}, {QStringLiteral("I"), 23}, {QStringLiteral("O"), 24},
        {QStringLiteral("P"), 25}, {QStringLiteral("BracketLeft"), 26},
        {QStringLiteral("BracketRight"), 27}, {QStringLiteral("Enter"), 28},
        {QStringLiteral("Ctrl"), 29}, {QStringLiteral("A"), 30}, {QStringLiteral("S"), 31},
        {QStringLiteral("D"), 32}, {QStringLiteral("F"), 33}, {QStringLiteral("G"), 34},
        {QStringLiteral("H"), 35}, {QStringLiteral("J"), 36}, {QStringLiteral("K"), 37},
        {QStringLiteral("L"), 38}, {QStringLiteral("Semicolon"), 39},
        {QStringLiteral("Quote"), 40}, {QStringLiteral("Backquote"), 41},
        {QStringLiteral("Shift"), 42}, {QStringLiteral("Backslash"), 43},
        {QStringLiteral("Z"), 44}, {QStringLiteral("X"), 45}, {QStringLiteral("C"), 46},
        {QStringLiteral("V"), 47}, {QStringLiteral("B"), 48}, {QStringLiteral("N"), 49},
        {QStringLiteral("M"), 50}, {QStringLiteral("Comma"), 51},
        {QStringLiteral("Period"), 52}, {QStringLiteral("Slash"), 53},
        {QStringLiteral("Alt"), 56}, {QStringLiteral("Space"), 57},
        {QStringLiteral("F1"), 59}, {QStringLiteral("F2"), 60}, {QStringLiteral("F3"), 61},
        {QStringLiteral("F4"), 62}, {QStringLiteral("F5"), 63}, {QStringLiteral("F6"), 64},
        {QStringLiteral("F7"), 65}, {QStringLiteral("F8"), 66}, {QStringLiteral("F9"), 67},
        {QStringLiteral("F10"), 68}, {QStringLiteral("F11"), 87}, {QStringLiteral("F12"), 88},
        {QStringLiteral("Home"), 102}, {QStringLiteral("Up"), 103},
        {QStringLiteral("PageUp"), 104}, {QStringLiteral("Left"), 105},
        {QStringLiteral("Right"), 106}, {QStringLiteral("End"), 107},
        {QStringLiteral("Down"), 108}, {QStringLiteral("PageDown"), 109},
        {QStringLiteral("Insert"), 110}, {QStringLiteral("Delete"), 111}
    };
    return map.value(key, -1);
}

void InputInjector::sendChord(const QString &requiredChord,
                              const QString &triggeringShortcut,
                              std::function<void(bool)> done)
{
    if (!isAvailable()) {
        if (done) done(false);
        return;
    }

    const QStringList required = tokens(requiredChord);
    const QStringList heldList = tokens(triggeringShortcut);
    const QSet<QString> held(heldList.cbegin(), heldList.cend());
    const QSet<QString> modifiers{QStringLiteral("Ctrl"), QStringLiteral("Shift"), QStringLiteral("Alt")};

    QStringList args{
        QStringLiteral("key"),
        QStringLiteral("--key-delay"),
        QStringLiteral("18")
    };
    QList<int> syntheticModifiers;
    QList<int> normalKeys;

    for (const QString &key : required) {
        const int code = linuxKeyCode(key);
        if (code < 0) {
            m_logger->write(QStringLiteral("error [InputInjector] Unsupported key: %1").arg(key));
            if (done) done(false);
            return;
        }
        if (modifiers.contains(key)) {
            if (!held.contains(key)) syntheticModifiers.append(code);
        } else {
            normalKeys.append(code);
        }
    }

    for (const int code : syntheticModifiers) args << QStringLiteral("%1:1").arg(code);
    for (const int code : normalKeys) {
        args << QStringLiteral("%1:1").arg(code) << QStringLiteral("%1:0").arg(code);
    }
    for (auto it = syntheticModifiers.crbegin(); it != syntheticModifiers.crend(); ++it) {
        args << QStringLiteral("%1:0").arg(*it);
    }

    enqueue({Command{m_ydotool, args, 20}}, std::move(done));
}

void InputInjector::sendSequence(const QStringList &chords,
                                 std::function<void(bool)> done)
{
    if (!isAvailable()) {
        if (done) done(false);
        return;
    }

    const QSet<QString> modifiers{
        QStringLiteral("Ctrl"), QStringLiteral("Shift"), QStringLiteral("Alt")
    };
    QStringList args{QStringLiteral("key")};
    for (const QString &chord : chords) {
        QList<int> chordModifiers;
        QList<int> normalKeys;
        for (const QString &key : tokens(chord)) {
            const int code = linuxKeyCode(key);
            if (code < 0) {
                m_logger->write(QStringLiteral("error [InputInjector] Unsupported key: %1")
                    .arg(key));
                if (done) done(false);
                return;
            }
            if (modifiers.contains(key)) chordModifiers.append(code);
            else normalKeys.append(code);
        }
        for (const int code : chordModifiers) {
            args << QStringLiteral("%1:1").arg(code);
        }
        for (const int code : normalKeys) {
            args << QStringLiteral("%1:1").arg(code)
                 << QStringLiteral("%1:0").arg(code);
        }
        for (auto it = chordModifiers.crbegin(); it != chordModifiers.crend(); ++it) {
            args << QStringLiteral("%1:0").arg(*it);
        }
    }
    enqueue({Command{m_ydotool, args, 20}}, std::move(done));
}

void InputInjector::releaseShortcut(const QString &shortcut,
                                    bool keepModifiers,
                                    std::function<void(bool)> done)
{
    if (!isAvailable()) {
        if (done) done(false);
        return;
    }

    const QSet<QString> modifiers{
        QStringLiteral("Ctrl"), QStringLiteral("Shift"), QStringLiteral("Alt")
    };
    QStringList args{QStringLiteral("key")};
    const QStringList keys = tokens(shortcut);
    for (auto it = keys.crbegin(); it != keys.crend(); ++it) {
        if (keepModifiers && modifiers.contains(*it)) continue;
        const int code = linuxKeyCode(*it);
        if (code < 0) {
            m_logger->write(QStringLiteral("error [InputInjector] Unsupported release key: %1").arg(*it));
            if (done) done(false);
            return;
        }
        args << QStringLiteral("%1:0").arg(code);
    }

    if (args.size() == 1) {
        if (done) done(true);
        return;
    }
    enqueue({Command{m_ydotool, args, 12}}, std::move(done));
}

void InputInjector::tapKey(const QString &key, std::function<void(bool)> done)
{
    sendChord(key, {}, std::move(done));
}

void InputInjector::typeText(const QString &text, std::function<void(bool)> done)
{
    if (!isAvailable()) {
        if (done) done(false);
        return;
    }
    enqueue({Command{m_ydotool,
                     {QStringLiteral("type"), QStringLiteral("--key-delay"), QStringLiteral("12"), text},
                     20}}, std::move(done));
}

void InputInjector::enqueue(QList<Command> commands, std::function<void(bool)> done)
{
    if (commands.isEmpty()) {
        if (done) done(true);
        return;
    }
    m_jobs.enqueue(Job{std::move(commands), 0, std::move(done), true});
    startNextJob();
}

void InputInjector::startNextJob()
{
    if (m_running || m_jobs.isEmpty()) return;
    m_running = true;
    runCurrentCommand();
}

void InputInjector::runCurrentCommand()
{
    if (m_jobs.isEmpty()) {
        m_running = false;
        return;
    }

    Job &job = m_jobs.head();
    if (job.index >= job.commands.size()) {
        const auto done = std::move(job.done);
        const bool success = job.success;
        m_jobs.dequeue();
        m_running = false;
        if (done) done(success);
        startNextJob();
        return;
    }

    const Command command = job.commands.at(job.index++);
    auto *process = new QProcess(this);
    auto *timeout = new QTimer(process);
    timeout->setSingleShot(true);

    connect(timeout, &QTimer::timeout, process, [this, process] {
        m_logger->write(QStringLiteral("error [InputInjector] ydotool command timed out."));
        process->kill();
    });
    connect(process, &QProcess::errorOccurred, process,
            [this, process, timeout, command](QProcess::ProcessError error) {
        if (!m_jobs.isEmpty()) m_jobs.head().success = false;
        m_logger->write(QStringLiteral("error [InputInjector] ydotool process error: %1")
            .arg(static_cast<int>(error)));
        if (error == QProcess::FailedToStart && !process->property("aptHandled").toBool()) {
            process->setProperty("aptHandled", true);
            timeout->stop();
            process->deleteLater();
            QTimer::singleShot(command.delayAfterMs, this, &InputInjector::runCurrentCommand);
        }
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process,
            [this, process, timeout, command](int exitCode, QProcess::ExitStatus status) {
        if (process->property("aptHandled").toBool()) return;
        process->setProperty("aptHandled", true);
        timeout->stop();
        if (status != QProcess::NormalExit || exitCode != 0) {
            if (!m_jobs.isEmpty()) m_jobs.head().success = false;
            const QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
            m_logger->write(QStringLiteral("error [InputInjector] ydotool failed: %1")
                .arg(error.isEmpty() ? QString::number(exitCode) : error));
        }
        process->deleteLater();
        QTimer::singleShot(command.delayAfterMs, this, &InputInjector::runCurrentCommand);
    });

    process->start(command.program, command.arguments, QIODevice::ReadOnly);
    timeout->start(1500);
}

} // namespace AptNative
