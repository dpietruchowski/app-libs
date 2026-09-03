#pragma once

#include <QString>
#include <QVariantMap>

class StringTemplate
{
public:
    explicit StringTemplate(QString source);

    QString render(const QVariantMap& values) const;

    const QString& source() const;

private:
    QString m_source;
};
