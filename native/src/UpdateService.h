#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

namespace AptNative {

class Logger;

class UpdateService final : public QObject {
    Q_OBJECT
public:
    UpdateService(QString repository,
                  QString currentVersion,
                  QString dataDirectory,
                  bool disabledByFlag,
                  Logger *logger,
                  QObject *parent = nullptr);

    QJsonObject state() const;
    void check();
    void installAndRestart();

signals:
    void stateChanged(const QJsonObject &state);

private:
    void setState(QJsonObject state);
    void processRelease(const QByteArray &document);
    void download(const QUrl &url,
                  const QString &assetName,
                  const QByteArray &expectedSha256,
                  const QString &version);
    bool canReplaceAppImage() const;
    static QString normalizedVersion(const QString &version);

    QString m_repository;
    QString m_currentVersion;
    QString m_dataDirectory;
    QString m_appImagePath;
    QString m_pendingPath;
    QString m_pendingVersion;
    bool m_disabledByFlag;
    bool m_requestPending = false;
    Logger *m_logger;
    QNetworkAccessManager m_network;
    QJsonObject m_state{{QStringLiteral("state"), QStringLiteral("initial")}};
};

} // namespace AptNative
