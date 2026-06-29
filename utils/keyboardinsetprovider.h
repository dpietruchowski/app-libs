#pragma once

#include <QObject>

class KeyboardInsetProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int bottom READ bottom NOTIFY bottomChanged)

public:
    explicit KeyboardInsetProvider(QObject* parent = nullptr);
    ~KeyboardInsetProvider() override;

    int bottom() const { return m_bottom; }

    void setBottomFromPx(int px);

signals:
    void bottomChanged();

private:
    void refreshImeInset();
    void setBottom(int bottom);

    int m_bottom = 0;
};
