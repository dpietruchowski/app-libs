#pragma once

#include <QImage>
#include <QString>

namespace android
{

class AndroidPackage final
{
public:
    explicit AndroidPackage(QString package);

    bool isInstalled() const;
    void launch() const;
    QImage loadIcon() const;

private:
    QString m_package;
};

}  // namespace android
