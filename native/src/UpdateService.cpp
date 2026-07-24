#include "UpdateService.h"

#include "Logger.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QVersionNumber>

#include <utility>

namespace AptNative {
namespace {

QJsonObject timestampedState(const QString &state)
{
    return {
        {QStringLiteral("state"), state},
        {QStringLiteral("checkedAt"), QDateTime::currentMSecsSinceEpoch()}
    };
}

bool copyFileAtomically(const QString &source, const QString &destination)
{
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) return false;
    QSaveFile output(destination);
    if (!output.open(QIODevice::WriteOnly)) return false;

    QByteArray buffer;
    buffer.resize(1024 * 1024);
    while (!input.atEnd()) {
        const qint64 read = input.read(buffer.data(), buffer.size());
        if (read < 0 || output.write(buffer.constData(), read) != read) return false;
    }
    return output.commit();
}

} // namespace

UpdateService::UpdateService(
    QString repository,
    QString currentVersion,
    QString dataDirectory,
    bool disabledByFlag,
    Logger *logger,
    QObject *parent)
    : QObject(parent),
      m_repository(std::move(repository)),
      m_currentVersion(std::move(currentVersion)),
      m_dataDirectory(std::move(dataDirectory)),
      m_appImagePath(qEnvironmentVariable("APPIMAGE")),
      m_disabledByFlag(disabledByFlag),
      m_logger(logger)
{
}

QJsonObject UpdateService::state() const
{
    return m_state;
}

void UpdateService::setState(QJsonObject state)
{
    m_state = std::move(state);
    emit stateChanged(m_state);
}

QString UpdateService::normalizedVersion(const QString &version)
{
    QString normalized = version.trimmed();
    while (!normalized.isEmpty() && !normalized.front().isDigit()) {
        normalized.remove(0, 1);
    }
    normalized.replace(QStringLiteral("-native."), QStringLiteral("."));
    const qsizetype dash = normalized.indexOf(QLatin1Char('-'));
    if (dash >= 0) normalized.truncate(dash);
    return normalized;
}

bool UpdateService::canReplaceAppImage() const
{
    const QFileInfo info(m_appImagePath);
    return !m_appImagePath.isEmpty() && info.isFile() && info.isWritable();
}

void UpdateService::check()
{
    if (m_requestPending) return;
    if (m_repository.isEmpty()) {
        m_logger->write(QStringLiteral(
            "error [Updater] No GitHub update repository was compiled into this build."));
        setState(timestampedState(QStringLiteral("error")));
        return;
    }

    m_requestPending = true;
    setState({{QStringLiteral("state"), QStringLiteral("checking-for-update")}});

    QNetworkRequest request(QUrl(
        QStringLiteral("https://api.github.com/repos/%1/releases/latest")
            .arg(m_repository)));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setRawHeader("User-Agent", "Awakened-PoE-Trade-Native-Updater");
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_requestPending = false;
        const QByteArray body = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString error = reply->errorString();
        reply->deleteLater();
        if (!ok) {
            m_logger->write(QStringLiteral("error [Updater] Release check failed: %1").arg(error));
            setState(timestampedState(QStringLiteral("error")));
            return;
        }
        processRelease(body);
    });
}

void UpdateService::processRelease(const QByteArray &document)
{
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(document, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        m_logger->write(QStringLiteral("error [Updater] GitHub returned invalid release metadata."));
        setState(timestampedState(QStringLiteral("error")));
        return;
    }

    const QJsonObject release = json.object();
    const QString version = normalizedVersion(
        release.value(QStringLiteral("tag_name")).toString());
    if (version.isEmpty() ||
        QVersionNumber::compare(
            QVersionNumber::fromString(version),
            QVersionNumber::fromString(normalizedVersion(m_currentVersion))) <= 0) {
        setState(timestampedState(QStringLiteral("update-not-available")));
        return;
    }

    QJsonObject selectedAsset;
    for (const QJsonValue &value : release.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (name.endsWith(QStringLiteral(".AppImage"), Qt::CaseInsensitive) &&
            name.contains(QStringLiteral("native"), Qt::CaseInsensitive) &&
            (name.contains(QStringLiteral("x86_64"), Qt::CaseInsensitive) ||
             name.contains(QStringLiteral("x64"), Qt::CaseInsensitive))) {
            selectedAsset = asset;
            break;
        }
    }

    const QString noDownloadReason =
        m_disabledByFlag ? QStringLiteral("disabled-by-flag")
                         : (!canReplaceAppImage() || selectedAsset.isEmpty()
                            ? QStringLiteral("not-supported")
                            : QString{});
    setState({
        {QStringLiteral("state"), QStringLiteral("update-available")},
        {QStringLiteral("version"), version},
        {QStringLiteral("noDownloadReason"),
         noDownloadReason.isEmpty() ? QJsonValue::Null : QJsonValue(noDownloadReason)}
    });
    if (!noDownloadReason.isEmpty()) return;

    const QString digest = selectedAsset.value(QStringLiteral("digest")).toString();
    const QUrl url(selectedAsset.value(QStringLiteral("browser_download_url")).toString());
    if (!digest.startsWith(QStringLiteral("sha256:")) || !url.isValid()) {
        m_logger->write(QStringLiteral(
            "error [Updater] Native release is missing its GitHub SHA-256 digest."));
        setState(timestampedState(QStringLiteral("error")));
        return;
    }
    download(
        url,
        selectedAsset.value(QStringLiteral("name")).toString(),
        digest.mid(7).toLatin1().toLower(),
        version);
}

void UpdateService::download(
    const QUrl &url,
    const QString &assetName,
    const QByteArray &expectedSha256,
    const QString &version)
{
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/octet-stream");
    request.setRawHeader("User-Agent", "Awakened-PoE-Trade-Native-Updater");
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, assetName, expectedSha256, version] {
        const QByteArray contents = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString error = reply->errorString();
        reply->deleteLater();
        if (!ok) {
            m_logger->write(QStringLiteral("error [Updater] Download failed: %1").arg(error));
            setState(timestampedState(QStringLiteral("error")));
            return;
        }
        const QByteArray actual =
            QCryptographicHash::hash(contents, QCryptographicHash::Sha256).toHex();
        if (actual != expectedSha256) {
            m_logger->write(QStringLiteral("error [Updater] Download digest did not match GitHub."));
            setState(timestampedState(QStringLiteral("error")));
            return;
        }

        const QString updateDirectory =
            QDir(m_dataDirectory).filePath(QStringLiteral("updates"));
        if (!QDir().mkpath(updateDirectory)) {
            setState(timestampedState(QStringLiteral("error")));
            return;
        }
        const QString destination = QDir(updateDirectory).filePath(assetName);
        QSaveFile file(destination);
        if (!file.open(QIODevice::WriteOnly) ||
            file.write(contents) != contents.size() ||
            !file.commit() ||
            !QFile::setPermissions(destination,
                QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                QFileDevice::ExeGroup | QFileDevice::ReadOther |
                QFileDevice::ExeOther)) {
            m_logger->write(QStringLiteral("error [Updater] Could not stage the downloaded AppImage."));
            setState(timestampedState(QStringLiteral("error")));
            return;
        }
        m_pendingPath = destination;
        m_pendingVersion = version;
        setState({
            {QStringLiteral("state"), QStringLiteral("update-downloaded")},
            {QStringLiteral("version"), version}
        });
    });
}

void UpdateService::installAndRestart()
{
    if (m_pendingPath.isEmpty() || !QFileInfo::exists(m_pendingPath) ||
        !canReplaceAppImage()) {
        m_logger->write(QStringLiteral("error [Updater] No installable native update is staged."));
        setState(timestampedState(QStringLiteral("error")));
        return;
    }

    const QString replacement = m_appImagePath + QStringLiteral(".new");
    const QString backup = m_appImagePath + QStringLiteral(".old");
    QFile::remove(replacement);
    if (!copyFileAtomically(m_pendingPath, replacement) ||
        !QFile::setPermissions(replacement, QFileInfo(m_appImagePath).permissions())) {
        m_logger->write(QStringLiteral("error [Updater] Could not prepare the replacement AppImage."));
        setState(timestampedState(QStringLiteral("error")));
        return;
    }

    QFile::remove(backup);
    if (!QFile::rename(m_appImagePath, backup)) {
        QFile::remove(replacement);
        m_logger->write(QStringLiteral("error [Updater] Could not back up the running AppImage."));
        setState(timestampedState(QStringLiteral("error")));
        return;
    }
    if (!QFile::rename(replacement, m_appImagePath)) {
        QFile::rename(backup, m_appImagePath);
        m_logger->write(QStringLiteral("error [Updater] Could not activate the new AppImage."));
        setState(timestampedState(QStringLiteral("error")));
        return;
    }

    if (!QProcess::startDetached(m_appImagePath, QCoreApplication::arguments().mid(1))) {
        QFile::remove(m_appImagePath);
        QFile::rename(backup, m_appImagePath);
        m_logger->write(QStringLiteral("error [Updater] Restart failed; the previous AppImage was restored."));
        setState(timestampedState(QStringLiteral("error")));
        return;
    }
    m_logger->write(QStringLiteral("info [Updater] Installed native v%1; restarting.")
        .arg(m_pendingVersion));
    QCoreApplication::quit();
}

} // namespace AptNative
