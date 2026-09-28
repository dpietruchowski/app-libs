#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class KeyboardBuffer : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(
        int cursorPosition READ cursorPosition WRITE setCursorPosition NOTIFY cursorPositionChanged)
    Q_PROPERTY(ShiftState shiftState READ shiftState NOTIFY shiftStateChanged)
    Q_PROPERTY(bool upperCase READ upperCase NOTIFY shiftStateChanged)

public:
    enum class ShiftState
    {
        Off,
        Once,
        Locked
    };
    Q_ENUM(ShiftState)

    explicit KeyboardBuffer(QObject* parent = nullptr);

    QString text() const;
    void setText(const QString& text);
    int cursorPosition() const;
    void setCursorPosition(int position);
    ShiftState shiftState() const;
    bool upperCase() const;

    Q_INVOKABLE void insert(const QString& text);
    Q_INVOKABLE void typeKey(const QString& key);
    Q_INVOKABLE void backspace();
    Q_INVOKABLE void deleteForward();
    Q_INVOKABLE void moveCursor(int steps);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void toggleShift();
    Q_INVOKABLE void lockShift();
    Q_INVOKABLE void submit();

signals:
    void textChanged();
    void cursorPositionChanged();
    void shiftStateChanged();
    void submitted(const QString& text);

private:
    int previousBoundary(int position) const;
    int nextBoundary(int position) const;
    void apply(const QString& text, int cursorPosition);
    void setShiftState(ShiftState state);

    QString m_text;
    int m_cursorPosition = 0;
    ShiftState m_shiftState = ShiftState::Off;
};
