#include "DesktopLaunch.h"

#include <QProcess>

namespace AptNative::DesktopLaunch {

QProcessEnvironment cleanEnvironment(const QString &activationToken)
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    // AppImage and the layer-shell host may override Qt's loader paths. Those
    // settings belong to this process only and can make a system browser or
    // file manager load incompatible bundled plugins.
    const QStringList privateVariables{
        QStringLiteral("APPDIR"),
        QStringLiteral("APPIMAGE"),
        QStringLiteral("APPIMAGE_ORIGINAL_LD_LIBRARY_PATH"),
        QStringLiteral("ARGV0"),
        QStringLiteral("OWD"),
        QStringLiteral("QT_PLUGIN_PATH"),
        QStringLiteral("QT_QPA_PLATFORM_PLUGIN_PATH"),
        QStringLiteral("QT_WAYLAND_SHELL_INTEGRATION"),
        QStringLiteral("QML2_IMPORT_PATH"),
        QStringLiteral("QML_IMPORT_PATH"),
        QStringLiteral("QTWEBENGINEPROCESS_PATH"),
        QStringLiteral("XDG_ACTIVATION_TOKEN"),
    };
    for (const QString &variable : privateVariables) {
        environment.remove(variable);
    }
    if (!activationToken.isEmpty()) {
        environment.insert(
            QStringLiteral("XDG_ACTIVATION_TOKEN"), activationToken);
    }
    return environment;
}

bool startDetached(const QString &program, const QStringList &arguments,
                   const QString &activationToken)
{
    QProcess process;
    process.setProcessEnvironment(cleanEnvironment(activationToken));
    process.setProgram(program);
    process.setArguments(arguments);
    return process.startDetached();
}

} // namespace AptNative::DesktopLaunch
