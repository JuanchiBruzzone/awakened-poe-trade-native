#pragma once

#include <QString>

namespace AptNative::RuntimePaths {

QString findDataFile(const QString &relativePath,
                     const QString &applicationDirectory = {});

} // namespace AptNative::RuntimePaths
