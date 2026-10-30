#pragma once

#include <QObject>
#include <QQmlEngine>

class KeyboardHaptics : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit KeyboardHaptics(QObject* parent = nullptr);

    Q_INVOKABLE void keyPress() const;
    Q_INVOKABLE void longPress() const;
};
