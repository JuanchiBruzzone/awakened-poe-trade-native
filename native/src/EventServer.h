#pragma once

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTcpServer>
#include <QUrl>

#include <optional>

class QTcpSocket;

namespace AptNative {

class ConfigStore;
class Logger;

class EventServer final : public QObject {
    Q_OBJECT
public:
    explicit EventServer(QString rendererRoot,
                         ConfigStore *configStore,
                         Logger *logger,
                         QObject *parent = nullptr);
    ~EventServer() override;

    bool listen(const QHostAddress &address = QHostAddress::LocalHost, quint16 port = 0);
    quint16 port() const;
    QUrl appUrl() const;
    void setUpdaterState(const QJsonObject &state);

    void broadcast(const QJsonObject &event);
    void sendToLastActive(const QJsonObject &event);

signals:
    void eventReceived(const QString &name, const QJsonValue &payload);
    void clientConnected();

private:
    struct ClientState {
        QByteArray buffer;
        bool websocket = false;
    };
    struct HttpRequest {
        QByteArray method;
        QByteArray target;
        QMap<QByteArray, QByteArray> headers;
        QByteArray body;
    };

    void acceptConnections();
    void readClient(QTcpSocket *socket);
    void removeClient(QTcpSocket *socket);
    std::optional<HttpRequest> takeHttpRequest(ClientState &state, QTcpSocket *socket);
    void handleHttp(QTcpSocket *socket, const HttpRequest &request);
    void handleStatic(QTcpSocket *socket, const HttpRequest &request);
    void handleConfig(QTcpSocket *socket);
    void handleUpload(QTcpSocket *socket, const HttpRequest &request, const QString &path);
    void handleProxy(QTcpSocket *socket, const HttpRequest &request);
    void upgradeWebSocket(QTcpSocket *socket, const HttpRequest &request);
    void consumeWebSocketFrames(QTcpSocket *socket, ClientState &state);
    void processWebSocketMessage(QTcpSocket *socket, const QByteArray &payload);

    static QByteArray websocketFrame(const QByteArray &payload, quint8 opcode = 0x1);
    static QByteArray mimeType(const QString &path);
    static QByteArray reasonPhrase(int status);
    static bool pathInside(const QString &root, const QString &candidate);

    void respond(QTcpSocket *socket,
                 int status,
                 const QByteArray &body = {},
                 const QByteArray &contentType = QByteArrayLiteral("text/plain; charset=utf-8"),
                 const QList<QPair<QByteArray, QByteArray>> &headers = {});
    void sendFrame(QTcpSocket *socket, const QByteArray &payload, quint8 opcode = 0x1);

    QString m_rendererRoot;
    ConfigStore *m_configStore;
    Logger *m_logger;
    QTcpServer m_tcp;
    QHash<QTcpSocket *, ClientState> m_clients;
    QPointer<QTcpSocket> m_lastActive;
    QNetworkAccessManager m_network;
    QJsonObject m_updaterState{
        {QStringLiteral("state"), QStringLiteral("initial")}
    };
};

} // namespace AptNative
