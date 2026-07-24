#pragma once

#include <QImage>
#include <QObject>
#include <QStringList>

#include <functional>
#include <memory>

namespace AptNative {

class Logger;

struct HeistOcrResult {
    qint64 elapsedMs = 0;
    QStringList paragraphs;
    QString error;
};

class HeistOcrService final : public QObject {
    Q_OBJECT
public:
    HeistOcrService(QString dataDirectory, Logger *logger, QObject *parent = nullptr);
    ~HeistOcrService() override;

    void setLanguage(const QString &language);
    void recognize(const QImage &screenshot,
                   std::function<void(HeistOcrResult)> onFinished);

private:
    struct State;
    std::unique_ptr<State> m_state;
    Logger *m_logger;
    bool m_pending = false;
};

} // namespace AptNative
