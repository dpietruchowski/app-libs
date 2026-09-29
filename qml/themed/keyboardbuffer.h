#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

class KeyboardBuffer : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(
        int cursorPosition READ cursorPosition WRITE setCursorPosition NOTIFY cursorPositionChanged)
    Q_PROPERTY(ShiftState shiftState READ shiftState NOTIFY shiftStateChanged)
    Q_PROPERTY(bool upperCase READ upperCase NOTIFY shiftStateChanged)
    Q_PROPERTY(bool composing READ composing NOTIFY compositionChanged)
    Q_PROPERTY(int compositionStart READ compositionStart NOTIFY compositionChanged)
    Q_PROPERTY(int compositionLength READ compositionLength NOTIFY compositionChanged)

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
    bool composing() const;
    int compositionStart() const;
    int compositionLength() const;

    Q_INVOKABLE void insert(const QString& text);
    Q_INVOKABLE void typeKey(const QString& key);
    Q_INVOKABLE void backspace();
    Q_INVOKABLE void deleteForward();
    Q_INVOKABLE void moveCursor(int steps);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void toggleShift();
    Q_INVOKABLE void lockShift();
    Q_INVOKABLE void submit();
    Q_INVOKABLE void compose(const QStringList& characters);
    Q_INVOKABLE void commitComposition();

signals:
    void textChanged();
    void cursorPositionChanged();
    void shiftStateChanged();
    void compositionChanged();
    void submitted(const QString& text);

private:
    int previousBoundary(int position) const;
    int nextBoundary(int position) const;
    void apply(const QString& text, int cursorPosition);
    void placeCursor(int position);
    void setShiftState(ShiftState state);
    QString composedCharacter() const;

    QString m_text;
    int m_cursorPosition = 0;
    ShiftState m_shiftState = ShiftState::Off;
    QStringList m_composition;
    int m_compositionIndex = 0;
    int m_compositionStart = 0;
    bool m_compositionUpperCase = false;
};
