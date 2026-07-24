#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QSystemTrayIcon;

namespace AptNative {

class AppTray final : public QObject {
    Q_OBJECT
public:
    explicit AppTray(QObject *parent = nullptr);
    void setAppUrl(const QUrl &url);
    void setOverlayKey(const QString &overlayKey);
    void setDataDirectory(const QString &dataDirectory);

signals:
    void toggleOverlay();
    void quitRequested();

private:
    QSystemTrayIcon *m_tray;
    QUrl m_appUrl;
    QString m_overlayKey = QStringLiteral("Shift + Space");
    QString m_dataDirectory;
};

} // namespace AptNative
