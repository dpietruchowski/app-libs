#pragma once

#include <QObject>

class QTimer;

class KeyboardInsetProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int bottom READ bottom NOTIFY bottomChanged)
    Q_PROPERTY(int rememberedHeight READ rememberedHeight WRITE setRememberedHeight NOTIFY
                   rememberedHeightChanged)

public:
    explicit KeyboardInsetProvider(QObject* parent = nullptr);
    ~KeyboardInsetProvider() override;

    int bottom() const { return m_bottom; }
    int rememberedHeight() const { return m_rememberedHeight; }
    void setRememberedHeight(int height);

    void setBottomFromPx(int px);

signals:
    void bottomChanged();
    void rememberedHeightChanged();

private:
    void refreshImeInset();
    void setBottom(int bottom);

    int m_bottom = 0;
    int m_rememberedHeight = 0;
    QTimer* m_imeDebounce = nullptr;
};
