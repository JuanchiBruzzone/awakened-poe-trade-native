#include "NativeHost.h"

#include <LayerShellQt/Shell>

#include <QApplication>
#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QStandardPaths>

#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <utility>

using AptNative::NativeHost;
using AptNative::NativeOptions;

namespace {
bool hasArgument(int argc, char **argv, std::initializer_list<QByteArray> arguments)
{
    for (int i = 1; i < argc; ++i) {
        const QByteArray current(argv[i]);
        for (const QByteArray &argument : arguments) {
            if (current == argument) return true;
        }
    }
    return false;
}

QString findRenderer(const QString &requested)
{
    QStringList candidates;
    if (!requested.isEmpty()) candidates << requested;
    const QString compiled = QStringLiteral(APT_RENDERER_DEFAULT_PATH);
    if (!compiled.isEmpty()) candidates << compiled;
    candidates << QDir::current().filePath(QStringLiteral("renderer/dist"));
    const QDir executableDir(QCoreApplication::applicationDirPath());
    candidates << executableDir.filePath(QStringLiteral("../../renderer/dist"));
    candidates << executableDir.filePath(QStringLiteral("../share/awakened-poe-trade-native/renderer"));

    for (const QString &candidate : candidates) {
        const QString absolute = QFileInfo(candidate).absoluteFilePath();
        if (QFileInfo(QDir(absolute).filePath(QStringLiteral("index.html"))).isFile()) return absolute;
    }
    return {};
}
}

int main(int argc, char **argv)
{
    const bool informationalCommand = hasArgument(argc, argv, {
        QByteArrayLiteral("-h"), QByteArrayLiteral("--help"), QByteArrayLiteral("--help-all"),
        QByteArrayLiteral("-v"), QByteArrayLiteral("--version"),
    });
    const bool layerShellRequested =
        !informationalCommand &&
        !hasArgument(argc, argv, {QByteArrayLiteral("--no-layer-shell")}) &&
        !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
    if (informationalCommand && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }
    if (layerShellRequested) {
        // LayerShellQt must be enabled before QApplication so it owns the
        // first shell integration selected for the overlay's QWaylandWindow.
        qputenv("QT_WAYLAND_SHELL_INTEGRATION", QByteArrayLiteral("layer-shell"));
        LayerShellQt::Shell::useLayerShell();
    }

    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    QCoreApplication::setOrganizationName(QStringLiteral("SnosMe"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("snosme.github.io"));
    QCoreApplication::setApplicationName(QStringLiteral("awakened-poe-trade"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APT_NATIVE_VERSION));
    QApplication::setApplicationDisplayName(QStringLiteral("Awakened PoE Trade Native"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Native Qt/KDE Wayland host for Awakened PoE Trade"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption(QStringLiteral("renderer"),
        QStringLiteral("Path to the built renderer/dist directory."), QStringLiteral("path")));
    parser.addOption(QCommandLineOption(QStringLiteral("listen"),
        QStringLiteral("Listen on HOST:PORT. Defaults to 127.0.0.1:0."), QStringLiteral("host:port")));
    parser.addOption(QCommandLineOption(QStringLiteral("steal-shortcuts"),
        QStringLiteral("Explicitly steal conflicting KDE global shortcuts.")));
    parser.addOption(QCommandLineOption(QStringLiteral("no-layer-shell"),
        QStringLiteral("Use a regular Wayland window for debugging.")));
    parser.addOption(QCommandLineOption(QStringLiteral("browser"),
        QStringLiteral("Open the renderer in the default browser without an overlay.")));
    parser.addOption(QCommandLineOption(QStringLiteral("no-updates"),
        QStringLiteral("Check for releases but disable automatic update downloads.")));
    parser.process(app);

    if (!app.platformName().contains(QStringLiteral("wayland"),
                                     Qt::CaseInsensitive) ||
        qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        qCritical("Awakened PoE Trade Native is Wayland-only. "
                  "Start a Plasma Wayland session and relaunch the application.");
        return EXIT_FAILURE;
    }

    const QString rendererRoot = findRenderer(parser.value(QStringLiteral("renderer")));
    if (rendererRoot.isEmpty()) {
        qCritical("renderer/dist was not found; pass --renderer PATH");
        return EXIT_FAILURE;
    }

    QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDir.isEmpty()) runtimeDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(runtimeDir);
    auto lock = std::make_unique<QLockFile>(QDir(runtimeDir).filePath(QStringLiteral("awakened-poe-trade-native.lock")));
    lock->setStaleLockTime(0);
    if (!lock->tryLock(100)) {
        qCritical("another native Awakened PoE Trade instance is already running");
        return EXIT_FAILURE;
    }

    NativeOptions options;
    options.rendererRoot = rendererRoot;
    options.useLayerShell = app.platformName().contains(QStringLiteral("wayland"),
                                                        Qt::CaseInsensitive) &&
                            !parser.isSet(QStringLiteral("no-layer-shell"));
    options.browserMode = parser.isSet(QStringLiteral("browser"));
    options.stealShortcuts = parser.isSet(QStringLiteral("steal-shortcuts"));
    options.disableUpdates = parser.isSet(QStringLiteral("no-updates"));

    const QString listen = parser.value(QStringLiteral("listen"));
    if (!listen.isEmpty()) {
        const qsizetype colon = listen.lastIndexOf(QLatin1Char(':'));
        const QString host = colon >= 0 ? listen.left(colon) : listen;
        const QString portString = colon >= 0 ? listen.mid(colon + 1) : QString{};
        if (!host.isEmpty()) options.listenAddress = QHostAddress(host);
        if (!portString.isEmpty()) {
            bool ok = false;
            const uint value = portString.toUInt(&ok);
            if (!ok || value > 65535U) {
                qCritical("invalid --listen port");
                return EXIT_FAILURE;
            }
            options.listenPort = static_cast<quint16>(value);
        }
    }

    NativeHost host(std::move(options));
    if (!host.start()) return EXIT_FAILURE;
    return app.exec();
}
