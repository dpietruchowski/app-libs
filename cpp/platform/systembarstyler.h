#pragma once

#include <QObject>

class SystemBarStyler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY systemDarkChanged)

public:
    explicit SystemBarStyler(QObject* parent = nullptr);

    bool systemDark() const;

signals:
    void systemDarkChanged();
};
