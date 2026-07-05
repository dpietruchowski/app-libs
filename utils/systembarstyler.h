#pragma once

#include <QColor>
#include <QObject>

class SystemBarStyler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY systemDarkChanged)

public:
    explicit SystemBarStyler(QObject* parent = nullptr);

    bool systemDark() const;

    Q_INVOKABLE void apply(bool darkTheme, const QColor& background);

signals:
    void systemDarkChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};
