#include "ScreenshotService.h"

#include "Logger.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QFutureWatcher>
#include <QVariantMap>
#include <QtConcurrent>

#include <cerrno>
#include <cstring>
#include <limits>
#include <unistd.h>

namespace AptNative {
namespace {

struct CaptureResult {
    QImage image;
    QString error;
};

CaptureResult captureWithKWin()
{
    int pipeFds[2]{-1, -1};
    if (::pipe(pipeFds) != 0) {
        return {{}, QStringLiteral("Could not create screenshot pipe: %1")
            .arg(QString::fromLocal8Bit(std::strerror(errno)))};
    }

    QDBusUnixFileDescriptor outputFd;
    outputFd.giveFileDescriptor(pipeFds[1]);
    pipeFds[1] = -1;

    QDBusInterface screenshot(
        QStringLiteral("org.kde.KWin.ScreenShot2"),
        QStringLiteral("/org/kde/KWin/ScreenShot2"),
        QStringLiteral("org.kde.KWin.ScreenShot2"),
        QDBusConnection::sessionBus());
    if (!screenshot.isValid()) {
        ::close(pipeFds[0]);
        return {{}, QStringLiteral(
            "KWin ScreenShot2 is unavailable. This feature requires KDE Plasma Wayland.")};
    }

    const QVariantMap options{
        {QStringLiteral("include-cursor"), false},
        {QStringLiteral("include-decoration"), false},
        {QStringLiteral("include-shadow"), false},
        {QStringLiteral("native-resolution"), true},
        {QStringLiteral("hide-caller-windows"), true}
    };
    const QDBusReply<QVariantMap> reply = screenshot.call(
        QStringLiteral("CaptureActiveWindow"),
        options,
        QVariant::fromValue(outputFd));
    outputFd = QDBusUnixFileDescriptor();

    if (!reply.isValid()) {
        ::close(pipeFds[0]);
        return {{}, QStringLiteral("KWin rejected the active-window screenshot: %1")
            .arg(reply.error().message())};
    }

    const QVariantMap metadata = reply.value();
    const int width = metadata.value(QStringLiteral("width")).toInt();
    const int height = metadata.value(QStringLiteral("height")).toInt();
    const int stride = metadata.value(QStringLiteral("stride")).toInt();
    const int formatValue = metadata.value(QStringLiteral("format")).toInt();
    if (width <= 0 || height <= 0 || stride <= 0 ||
        height > std::numeric_limits<int>::max() / stride) {
        ::close(pipeFds[0]);
        return {{}, QStringLiteral("KWin returned invalid screenshot dimensions.")};
    }

    const qsizetype expected = static_cast<qsizetype>(height) * stride;
    QByteArray pixels;
    pixels.resize(expected);
    qsizetype received = 0;
    while (received < expected) {
        const ssize_t count = ::read(
            pipeFds[0],
            pixels.data() + received,
            static_cast<size_t>(expected - received));
        if (count > 0) {
            received += static_cast<qsizetype>(count);
        } else if (count == 0) {
            break;
        } else if (errno != EINTR) {
            const QString error = QString::fromLocal8Bit(std::strerror(errno));
            ::close(pipeFds[0]);
            return {{}, QStringLiteral("Could not read the KWin screenshot: %1").arg(error)};
        }
    }
    ::close(pipeFds[0]);
    if (received != expected) {
        return {{}, QStringLiteral("KWin returned an incomplete screenshot.")};
    }

    const auto format = static_cast<QImage::Format>(formatValue);
    const QImage borrowed(
        reinterpret_cast<const uchar *>(pixels.constData()),
        width,
        height,
        stride,
        format);
    if (borrowed.isNull()) {
        return {{}, QStringLiteral("KWin returned an unsupported screenshot format.")};
    }
    return {borrowed.copy(), {}};
}

} // namespace

ScreenshotService::ScreenshotService(Logger *logger, QObject *parent)
    : QObject(parent), m_logger(logger)
{
}

void ScreenshotService::captureActiveWindow(
    std::function<void(QImage)> onSuccess,
    std::function<void(QString)> onFailure)
{
    if (m_capturePending) {
        onFailure(QStringLiteral("A screenshot is already in progress."));
        return;
    }
    m_capturePending = true;

    auto *watcher = new QFutureWatcher<CaptureResult>(this);
    connect(watcher, &QFutureWatcher<CaptureResult>::finished, this,
            [this, watcher, onSuccess = std::move(onSuccess),
             onFailure = std::move(onFailure)]() mutable {
        const CaptureResult result = watcher->result();
        watcher->deleteLater();
        m_capturePending = false;
        if (!result.error.isEmpty()) {
            m_logger->write(QStringLiteral("error [Screenshot] %1").arg(result.error));
            onFailure(result.error);
        } else {
            onSuccess(result.image);
        }
    });
    watcher->setFuture(QtConcurrent::run(captureWithKWin));
}

} // namespace AptNative
