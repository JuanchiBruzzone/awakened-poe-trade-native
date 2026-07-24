#include "EventServer.h"

#include "ConfigStore.h"
#include "Logger.h"
#include "Types.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

#include <limits>
#include <utility>

namespace AptNative {

namespace {
constexpr qsizetype MaxRequestBytes = 16 * 1024 * 1024;
constexpr qsizetype MaxWebSocketPayload = 8 * 1024 * 1024;
constexpr qsizetype MaxProxyResponse = 64 * 1024 * 1024;
const QByteArray WebSocketGuid = QByteArrayLiteral("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

const QSet<QString> AllowedProxyHosts{
    QStringLiteral("www.pathofexile.com"),
    QStringLiteral("ru.pathofexile.com"),
    QStringLiteral("pathofexile.tw"),
    QStringLiteral("poe.game.daum.net"),
    QStringLiteral("poe.ninja"),
    QStringLiteral("www.poeprices.info")
};
}

EventServer::EventServer(QString rendererRoot,
                         ConfigStore *configStore,
                         Logger *logger,
                         QObject *parent)
    : QObject(parent),
      m_rendererRoot(QDir(std::move(rendererRoot)).absolutePath()),
      m_configStore(configStore),
      m_logger(logger)
{
    m_network.setCookieJar(new QNetworkCookieJar(&m_network));
    connect(&m_tcp, &QTcpServer::newConnection, this, &EventServer::acceptConnections);
    connect(m_logger, &Logger::entry, this, [this](const QString &message) {
        broadcast(makeEvent(QStringLiteral("MAIN->CLIENT::log-entry"),
                            QJsonObject{{QStringLiteral("message"), message}}));
    });
}

bool EventServer::listen(const QHostAddress &address, quint16 requestedPort)
{
    if (!m_tcp.listen(address, requestedPort)) {
        m_logger->write(QStringLiteral("error [Server] %1").arg(m_tcp.errorString()));
        return false;
    }
    m_logger->write(QStringLiteral("info [Server] Listening on %1:%2")
        .arg(m_tcp.serverAddress().toString()).arg(m_tcp.serverPort()));
    return true;
}

quint16 EventServer::port() const
{
    return m_tcp.serverPort();
}

QUrl EventServer::appUrl() const
{
    return QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(port()));
}

void EventServer::setUpdaterState(const QJsonObject &state)
{
    m_updaterState = state;
}

void EventServer::acceptConnections()
{
    while (m_tcp.hasPendingConnections()) {
        QTcpSocket *socket = m_tcp.nextPendingConnection();
        if (!socket) continue;
        m_clients.insert(socket, {});
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { readClient(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] { removeClient(socket); });
    }
}

void EventServer::removeClient(QTcpSocket *socket)
{
    const bool wasLastActive = m_lastActive == socket;
    m_clients.remove(socket);
    socket->deleteLater();
    if (wasLastActive) {
        m_lastActive.clear();
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            if (it.value().websocket) {
                m_lastActive = it.key();
                break;
            }
        }
    }
}

void EventServer::readClient(QTcpSocket *socket)
{
    auto it = m_clients.find(socket);
    if (it == m_clients.end()) return;
    ClientState &state = it.value();
    state.buffer.append(socket->readAll());

    if (state.buffer.size() > MaxRequestBytes && !state.websocket) {
        respond(socket, 413, QByteArrayLiteral("Request too large"));
        return;
    }
    if (state.websocket) {
        consumeWebSocketFrames(socket, state);
        return;
    }

    const auto request = takeHttpRequest(state, socket);
    if (request) handleHttp(socket, *request);
}

std::optional<EventServer::HttpRequest> EventServer::takeHttpRequest(ClientState &state, QTcpSocket *socket)
{
    const qsizetype headerEnd = state.buffer.indexOf(QByteArrayLiteral("\r\n\r\n"));
    if (headerEnd < 0) return std::nullopt;

    const QByteArray headerBlock = state.buffer.left(headerEnd);
    QList<QByteArray> lines = headerBlock.split('\n');
    if (lines.isEmpty()) {
        respond(socket, 400, QByteArrayLiteral("Malformed request"));
        return std::nullopt;
    }

    const QByteArray requestLine = lines.takeFirst().trimmed();
    const QList<QByteArray> requestParts = requestLine.split(' ');
    if (requestParts.size() < 3) {
        respond(socket, 400, QByteArrayLiteral("Malformed request line"));
        return std::nullopt;
    }

    HttpRequest request;
    request.method = requestParts.at(0).trimmed().toUpper();
    request.target = requestParts.at(1).trimmed();
    for (const QByteArray &rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0) continue;
        request.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
    }

    bool ok = true;
    const qint64 contentLength = request.headers.value(QByteArrayLiteral("content-length"), QByteArrayLiteral("0")).toLongLong(&ok);
    if (!ok || contentLength < 0 || contentLength > MaxRequestBytes) {
        respond(socket, 413, QByteArrayLiteral("Invalid content length"));
        return std::nullopt;
    }

    const qint64 total = static_cast<qint64>(headerEnd) + 4 + contentLength;
    if (state.buffer.size() < total) return std::nullopt;
    request.body = state.buffer.mid(headerEnd + 4, contentLength);
    state.buffer.remove(0, total);
    return request;
}

void EventServer::handleHttp(QTcpSocket *socket, const HttpRequest &request)
{
    const QUrl parsed = QUrl::fromEncoded(request.target);
    const QString path = parsed.path();

    if (path == QStringLiteral("/events") &&
        request.headers.value(QByteArrayLiteral("upgrade")).compare(QByteArrayLiteral("websocket"), Qt::CaseInsensitive) == 0) {
        upgradeWebSocket(socket, request);
        return;
    }
    if (path == QStringLiteral("/config") && request.method == QByteArrayLiteral("GET")) {
        handleConfig(socket);
        return;
    }
    if (path.startsWith(QStringLiteral("/uploads/"))) {
        handleUpload(socket, request, path);
        return;
    }
    if (path.startsWith(QStringLiteral("/proxy/"))) {
        handleProxy(socket, request);
        return;
    }
    handleStatic(socket, request);
}

void EventServer::handleConfig(QTcpSocket *socket)
{
    const QString contents = m_configStore->load();
    QJsonObject state{
        {QStringLiteral("version"), QStringLiteral(APT_NATIVE_VERSION)},
        {QStringLiteral("updater"), m_updaterState},
        {QStringLiteral("contents"), contents.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(contents)}
    };
    respond(socket, 200, QJsonDocument(state).toJson(QJsonDocument::Compact), QByteArrayLiteral("application/json"));
}

void EventServer::handleUpload(QTcpSocket *socket, const HttpRequest &request, const QString &path)
{
    const QString requestedName = QFileInfo(path.mid(QStringLiteral("/uploads/").size())).fileName();
    if (requestedName.isEmpty()) {
        respond(socket, 400, QByteArrayLiteral("Missing filename"));
        return;
    }
    const QString uploads = m_configStore->uploadsDirectory();
    QDir().mkpath(uploads);

    if (request.method == QByteArrayLiteral("GET")) {
        QFile file(QDir(uploads).filePath(requestedName));
        if (!file.open(QIODevice::ReadOnly)) {
            respond(socket, 404, QByteArrayLiteral("Not found"));
            return;
        }
        respond(socket, 200, file.readAll(), mimeType(requestedName));
        return;
    }

    if (request.method == QByteArrayLiteral("POST")) {
        const QByteArray hash = QCryptographicHash::hash(request.body, QCryptographicHash::Md5).toHex();
        const QString extension = QFileInfo(requestedName).suffix();
        const QString stored = QString::fromLatin1(hash) + (extension.isEmpty() ? QString{} : QStringLiteral(".") + extension);
        QFile file(QDir(uploads).filePath(stored));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(request.body) != request.body.size()) {
            respond(socket, 500, QByteArrayLiteral("Failed to save upload"));
            return;
        }
        const QJsonObject body{{QStringLiteral("name"), stored}};
        respond(socket, 200, QJsonDocument(body).toJson(QJsonDocument::Compact), QByteArrayLiteral("application/json"));
        return;
    }

    respond(socket, 405, QByteArrayLiteral("Method not allowed"));
}

void EventServer::handleProxy(QTcpSocket *socket, const HttpRequest &request)
{
    const QByteArray prefix = QByteArrayLiteral("/proxy/");
    const QByteArray encodedDestination = request.target.mid(prefix.size());
    const QUrl destination = QUrl::fromEncoded(QByteArrayLiteral("https://") + encodedDestination);
    if (!destination.isValid() || !AllowedProxyHosts.contains(destination.host())) {
        respond(socket, 403, QByteArrayLiteral("Proxy host is not allowed"));
        return;
    }

    QNetworkRequest outgoing(destination);
    outgoing.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    outgoing.setRawHeader(QByteArrayLiteral("user-agent"), QByteArrayLiteral("Awakened-PoE-Trade-Native/") + QByteArray(APT_NATIVE_VERSION));
    outgoing.setRawHeader(QByteArrayLiteral("accept-encoding"), QByteArrayLiteral("identity"));

    for (auto it = request.headers.cbegin(); it != request.headers.cend(); ++it) {
        const QByteArray key = it.key().toLower();
        if (key.startsWith(QByteArrayLiteral("sec-")) || key == QByteArrayLiteral("host") ||
            key == QByteArrayLiteral("origin") || key == QByteArrayLiteral("content-length") ||
            key == QByteArrayLiteral("connection") || key == QByteArrayLiteral("accept-encoding")) {
            continue;
        }
        outgoing.setRawHeader(key, it.value());
    }

    QNetworkReply *reply = nullptr;
    if (request.method == QByteArrayLiteral("GET")) reply = m_network.get(outgoing);
    else if (request.method == QByteArrayLiteral("POST")) reply = m_network.post(outgoing, request.body);
    else if (request.method == QByteArrayLiteral("PUT")) reply = m_network.put(outgoing, request.body);
    else if (request.method == QByteArrayLiteral("DELETE")) reply = m_network.sendCustomRequest(outgoing, QByteArrayLiteral("DELETE"), request.body);
    else reply = m_network.sendCustomRequest(outgoing, request.method, request.body);

    QPointer<QTcpSocket> guardedSocket(socket);
    connect(reply, &QNetworkReply::finished, this, [this, reply, guardedSocket, destination] {
        if (!guardedSocket) {
            reply->deleteLater();
            return;
        }
        const QByteArray body = reply->readAll();
        if (body.size() > MaxProxyResponse) {
            respond(guardedSocket, 502, QByteArrayLiteral("Proxy response too large"));
            reply->deleteLater();
            return;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QList<QPair<QByteArray, QByteArray>> headers;
        for (const auto &pair : reply->rawHeaderPairs()) {
            const QByteArray lower = pair.first.toLower();
            if (lower == QByteArrayLiteral("content-length") ||
                lower == QByteArrayLiteral("transfer-encoding") ||
                lower == QByteArrayLiteral("content-encoding") ||
                lower == QByteArrayLiteral("content-type") ||
                lower == QByteArrayLiteral("connection")) continue;
            headers.append(pair);
        }
        const QByteArray contentType = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
        if (reply->error() != QNetworkReply::NoError && status == 0) {
            m_logger->write(QStringLiteral("error [cors-proxy] %1 (%2)")
                .arg(reply->errorString(), destination.host()));
            respond(guardedSocket, 502, reply->errorString().toUtf8());
        } else {
            respond(guardedSocket, status == 0 ? 200 : status, body,
                    contentType.isEmpty() ? QByteArrayLiteral("application/octet-stream") : contentType,
                    headers);
        }
        reply->deleteLater();
    });
}

void EventServer::handleStatic(QTcpSocket *socket, const HttpRequest &request)
{
    if (request.method != QByteArrayLiteral("GET") && request.method != QByteArrayLiteral("HEAD")) {
        respond(socket, 405, QByteArrayLiteral("Method not allowed"));
        return;
    }

    const QUrl url = QUrl::fromEncoded(request.target);
    QString relative = url.path();
    if (relative == QStringLiteral("/")) relative = QStringLiteral("/index.html");
    relative = QDir::cleanPath(relative.mid(1));
    const QString candidate = QDir(m_rendererRoot).absoluteFilePath(relative);
    if (!pathInside(m_rendererRoot, candidate)) {
        respond(socket, 403, QByteArrayLiteral("Forbidden"));
        return;
    }

    QFile file(candidate);
    if (!file.open(QIODevice::ReadOnly)) {
        respond(socket, 404, QByteArrayLiteral("Not found"));
        return;
    }
    const QByteArray body = request.method == QByteArrayLiteral("HEAD") ? QByteArray{} : file.readAll();
    respond(socket, 200, body, mimeType(candidate));
}

void EventServer::upgradeWebSocket(QTcpSocket *socket, const HttpRequest &request)
{
    const QByteArray key = request.headers.value(QByteArrayLiteral("sec-websocket-key"));
    if (key.isEmpty()) {
        respond(socket, 400, QByteArrayLiteral("Missing WebSocket key"));
        return;
    }
    const QByteArray accept = QCryptographicHash::hash(key + WebSocketGuid, QCryptographicHash::Sha1).toBase64();
    QByteArray response = QByteArrayLiteral("HTTP/1.1 101 Switching Protocols\r\n")
        + QByteArrayLiteral("Upgrade: websocket\r\n")
        + QByteArrayLiteral("Connection: Upgrade\r\n")
        + QByteArrayLiteral("Sec-WebSocket-Accept: ") + accept + QByteArrayLiteral("\r\n\r\n");
    socket->write(response);
    m_clients[socket].websocket = true;
    m_lastActive = socket;
    emit clientConnected();

    const QString history = m_logger->history();
    if (!history.isEmpty()) {
        sendFrame(socket, QJsonDocument(makeEvent(
            QStringLiteral("MAIN->CLIENT::log-entry"),
            QJsonObject{{QStringLiteral("message"), history}})).toJson(QJsonDocument::Compact));
    }
    ClientState &state = m_clients[socket];
    if (!state.buffer.isEmpty()) consumeWebSocketFrames(socket, state);
}

void EventServer::consumeWebSocketFrames(QTcpSocket *socket, ClientState &state)
{
    while (state.buffer.size() >= 2) {
        const auto first = static_cast<quint8>(state.buffer.at(0));
        const auto second = static_cast<quint8>(state.buffer.at(1));
        const bool finalFrame = (first & 0x80U) != 0;
        const quint8 opcode = first & 0x0FU;
        const bool masked = (second & 0x80U) != 0;
        quint64 length = second & 0x7FU;
        qsizetype offset = 2;

        if (!finalFrame) {
            socket->disconnectFromHost();
            return;
        }
        if (length == 126) {
            if (state.buffer.size() < 4) return;
            length = (static_cast<quint8>(state.buffer.at(2)) << 8U) |
                     static_cast<quint8>(state.buffer.at(3));
            offset = 4;
        } else if (length == 127) {
            if (state.buffer.size() < 10) return;
            length = 0;
            for (int i = 2; i < 10; ++i) {
                length = (length << 8U) | static_cast<quint8>(state.buffer.at(i));
            }
            offset = 10;
        }
        if (length > static_cast<quint64>(MaxWebSocketPayload)) {
            socket->disconnectFromHost();
            return;
        }

        QByteArray mask;
        if (masked) {
            if (state.buffer.size() < offset + 4) return;
            mask = state.buffer.mid(offset, 4);
            offset += 4;
        }
        if (static_cast<quint64>(state.buffer.size() - offset) < length) return;

        QByteArray payload = state.buffer.mid(offset, static_cast<qsizetype>(length));
        state.buffer.remove(0, offset + static_cast<qsizetype>(length));
        if (masked) {
            for (qsizetype i = 0; i < payload.size(); ++i) payload[i] = static_cast<char>(payload.at(i) ^ mask.at(i % 4));
        }

        if (opcode == 0x1) processWebSocketMessage(socket, payload);
        else if (opcode == 0x8) {
            sendFrame(socket, {}, 0x8);
            socket->disconnectFromHost();
            return;
        } else if (opcode == 0x9) {
            sendFrame(socket, payload, 0xA);
        }
    }
}

void EventServer::processWebSocketMessage(QTcpSocket *socket, const QByteArray &payload)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        m_logger->write(QStringLiteral("warn [Server] Ignored malformed WebSocket event."));
        return;
    }
    const QJsonObject event = document.object();
    const QString name = event.value(QStringLiteral("name")).toString();
    if (name.isEmpty()) return;
    if (name == QStringLiteral("CLIENT->MAIN::used-recently")) m_lastActive = socket;
    emit eventReceived(name, event.value(QStringLiteral("payload")));
}

QByteArray EventServer::websocketFrame(const QByteArray &payload, quint8 opcode)
{
    QByteArray frame;
    frame.append(static_cast<char>(0x80U | (opcode & 0x0FU)));
    const quint64 length = static_cast<quint64>(payload.size());
    if (length < 126) {
        frame.append(static_cast<char>(length));
    } else if (length <= 0xFFFFU) {
        frame.append(static_cast<char>(126));
        frame.append(static_cast<char>((length >> 8U) & 0xFFU));
        frame.append(static_cast<char>(length & 0xFFU));
    } else {
        frame.append(static_cast<char>(127));
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.append(static_cast<char>((length >> static_cast<unsigned>(shift)) & 0xFFU));
        }
    }
    frame.append(payload);
    return frame;
}

void EventServer::sendFrame(QTcpSocket *socket, const QByteArray &payload, quint8 opcode)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;
    socket->write(websocketFrame(payload, opcode));
}

void EventServer::broadcast(const QJsonObject &event)
{
    const QByteArray payload = QJsonDocument(event).toJson(QJsonDocument::Compact);
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value().websocket) sendFrame(it.key(), payload);
    }
}

void EventServer::sendToLastActive(const QJsonObject &event)
{
    if (!m_lastActive) {
        broadcast(event);
        return;
    }
    sendFrame(m_lastActive, QJsonDocument(event).toJson(QJsonDocument::Compact));
}

void EventServer::respond(QTcpSocket *socket,
                          int status,
                          const QByteArray &body,
                          const QByteArray &contentType,
                          const QList<QPair<QByteArray, QByteArray>> &headers)
{
    if (!socket) return;
    QByteArray response = QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(status) + QByteArrayLiteral(" ")
        + reasonPhrase(status) + QByteArrayLiteral("\r\n");
    response += QByteArrayLiteral("Content-Type: ") + contentType + QByteArrayLiteral("\r\n");
    response += QByteArrayLiteral("Content-Length: ") + QByteArray::number(body.size()) + QByteArrayLiteral("\r\n");
    response += QByteArrayLiteral("Connection: close\r\n");
    response += QByteArrayLiteral("X-Content-Type-Options: nosniff\r\n");
    for (const auto &header : headers) response += header.first + QByteArrayLiteral(": ") + header.second + QByteArrayLiteral("\r\n");
    response += QByteArrayLiteral("\r\n");
    response += body;
    socket->write(response);
    socket->disconnectFromHost();
}

QByteArray EventServer::mimeType(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("html")) return QByteArrayLiteral("text/html; charset=utf-8");
    if (suffix == QStringLiteral("js") || suffix == QStringLiteral("mjs")) return QByteArrayLiteral("text/javascript; charset=utf-8");
    if (suffix == QStringLiteral("css")) return QByteArrayLiteral("text/css; charset=utf-8");
    if (suffix == QStringLiteral("json")) return QByteArrayLiteral("application/json");
    if (suffix == QStringLiteral("svg")) return QByteArrayLiteral("image/svg+xml");
    if (suffix == QStringLiteral("png")) return QByteArrayLiteral("image/png");
    if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")) return QByteArrayLiteral("image/jpeg");
    if (suffix == QStringLiteral("webp")) return QByteArrayLiteral("image/webp");
    if (suffix == QStringLiteral("woff")) return QByteArrayLiteral("font/woff");
    if (suffix == QStringLiteral("woff2")) return QByteArrayLiteral("font/woff2");
    if (suffix == QStringLiteral("wasm")) return QByteArrayLiteral("application/wasm");
    return QByteArrayLiteral("application/octet-stream");
}

QByteArray EventServer::reasonPhrase(int status)
{
    switch (status) {
    case 200: return QByteArrayLiteral("OK");
    case 400: return QByteArrayLiteral("Bad Request");
    case 403: return QByteArrayLiteral("Forbidden");
    case 404: return QByteArrayLiteral("Not Found");
    case 405: return QByteArrayLiteral("Method Not Allowed");
    case 413: return QByteArrayLiteral("Payload Too Large");
    case 500: return QByteArrayLiteral("Internal Server Error");
    case 502: return QByteArrayLiteral("Bad Gateway");
    default: return QByteArrayLiteral("Response");
    }
}

bool EventServer::pathInside(const QString &root, const QString &candidate)
{
    const QString cleanRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    const QString cleanCandidate = QDir::cleanPath(QFileInfo(candidate).absoluteFilePath());
    return cleanCandidate == cleanRoot || cleanCandidate.startsWith(cleanRoot + QDir::separator());
}

} // namespace AptNative
