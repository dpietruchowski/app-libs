#include "keyboardbuffer.h"

#include <algorithm>

KeyboardBuffer::KeyboardBuffer(QObject* parent)
    : QObject(parent)
{
}

QString KeyboardBuffer::text() const { return m_text; }

void KeyboardBuffer::setText(const QString& text)
{
    commitComposition();
    apply(text, static_cast<int>(text.size()));
}

int KeyboardBuffer::cursorPosition() const { return m_cursorPosition; }

void KeyboardBuffer::setCursorPosition(int position)
{
    commitComposition();
    placeCursor(position);
}

KeyboardBuffer::ShiftState KeyboardBuffer::shiftState() const { return m_shiftState; }

bool KeyboardBuffer::upperCase() const { return m_shiftState != ShiftState::Off; }

bool KeyboardBuffer::composing() const { return !m_composition.isEmpty() || m_composingWord; }

int KeyboardBuffer::compositionStart() const
{
    return composing() ? m_compositionStart : m_cursorPosition;
}

int KeyboardBuffer::compositionLength() const
{
    return composing() ? m_cursorPosition - m_compositionStart : 0;
}

void KeyboardBuffer::insert(const QString& text)
{
    commitComposition();
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
    commitComposition();
    const int start = previousBoundary(m_cursorPosition);
    if (start == m_cursorPosition)
        return;

    QString updated = m_text;
    updated.remove(start, m_cursorPosition - start);
    apply(updated, start);
}

void KeyboardBuffer::deleteForward()
{
    commitComposition();
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

void KeyboardBuffer::clear()
{
    commitComposition();
    apply(QString(), 0);
}

void KeyboardBuffer::toggleShift()
{
    commitComposition();
    setShiftState(m_shiftState == ShiftState::Off ? ShiftState::Once : ShiftState::Off);
}

void KeyboardBuffer::lockShift()
{
    commitComposition();
    setShiftState(ShiftState::Locked);
}

void KeyboardBuffer::submit()
{
    commitComposition();
    const QString trimmed = m_text.trimmed();
    if (!trimmed.isEmpty())
        emit submitted(trimmed);
}

void KeyboardBuffer::compose(const QStringList& characters)
{
    if (characters.isEmpty())
        return;
    if (characters.size() == 1) {
        typeKey(characters.first());
        return;
    }

    QString updated = m_text;
    if (characters == m_composition) {
        m_compositionIndex = (m_compositionIndex + 1) % static_cast<int>(m_composition.size());
        updated.replace(m_compositionStart, compositionLength(), composedCharacter());
    } else {
        commitComposition();
        m_composition = characters;
        m_compositionIndex = 0;
        m_compositionStart = m_cursorPosition;
        m_compositionUpperCase = upperCase();
        if (m_shiftState == ShiftState::Once)
            setShiftState(ShiftState::Off);
        updated.insert(m_compositionStart, composedCharacter());
    }
    apply(updated, m_compositionStart + static_cast<int>(composedCharacter().size()));
    emit compositionChanged();
}

void KeyboardBuffer::composeWord(const QString& word)
{
    if (!m_composingWord) {
        commitComposition();
        if (word.isEmpty())
            return;
        m_composingWord = true;
        m_compositionStart = m_cursorPosition;
        m_wordShift = m_shiftState;
        if (m_shiftState == ShiftState::Once)
            setShiftState(ShiftState::Off);
    }

    const QString cased = casedWord(word);
    QString updated = m_text;
    updated.replace(m_compositionStart, compositionLength(), cased);
    m_composingWord = !word.isEmpty();
    apply(updated, m_compositionStart + static_cast<int>(cased.size()));
    emit compositionChanged();
}

void KeyboardBuffer::commitComposition()
{
    if (!composing())
        return;

    m_composition.clear();
    m_compositionIndex = 0;
    m_composingWord = false;
    emit compositionChanged();
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
    placeCursor(cursorPosition);
}

void KeyboardBuffer::placeCursor(int position)
{
    const int clamped = std::clamp(position, 0, static_cast<int>(m_text.size()));
    if (clamped == m_cursorPosition)
        return;

    m_cursorPosition = clamped;
    emit cursorPositionChanged();
}

void KeyboardBuffer::setShiftState(ShiftState state)
{
    if (state == m_shiftState)
        return;

    m_shiftState = state;
    emit shiftStateChanged();
}

QString KeyboardBuffer::composedCharacter() const
{
    const QString& character = m_composition.at(m_compositionIndex);
    return m_compositionUpperCase ? character.toUpper() : character;
}

QString KeyboardBuffer::casedWord(const QString& word) const
{
    switch (m_wordShift) {
    case ShiftState::Locked:
        return word.toUpper();
    case ShiftState::Once:
        return word.left(1).toUpper() + word.mid(1);
    case ShiftState::Off:
        break;
    }
    return word;
}
