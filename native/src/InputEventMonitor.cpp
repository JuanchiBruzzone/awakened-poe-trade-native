#include "InputEventMonitor.h"

#include "Logger.h"

#include <QDir>
#include <QSocketNotifier>

#include <algorithm>
#include <array>
#include <cerrno>
#include <utility>

#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace AptNative {

namespace {
constexpr qsizetype BitsPerWord = qsizetype(sizeof(unsigned long) * 8);

template <std::size_t N>
bool bitSet(const std::array<unsigned long, N> &bits, int bit)
{
    const qsizetype word = bit / BitsPerWord;
    const qsizetype offset = bit % BitsPerWord;
    return word >= 0 && word < qsizetype(N) &&
           (bits.at(std::size_t(word)) & (1UL << offset)) != 0;
}
}

struct InputEventMonitor::Device {
    int fd = -1;
    QSocketNotifier *notifier = nullptr;
    bool keyboard = false;
    bool standardWheel = false;
    bool highResolutionWheel = false;
    bool leftCtrl = false;
    bool rightCtrl = false;
    int highResolutionRemainder = 0;
};

InputEventMonitor::InputEventMonitor(Logger *logger, QObject *parent)
    : QObject(parent), m_logger(logger)
{
    m_rescanTimer.setInterval(2000);
    connect(&m_rescanTimer, &QTimer::timeout,
            this, &InputEventMonitor::rescanDevices);
}

InputEventMonitor::~InputEventMonitor()
{
    closeDevices();
}

void InputEventMonitor::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    if (enabled) {
        rescanDevices();
        m_rescanTimer.start();
    } else {
        m_rescanTimer.stop();
        closeDevices();
        m_available = false;
        m_reportedUnavailable = false;
        m_reportedAvailable = false;
        m_seenDevicePaths.clear();
    }
}

bool InputEventMonitor::isAvailable() const
{
    return m_available;
}

void InputEventMonitor::rescanDevices()
{
    if (!m_enabled) return;

    bool permissionDenied = false;
    const QDir inputDirectory(QStringLiteral("/dev/input"));
    const QStringList entries = inputDirectory.entryList(
        {QStringLiteral("event*")}, QDir::System | QDir::Files, QDir::Name);
    QSet<QString> devicePaths;
    for (const QString &entry : entries) {
        devicePaths.insert(inputDirectory.absoluteFilePath(entry));
    }
    if (m_available && devicePaths == m_seenDevicePaths) return;

    closeDevices();
    m_seenDevicePaths = devicePaths;
    for (const QString &entry : entries) {
        const QByteArray path = inputDirectory.absoluteFilePath(entry).toLocal8Bit();
        const int fd = ::open(path.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            if (errno == EACCES || errno == EPERM) permissionDenied = true;
            continue;
        }

        std::array<unsigned long, (EV_MAX / BitsPerWord) + 1> eventBits{};
        std::array<unsigned long, (KEY_MAX / BitsPerWord) + 1> keyBits{};
        std::array<unsigned long, (REL_MAX / BitsPerWord) + 1> relativeBits{};
        if (::ioctl(fd, EVIOCGBIT(0, sizeof(eventBits)), eventBits.data()) < 0) {
            ::close(fd);
            continue;
        }
        if (bitSet(eventBits, EV_KEY)) {
            ::ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits.data());
        }
        if (bitSet(eventBits, EV_REL)) {
            ::ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relativeBits)), relativeBits.data());
        }

        const bool keyboard = bitSet(keyBits, KEY_LEFTCTRL) ||
                              bitSet(keyBits, KEY_RIGHTCTRL);
        const bool standardWheel = bitSet(relativeBits, REL_WHEEL);
        const bool highResolutionWheel = bitSet(relativeBits, REL_WHEEL_HI_RES);
        if (!keyboard && !standardWheel && !highResolutionWheel) {
            ::close(fd);
            continue;
        }

        auto *device = new Device;
        device->fd = fd;
        device->keyboard = keyboard;
        device->standardWheel = standardWheel;
        device->highResolutionWheel = highResolutionWheel;
        device->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        connect(device->notifier, &QSocketNotifier::activated,
                this, [this, device] { readDevice(device); });
        m_devices.insert(fd, device);
    }

    const bool hasWheel = std::any_of(
        m_devices.cbegin(), m_devices.cend(), [](const Device *device) {
            return device->standardWheel || device->highResolutionWheel;
        });
    const bool hasKeyboard = std::any_of(
        m_devices.cbegin(), m_devices.cend(), [](const Device *device) {
            return device->keyboard;
        });
    m_available = hasWheel && hasKeyboard;
    if (!m_available && !m_reportedUnavailable) {
        const QString reason = permissionDenied
            ? QStringLiteral("permission was denied for /dev/input/event*")
            : QStringLiteral("no wheel-capable input device was found");
        m_logger->write(QStringLiteral(
            "error [InputMonitor] Ctrl+wheel stash navigation is unavailable: %1.")
            .arg(reason));
        m_reportedUnavailable = true;
        m_reportedAvailable = false;
    } else if (m_available && !m_reportedAvailable) {
        m_reportedUnavailable = false;
        m_reportedAvailable = true;
        m_logger->write(QStringLiteral(
            "info [InputMonitor] Ctrl+wheel stash navigation is active."));
    }
}

void InputEventMonitor::closeDevices()
{
    for (Device *device : std::as_const(m_devices)) {
        delete device->notifier;
        if (device->fd >= 0) ::close(device->fd);
        delete device;
    }
    m_devices.clear();
}

bool InputEventMonitor::ctrlPressed() const
{
    return std::any_of(
        m_devices.cbegin(), m_devices.cend(), [](const Device *device) {
            return device->leftCtrl || device->rightCtrl;
        });
}

void InputEventMonitor::readDevice(Device *device)
{
    input_event events[32];
    for (;;) {
        const ssize_t bytes = ::read(device->fd, events, sizeof(events));
        if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
        if (bytes <= 0) return;

        const qsizetype count = qsizetype(bytes / ssize_t(sizeof(input_event)));
        for (qsizetype i = 0; i < count; ++i) {
            const input_event &event = events[i];
            if (event.type == EV_KEY && event.code == KEY_LEFTCTRL) {
                device->leftCtrl = event.value != 0;
            } else if (event.type == EV_KEY && event.code == KEY_RIGHTCTRL) {
                device->rightCtrl = event.value != 0;
            } else if (event.type == EV_REL && event.code == REL_WHEEL &&
                       device->standardWheel && ctrlPressed()) {
                emit wheelRotated(event.value);
            } else if (event.type == EV_REL && event.code == REL_WHEEL_HI_RES &&
                       !device->standardWheel && ctrlPressed()) {
                device->highResolutionRemainder += event.value;
                const int steps = device->highResolutionRemainder / 120;
                device->highResolutionRemainder %= 120;
                if (steps != 0) emit wheelRotated(steps);
            }
        }
    }
}

} // namespace AptNative
