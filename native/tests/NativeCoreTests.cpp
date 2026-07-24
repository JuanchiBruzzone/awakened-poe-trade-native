#include "GameConfigReader.h"
#include "GameLogWatcher.h"
#include "Logger.h"
#include "Types.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace AptNative;

class NativeCoreTests final : public QObject {
    Q_OBJECT

private slots:
    void hostConfigUsesSafeDefaults();
    void hostConfigReadsRendererPayload();
    void eventEnvelopeOmitsUndefinedPayload();
    void loggerStoresAndEmitsEntries();
    void gameConfigReadsAdvancedDescriptionBinding();
    void gameConfigFallsBackForMissingBinding();
    void gameLogWatcherEmitsOnlyAppendedLines();
};

void NativeCoreTests::hostConfigUsesSafeDefaults()
{
    const HostConfig config = HostConfig::fromJson({});

    QVERIFY(config.shortcuts.isEmpty());
    QVERIFY(!config.restoreClipboard);
    QVERIFY(config.clientLog.isEmpty());
    QVERIFY(config.gameConfig.isEmpty());
    QVERIFY(!config.stashScroll);
    QVERIFY(config.overlayKey.isEmpty());
    QVERIFY(!config.logKeys);
    QCOMPARE(config.windowTitle, QStringLiteral("Path of Exile"));
    QCOMPARE(config.language, QStringLiteral("en"));
}

void NativeCoreTests::hostConfigReadsRendererPayload()
{
    const QJsonArray shortcuts{
        QJsonObject{
            {QStringLiteral("shortcut"), QStringLiteral("Ctrl + D")},
            {QStringLiteral("action"),
             QJsonObject{{QStringLiteral("type"), QStringLiteral("price-check")}}}
        }
    };
    const HostConfig config = HostConfig::fromJson({
        {QStringLiteral("shortcuts"), shortcuts},
        {QStringLiteral("restoreClipboard"), true},
        {QStringLiteral("clientLog"), QStringLiteral("/tmp/Client.txt")},
        {QStringLiteral("gameConfig"), QStringLiteral("/tmp/production_Config.ini")},
        {QStringLiteral("stashScroll"), true},
        {QStringLiteral("overlayKey"), QStringLiteral("Shift + Space")},
        {QStringLiteral("logKeys"), true},
        {QStringLiteral("windowTitle"), QStringLiteral("Path of Exile 2")},
        {QStringLiteral("language"), QStringLiteral("es")}
    });

    QCOMPARE(config.shortcuts, shortcuts);
    QVERIFY(config.restoreClipboard);
    QCOMPARE(config.clientLog, QStringLiteral("/tmp/Client.txt"));
    QCOMPARE(config.gameConfig, QStringLiteral("/tmp/production_Config.ini"));
    QVERIFY(config.stashScroll);
    QCOMPARE(config.overlayKey, QStringLiteral("Shift + Space"));
    QVERIFY(config.logKeys);
    QCOMPARE(config.windowTitle, QStringLiteral("Path of Exile 2"));
    QCOMPARE(config.language, QStringLiteral("es"));
}

void NativeCoreTests::eventEnvelopeOmitsUndefinedPayload()
{
    const QJsonObject withoutPayload =
        makeEvent(QStringLiteral("MAIN->CLIENT::config-changed"));
    QCOMPARE(withoutPayload.size(), 1);
    QCOMPARE(withoutPayload.value(QStringLiteral("name")).toString(),
             QStringLiteral("MAIN->CLIENT::config-changed"));
    QVERIFY(!withoutPayload.contains(QStringLiteral("payload")));

    const QJsonObject payload{{QStringLiteral("visible"), true}};
    const QJsonObject withPayload =
        makeEvent(QStringLiteral("MAIN->OVERLAY::visibility"), payload);
    QCOMPARE(withPayload.value(QStringLiteral("payload")).toObject(), payload);
}

void NativeCoreTests::loggerStoresAndEmitsEntries()
{
    Logger logger;
    QSignalSpy entries(&logger, &Logger::entry);

    logger.write(QStringLiteral("info [Test] startup diagnostic"));

    QCOMPARE(entries.count(), 1);
    const QString emitted = entries.takeFirst().at(0).toString();
    QVERIFY(emitted.contains(QStringLiteral("info [Test] startup diagnostic")));
    QCOMPARE(logger.history(), emitted);
}

void NativeCoreTests::gameConfigReadsAdvancedDescriptionBinding()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path =
        directory.filePath(QStringLiteral("production_Config.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(file.write(
        "\xEF\xBB\xBF[DISPLAY]\n"
        "resolution_width=1920\n"
        "[ACTION_KEYS]\n"
        "show_advanced_item_descriptions=68 2\n") > 0);
    file.close();

    Logger logger;
    GameConfigReader reader(&logger);
    reader.readConfig(path);

    QCOMPARE(reader.actualPath(), path);
    QCOMPARE(reader.showModsKey(), QStringLiteral("Ctrl + D"));
    QVERIFY(logger.history().contains(
        QStringLiteral("Advanced descriptions key: Ctrl + D")));
}

void NativeCoreTests::gameConfigFallsBackForMissingBinding()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path =
        directory.filePath(QStringLiteral("production_Config.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(file.write("[ACTION_KEYS]\nuse_bound_skill1=81\n") > 0);
    file.close();

    Logger logger;
    GameConfigReader reader(&logger);
    reader.readConfig(path);

    QCOMPARE(reader.actualPath(), path);
    QCOMPARE(reader.showModsKey(), QStringLiteral("Alt"));
    QVERIFY(logger.history().contains(
        QStringLiteral("Advanced-description binding not found")));
}

void NativeCoreTests::gameLogWatcherEmitsOnlyAppendedLines()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("Client.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(file.write("existing line\n") > 0);
    file.close();

    Logger logger;
    GameLogWatcher watcher(&logger);
    QSignalSpy lines(&watcher, &GameLogWatcher::linesRead);
    watcher.restart(path);
    QCOMPARE(watcher.actualPath(), path);

    QVERIFY(file.open(QIODevice::Append | QIODevice::Text));
    QVERIFY(file.write("new first line\n\nnew second line\n") > 0);
    file.close();
    QVERIFY(QMetaObject::invokeMethod(&watcher, "poll", Qt::DirectConnection));

    QCOMPARE(lines.count(), 1);
    const QStringList emitted =
        qvariant_cast<QStringList>(lines.takeFirst().at(0));
    QCOMPARE(emitted,
             QStringList({QStringLiteral("new first line"),
                          QStringLiteral("new second line")}));
}

QTEST_GUILESS_MAIN(NativeCoreTests)

#include "NativeCoreTests.moc"
