#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

namespace AptNative {

class Logger;

class InputEventMonitor final : public QObject {
    Q_OBJECT
public:
    explicit InputEventMonitor(Logger *logger, QObject *parent = nullptr);
    ~InputEventMonitor() override;

    void setEnabled(bool enabled);
    bool isAvailable() const;

signals:
    void wheelRotated(int rotation);

private slots:
    void rescanDevices();

private:
    struct Device;

    void closeDevices();
    void readDevice(Device *device);
    bool ctrlPressed() const;

    Logger *m_logger;
    QTimer m_rescanTimer;
    QHash<int, Device *> m_devices;
    QSet<QString> m_seenDevicePaths;
    bool m_enabled = false;
    bool m_available = false;
    bool m_reportedUnavailable = false;
    bool m_reportedAvailable = false;
};

} // namespace AptNative
