#include "RuntimePaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace AptNative::RuntimePaths {

QString findDataFile(const QString &relativePath,
                     const QString &applicationDirectory)
{
    const QString executableDirectory = applicationDirectory.isEmpty()
        ? QCoreApplication::applicationDirPath()
        : applicationDirectory;

    // Prefer data shipped beside the executable. AppImages place their binary
    // in usr/bin and application data in usr/share, but do not consistently
    // add that private share directory to XDG_DATA_DIRS on every host.
    const QString bundled = QDir(executableDirectory)
        .filePath(QStringLiteral("../share/%1").arg(relativePath));
    if (QFileInfo(bundled).isFile()) {
        return QFileInfo(bundled).canonicalFilePath();
    }

    return QStandardPaths::locate(
        QStandardPaths::GenericDataLocation, relativePath);
}

} // namespace AptNative::RuntimePaths
