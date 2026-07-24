#include "AppTray.h"

#include "DesktopLaunch.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QIcon>
#include <QMenu>
#include <QStandardPaths>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>

namespace AptNative {

namespace {
bool startDetachedWithoutLayerShell(const QString &program,
                                    const QStringList &arguments)
{
    return DesktopLaunch::startDetached(program, arguments);
}

bool openWithDesktopHandler(const QUrl &url)
{
    const QString opener = QStandardPaths::findExecutable(
        QStringLiteral("xdg-open"));
    if (opener.isEmpty()) return QDesktopServices::openUrl(url);
    return startDetachedWithoutLayerShell(
        opener, {url.toString(QUrl::FullyEncoded)});
}

bool openConfigDirectory(const QString &directory)
{
    const QString dolphin = QStandardPaths::findExecutable(
        QStringLiteral("dolphin"));
    if (!dolphin.isEmpty()) {
        return startDetachedWithoutLayerShell(
            dolphin, {QStringLiteral("--new-window"), directory});
    }
    return openWithDesktopHandler(QUrl::fromLocalFile(directory));
}
}

AppTray::AppTray(QObject *parent) : QObject(parent), m_tray(new QSystemTrayIcon(this))
{
    auto *menu = new QMenu;
    auto *settings = menu->addAction(QStringLiteral("Settings/League"));
    auto *browser = menu->addAction(QStringLiteral("Open in Browser"));
    menu->addSeparator();
    auto *configFolder = menu->addAction(QStringLiteral("Open config folder"));
    auto *quit = menu->addAction(QStringLiteral("Quit"));

    connect(settings, &QAction::triggered, this, [this, menu] {
        menu->close();
        m_tray->showMessage(
            QStringLiteral("Settings"),
            QStringLiteral("Open Path of Exile and press \"%1\". "
                           "Click on the button with cog icon there.")
                .arg(m_overlayKey),
            QSystemTrayIcon::Information,
            8000);
    });
    connect(browser, &QAction::triggered, this, [this, menu] {
        menu->close();
        const QUrl url = m_appUrl;
        QTimer::singleShot(0, this, [url] {
            if (url.isValid()) openWithDesktopHandler(url);
        });
    });
    connect(configFolder, &QAction::triggered, this, [this, menu] {
        menu->close();
        const QString dataDirectory = m_dataDirectory;
        QTimer::singleShot(0, this, [dataDirectory] {
            if (!dataDirectory.isEmpty()) {
                openConfigDirectory(dataDirectory);
            }
        });
    });
    connect(quit, &QAction::triggered, this, &AppTray::quitRequested);

    m_tray->setContextMenu(menu);
    m_tray->setToolTip(QStringLiteral("Awakened PoE Trade v%1")
        .arg(QStringLiteral(APT_NATIVE_VERSION)));
#ifdef APT_HAS_APP_ICON
    m_tray->setIcon(QIcon(QStringLiteral(":/icons/128x128.png")));
#else
    m_tray->setIcon(QIcon::fromTheme(QStringLiteral("awakened-poe-trade"),
        QApplication::style()->standardIcon(QStyle::SP_ComputerIcon)));
#endif
    m_tray->show();
}

void AppTray::setAppUrl(const QUrl &url)
{
    m_appUrl = url;
}

void AppTray::setOverlayKey(const QString &overlayKey)
{
    if (!overlayKey.trimmed().isEmpty()) m_overlayKey = overlayKey;
}

void AppTray::setDataDirectory(const QString &dataDirectory)
{
    m_dataDirectory = dataDirectory;
}

} // namespace AptNative
