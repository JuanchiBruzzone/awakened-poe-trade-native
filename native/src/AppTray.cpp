#include "AppTray.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QIcon>
#include <QMessageBox>
#include <QMenu>
#include <QStyle>
#include <QSystemTrayIcon>

namespace AptNative {

AppTray::AppTray(QObject *parent) : QObject(parent), m_tray(new QSystemTrayIcon(this))
{
    auto *menu = new QMenu;
    auto *settings = menu->addAction(QStringLiteral("Settings/League"));
    auto *browser = menu->addAction(QStringLiteral("Open in Browser"));
    menu->addSeparator();
    auto *configFolder = menu->addAction(QStringLiteral("Open config folder"));
    auto *quit = menu->addAction(QStringLiteral("Quit"));

    connect(settings, &QAction::triggered, this, [this] {
        QMessageBox::information(nullptr,
            QStringLiteral("Settings"),
            QStringLiteral("Open Path of Exile and press \"%1\". "
                           "Click on the button with cog icon there.")
                .arg(m_overlayKey));
    });
    connect(browser, &QAction::triggered, this, [this] {
        if (m_appUrl.isValid()) QDesktopServices::openUrl(m_appUrl);
    });
    connect(configFolder, &QAction::triggered, this, [this] {
        if (!m_dataDirectory.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_dataDirectory));
        }
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
