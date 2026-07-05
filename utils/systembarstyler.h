#pragma once

#include <QColor>
#include <QObject>

class SystemBarStyler : public QObject
{
    Q_OBJECT

public:
    explicit SystemBarStyler(QObject* parent = nullptr);

    Q_INVOKABLE void apply(bool darkTheme, const QColor& background);
};
