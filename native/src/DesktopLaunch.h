#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace AptNative::DesktopLaunch {

QProcessEnvironment cleanEnvironment(const QString &activationToken = {});
bool startDetached(const QString &program, const QStringList &arguments,
                   const QString &activationToken = {});

} // namespace AptNative::DesktopLaunch
