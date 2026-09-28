#pragma once

#include <QObject>
#include <QString>

class QTimer;

class KeyboardInsetProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int bottom READ bottom NOTIFY bottomChanged)
    Q_PROPERTY(int rememberedHeight READ rememberedHeight NOTIFY rememberedHeightChanged)

public:
    explicit KeyboardInsetProvider(const QString& settingsPath = {}, QObject* parent = nullptr);
    ~KeyboardInsetProvider() override;

    int bottom() const { return m_bottom; }
    int rememberedHeight() const { return m_rememberedHeight; }

    void setBottomFromPx(int px);

signals:
    void bottomChanged();
    void rememberedHeightChanged();

private:
    void refreshImeInset();
    void setBottom(int bottom);
    void rememberHeight(int height);

    QString m_settingsPath;
    int m_bottom = 0;
    int m_rememberedHeight = 0;
    QTimer* m_imeDebounce = nullptr;
};
