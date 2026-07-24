#pragma once

#include <QImage>
#include <QObject>

#include <functional>

namespace AptNative {

class Logger;

class ScreenshotService final : public QObject {
    Q_OBJECT
public:
    explicit ScreenshotService(Logger *logger, QObject *parent = nullptr);

    void captureActiveWindow(std::function<void(QImage)> onSuccess,
                             std::function<void(QString)> onFailure);

private:
    Logger *m_logger;
    bool m_capturePending = false;
};

} // namespace AptNative
