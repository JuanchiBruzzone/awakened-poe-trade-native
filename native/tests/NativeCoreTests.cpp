#include "ClipboardService.h"
#include "DesktopLaunch.h"
#include "EventServer.h"
#include "GameConfigReader.h"
#include "GameLogWatcher.h"
#include "GameWindowTracker.h"
#include "InputInjector.h"
#include "Logger.h"
#include "Types.h"
#include "UpdateService.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTcpSocket>

#include <memory>

using namespace AptNative;

class NativeCoreTests final : public QObject {
    Q_OBJECT

private slots:
    void hostConfigUsesSafeDefaults();
    void hostConfigReadsRendererPayload();
    void eventEnvelopeOmitsUndefinedPayload();
    void itemCopyChordIncludesAdvancedDescriptions();
    void loggerStoresAndEmitsEntries();
    void gameConfigReadsAdvancedDescriptionBinding();
    void gameConfigFallsBackForMissingBinding();
    void gameLogWatcherEmitsOnlyAppendedLines();
    void recognizesLocalizedPoeItems();
    void rejectsNonItemClipboardText();
    void acceptsPoeWindowRegardlessOfProtocolLabel();
    void identifiesPoeApplicationClasses();
    void normalizesNativeReleaseVersions();
    void normalizesInputShortcuts();
    void mapsInputKeysToLinuxCodes();
    void itemCopyInjectionHoldsCustomAdvancedKey();
    void eventServerShutsDownWithConnectedClient();
    void desktopLaunchRemovesPrivateAppImageEnvironment();
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

void NativeCoreTests::itemCopyChordIncludesAdvancedDescriptions()
{
    QCOMPARE(advancedItemCopyChord(QStringLiteral("Alt")),
             QStringLiteral("Ctrl + Alt + C"));
    QCOMPARE(advancedItemCopyChord(QStringLiteral("Ctrl + D")),
             QStringLiteral("Ctrl + D + C"));
    QCOMPARE(advancedItemCopyChord(QString{}), QStringLiteral("Ctrl + C"));
}

void NativeCoreTests::itemCopyInjectionHoldsCustomAdvancedKey()
{
    QCOMPARE(InputInjector::shortcutTokens(
                 advancedItemCopyChord(QStringLiteral("Ctrl + D"))),
             QStringList({QStringLiteral("Ctrl"), QStringLiteral("D"),
                          QStringLiteral("C")}));
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

void NativeCoreTests::recognizesLocalizedPoeItems()
{
    const QStringList itemHeaders{
        QStringLiteral("Item Class: Maps"),
        QStringLiteral("Класс предмета: Карты"),
        QStringLiteral("Classe d'objet: Cartes"),
        QStringLiteral("Gegenstandsklasse: Karten"),
        QStringLiteral("Clase de objeto: Mapas"),
        QStringLiteral("아이템 종류: 지도"),
        QStringLiteral("物品类别: 地图"),
    };
    for (const QString &header : itemHeaders) {
        QVERIFY2(ClipboardService::isPoeItem(header),
                 qPrintable(QStringLiteral("Not recognized: %1").arg(header)));
    }
}

void NativeCoreTests::rejectsNonItemClipboardText()
{
    QVERIFY(!ClipboardService::isPoeItem(QString{}));
    QVERIFY(!ClipboardService::isPoeItem(
        QStringLiteral("whispered trade message")));
    QVERIFY(!ClipboardService::isPoeItem(
        QStringLiteral("__APT_FORCE_EMPTY_123")));
}

void NativeCoreTests::acceptsPoeWindowRegardlessOfProtocolLabel()
{
    const QString title = QStringLiteral("Path of Exile");
    const QString gameClass = QStringLiteral("steam_app_238960");

    QVERIFY(GameWindowTracker::matchesGameWindow({}, gameClass, title));
    QVERIFY(GameWindowTracker::isSupportedGameWindow(
        {}, gameClass, title, true));
    QVERIFY(GameWindowTracker::isSupportedGameWindow(
        {}, gameClass, title, false));
    QVERIFY(!GameWindowTracker::isSupportedGameWindow(
        QStringLiteral("Firefox"), QStringLiteral("firefox"), title, true));
    QVERIFY(!GameWindowTracker::matchesGameWindow(
        QStringLiteral("Firefox"), QStringLiteral("firefox"), QString{}));
}

void NativeCoreTests::identifiesPoeApplicationClasses()
{
    QCOMPARE(GameWindowTracker::gameNameForClass(
                 QStringLiteral("steam_app_238960")),
             QStringLiteral("Path of Exile 1"));
    QCOMPARE(GameWindowTracker::gameNameForClass(
                 QStringLiteral("STEAM_APP_2694490")),
             QStringLiteral("Path of Exile 2"));
    QCOMPARE(GameWindowTracker::gameNameForClass(
                 QStringLiteral("pathofexile_x64steam.exe")),
             QStringLiteral("Path of Exile"));
}

void NativeCoreTests::normalizesNativeReleaseVersions()
{
    QCOMPARE(UpdateService::normalizedVersion(
                 QStringLiteral("v3.28.104-native.2")),
             QStringLiteral("3.28.104.2"));
    QCOMPARE(UpdateService::normalizedVersion(
                 QStringLiteral("release-3.28.104-native.12-beta")),
             QStringLiteral("3.28.104.12"));
    QCOMPARE(UpdateService::normalizedVersion(QStringLiteral("  4.0.0  ")),
             QStringLiteral("4.0.0"));
}

void NativeCoreTests::normalizesInputShortcuts()
{
    QCOMPARE(InputInjector::shortcutTokens(
                 QStringLiteral(" Ctrl + ArrowRight ")),
             QStringList({QStringLiteral("Ctrl"), QStringLiteral("Right")}));
    QCOMPARE(InputInjector::shortcutTokens(QStringLiteral("Shift+Space")),
             QStringList({QStringLiteral("Shift"), QStringLiteral("Space")}));
}

void NativeCoreTests::mapsInputKeysToLinuxCodes()
{
    QCOMPARE(InputInjector::linuxKeyCode(QStringLiteral("Ctrl")), 29);
    QCOMPARE(InputInjector::linuxKeyCode(QStringLiteral("D")), 32);
    QCOMPARE(InputInjector::linuxKeyCode(QStringLiteral("F5")), 63);
    QCOMPARE(InputInjector::linuxKeyCode(QStringLiteral("Unknown")), -1);
}

void NativeCoreTests::eventServerShutsDownWithConnectedClient()
{
    Logger logger;
    auto server = std::make_unique<EventServer>(
        QString{}, nullptr, &logger);
    QVERIFY(server->listen());

    QTcpSocket client;
    QSignalSpy connected(server.get(), &EventServer::clientConnected);
    client.connectToHost(QHostAddress::LocalHost, server->port());
    QVERIFY(client.waitForConnected());
    client.write(
        "GET /events HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Connection: Upgrade\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n");
    QVERIFY(client.waitForBytesWritten());
    QTRY_COMPARE_WITH_TIMEOUT(connected.count(), 1, 1000);

    // Regression: QTcpServer used to destroy this connected socket after the
    // EventServer client map, invoking removeClient() on already-freed state.
    server.reset();
    QVERIFY(client.state() == QAbstractSocket::UnconnectedState ||
            client.waitForDisconnected(1000));
}

void NativeCoreTests::desktopLaunchRemovesPrivateAppImageEnvironment()
{
    qputenv("APPDIR", QByteArrayLiteral("/tmp/fake-appdir"));
    qputenv("QT_PLUGIN_PATH", QByteArrayLiteral("/tmp/fake-plugins"));
    qputenv("QT_WAYLAND_SHELL_INTEGRATION", QByteArrayLiteral("layer-shell"));
    qputenv("XDG_ACTIVATION_TOKEN", QByteArrayLiteral("stale-token"));

    const QProcessEnvironment environment =
        DesktopLaunch::cleanEnvironment(QStringLiteral("fresh-token"));

    QVERIFY(!environment.contains(QStringLiteral("APPDIR")));
    QVERIFY(!environment.contains(QStringLiteral("QT_PLUGIN_PATH")));
    QVERIFY(!environment.contains(
        QStringLiteral("QT_WAYLAND_SHELL_INTEGRATION")));
    QCOMPARE(environment.value(QStringLiteral("XDG_ACTIVATION_TOKEN")),
             QStringLiteral("fresh-token"));

    qunsetenv("APPDIR");
    qunsetenv("QT_PLUGIN_PATH");
    qunsetenv("QT_WAYLAND_SHELL_INTEGRATION");
    qunsetenv("XDG_ACTIVATION_TOKEN");
}

QTEST_GUILESS_MAIN(NativeCoreTests)

#include "NativeCoreTests.moc"
