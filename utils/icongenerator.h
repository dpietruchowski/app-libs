#pragma once

#include <QString>

class IconGenerator
{
public:
    static QString svg(const QString& seed);
    static QString svg(quint32 seed);
    static quint32 seedFor(const QString& text);
    static bool writeTo(const QString& filePath, const QString& seed, QString* error);
};
