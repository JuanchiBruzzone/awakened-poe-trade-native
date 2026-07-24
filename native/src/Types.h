#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace AptNative {

struct HostConfig {
    QJsonArray shortcuts;
    bool restoreClipboard = false;
    QString clientLog;
    QString gameConfig;
    bool stashScroll = false;
    QString overlayKey;
    bool logKeys = false;
    QString windowTitle = QStringLiteral("Path of Exile");
    QString language = QStringLiteral("en");

    static HostConfig fromJson(const QJsonObject &obj)
    {
        HostConfig cfg;
        cfg.shortcuts = obj.value(QStringLiteral("shortcuts")).toArray();
        cfg.restoreClipboard = obj.value(QStringLiteral("restoreClipboard")).toBool(false);
        cfg.clientLog = obj.value(QStringLiteral("clientLog")).toString();
        cfg.gameConfig = obj.value(QStringLiteral("gameConfig")).toString();
        cfg.stashScroll = obj.value(QStringLiteral("stashScroll")).toBool(false);
        cfg.overlayKey = obj.value(QStringLiteral("overlayKey")).toString();
        cfg.logKeys = obj.value(QStringLiteral("logKeys")).toBool(false);
        cfg.windowTitle = obj.value(QStringLiteral("windowTitle")).toString(QStringLiteral("Path of Exile"));
        cfg.language = obj.value(QStringLiteral("language")).toString(QStringLiteral("en"));
        return cfg;
    }
};

inline QJsonObject makeEvent(const QString &name,
                             const QJsonValue &payload = QJsonValue(QJsonValue::Undefined))
{
    QJsonObject event{{QStringLiteral("name"), name}};
    if (!payload.isUndefined()) {
        event.insert(QStringLiteral("payload"), payload);
    }
    return event;
}

} // namespace AptNative
