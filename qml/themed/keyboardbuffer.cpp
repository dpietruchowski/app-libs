#include "keyboardbuffer.h"

#include <algorithm>

KeyboardBuffer::KeyboardBuffer(QObject* parent)
    : QObject(parent)
{
}

QString KeyboardBuffer::text() const { return m_text; }

void KeyboardBuffer::setText(const QString& text) { apply(text, static_cast<int>(text.size())); }

int KeyboardBuffer::cursorPosition() const { return m_cursorPosition; }

void KeyboardBuffer::setCursorPosition(int position)
{
    const int clamped = std::clamp(position, 0, static_cast<int>(m_text.size()));
    if (clamped == m_cursorPosition)
        return;

    m_cursorPosition = clamped;
    emit cursorPositionChanged();
}

KeyboardBuffer::ShiftState KeyboardBuffer::shiftState() const { return m_shiftState; }

bool KeyboardBuffer::upperCase() const { return m_shiftState != ShiftState::Off; }

void KeyboardBuffer::insert(const QString& text)
{
    if (text.isEmpty())
        return;

    QString updated = m_text;
    updated.insert(m_cursorPosition, text);
    apply(updated, m_cursorPosition + static_cast<int>(text.size()));
}

void KeyboardBuffer::typeKey(const QString& key)
{
    insert(upperCase() ? key.toUpper() : key);
    if (m_shiftState == ShiftState::Once)
        setShiftState(ShiftState::Off);
}

void KeyboardBuffer::backspace()
{
    const int start = previousBoundary(m_cursorPosition);
    if (start == m_cursorPosition)
        return;

    QString updated = m_text;
    updated.remove(start, m_cursorPosition - start);
    apply(updated, start);
}

void KeyboardBuffer::deleteForward()
{
    const int end = nextBoundary(m_cursorPosition);
    if (end == m_cursorPosition)
        return;

    QString updated = m_text;
    updated.remove(m_cursorPosition, end - m_cursorPosition);
    apply(updated, m_cursorPosition);
}

void KeyboardBuffer::moveCursor(int steps)
{
    int position = m_cursorPosition;
    for (; steps < 0; ++steps)
        position = previousBoundary(position);
    for (; steps > 0; --steps)
        position = nextBoundary(position);
    setCursorPosition(position);
}

void KeyboardBuffer::clear() { apply(QString(), 0); }

void KeyboardBuffer::toggleShift()
{
    setShiftState(m_shiftState == ShiftState::Off ? ShiftState::Once : ShiftState::Off);
}

void KeyboardBuffer::lockShift() { setShiftState(ShiftState::Locked); }

void KeyboardBuffer::submit()
{
    const QString trimmed = m_text.trimmed();
    if (!trimmed.isEmpty())
        emit submitted(trimmed);
}

int KeyboardBuffer::previousBoundary(int position) const
{
    if (position <= 0)
        return 0;
    if (position >= 2 && m_text.at(position - 1).isLowSurrogate()
        && m_text.at(position - 2).isHighSurrogate())
        return position - 2;
    return position - 1;
}

int KeyboardBuffer::nextBoundary(int position) const
{
    const int size = static_cast<int>(m_text.size());
    if (position >= size)
        return size;
    if (position + 1 < size && m_text.at(position).isHighSurrogate()
        && m_text.at(position + 1).isLowSurrogate())
        return position + 2;
    return position + 1;
}

void KeyboardBuffer::apply(const QString& text, int cursorPosition)
{
    const bool textDiffers = text != m_text;
    m_text = text;
    if (textDiffers)
        emit textChanged();
    setCursorPosition(cursorPosition);
}

void KeyboardBuffer::setShiftState(ShiftState state)
{
    if (state == m_shiftState)
        return;

    m_shiftState = state;
    emit shiftStateChanged();
}
