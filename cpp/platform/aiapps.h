#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>

namespace ai_apps
{

QVariantList catalog();
QStringList names();
QString idAt(int index);
int indexOf(const QString& id);
void open(const QString& id);

}  // namespace ai_apps
