#pragma once

#include <QObject>
#include <QString>

namespace AptNative {

class Logger final : public QObject {
    Q_OBJECT
public:
    explicit Logger(QObject *parent = nullptr);

    QString history() const;
    void write(const QString &message);

signals:
    void entry(const QString &message);

private:
    QString m_history;
};

} // namespace AptNative
