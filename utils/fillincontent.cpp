#include "fillincontent.h"
#include "fillintextbuilder.h"

FillInContent::FillInContent(QObject* parent)
    : QObject(parent)
{
}

QString FillInContent::content() const
{
    QString sentence = buildSentence();
    QString header = buildHeader();

    if (!header.isEmpty())
        return sentence.isEmpty() ? header : header + sentence;
    return sentence.isEmpty() ? QStringLiteral("_________") : sentence;
}

QString FillInContent::buildSentence() const
{
    const bool noContext = !m_showSentence;
    const QString target = noContext ? m_entry.expression : m_entry.blankedText;
    const QString text = noContext ? m_entry.expression : m_entry.sentence;

    if (noContext && !m_interactive)
        return m_preview;
    if (text.isEmpty())
        return noContext ? m_preview : QString {};

    FillInTextBuilder builder(QStringList { target }, text);
    if (!m_preview.isEmpty())
        return builder.buildPreviewText(m_preview);
    if (!m_interactive)
        return builder.buildHighlightedText();
    if (m_userAnswer.isEmpty())
        return noContext ? m_preview : builder.buildPartialText();

    builder.setAttempt(0, m_userAnswer);
    return builder.buildPartialText();
}

QString FillInContent::buildHeader() const
{
    if (!m_showExpression || m_entry.expression.isEmpty())
        return {};
    if (!m_showSentence && m_interactive)
        return {};
    return "<p><b>" + m_entry.expression + "</b></p>";
}
