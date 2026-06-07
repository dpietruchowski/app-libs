#pragma once

#include <QObject>

class SafeAreaProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int top READ top NOTIFY insetsChanged)
    Q_PROPERTY(int bottom READ bottom NOTIFY insetsChanged)
    Q_PROPERTY(int left READ left NOTIFY insetsChanged)
    Q_PROPERTY(int right READ right NOTIFY insetsChanged)

public:
    explicit SafeAreaProvider(QObject* parent = nullptr);

    int top() const { return m_top; }
    int bottom() const { return m_bottom; }
    int left() const { return m_left; }
    int right() const { return m_right; }

    Q_INVOKABLE void refresh();

signals:
    void insetsChanged();

private:
    void setInsets(int top, int bottom, int left, int right);

    int m_top = 0;
    int m_bottom = 0;
    int m_left = 0;
    int m_right = 0;
};
