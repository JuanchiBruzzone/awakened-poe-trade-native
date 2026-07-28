#pragma once

#include <QElapsedTimer>
#include <QNetworkCookie>
#include <QPoint>
#include <QRect>
#include <QTimer>
#include <QUrl>
#include <QWidget>

class QWebEngineView;
namespace LayerShellQt { class Window; }

namespace AptNative {

class Logger;

class OverlayWindow final : public QWidget {
    Q_OBJECT
public:
    explicit OverlayWindow(QString profilePath,
                           bool useLayerShell,
                           Logger *logger,
                           QWidget *parent = nullptr);

    void load(const QUrl &url);
    QByteArray browserUserAgent() const;
    void setInteractive(bool interactive);
    bool isInteractive() const;
    void toggleInteractive();
    void beginTrackArea(const QPoint &from,
                        const QRect &area,
                        int closeThreshold,
                        const QString &holdKey = {});
    void updateCursorPosition(const QPoint &position);
    void focusGame();

signals:
    void pageLoaded();
    void interactionChanged(bool interactive);
    void hideExclusiveWidgetRequested();
    void focusGameRequested();
    void visibilityRequested(bool visible);
    void proxyCookieAvailable(const QNetworkCookie &cookie);

private slots:
    void checkTrackedArea();
    void checkUiVisibility();
    void syncEmbeddedBrowser();

private:
    void configureLayerShell();
    void openExternalUrl(const QUrl &url);
    void setUiVisible(bool visible);

    QWebEngineView *m_view;
    QWebEngineView *m_browserView;
    LayerShellQt::Window *m_layerWindow = nullptr;
    Logger *m_logger;
    bool m_useLayerShell;
    bool m_interactive = true;
    QPoint m_trackFrom;
    QRect m_trackArea;
    int m_closeThreshold = 0;
    QString m_trackHoldKey;
    QPoint m_cursorPosition;
    bool m_hasCursorPosition = false;
    QTimer m_trackTimer;
    QElapsedTimer m_trackGraceTimer;
    bool m_trackPositionSettled = false;
    QTimer m_visibilityTimer;
    QTimer m_hideUiTimer;
    QTimer m_browserSyncTimer;
    bool m_uiVisible = true;
    QUrl m_browserUrl;
};

} // namespace AptNative
